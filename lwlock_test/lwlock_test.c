/*--------------------------------------------------------------------------
 *
 * lwlock_test.c
 *		Tests LWLocks with SQL functions.
 *
 * Portions Copyright (c) 1996-2024, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *		lwlock_test/lwlock_test.c
 *
 * -------------------------------------------------------------------------
 */

#include "postgres.h"

#include <signal.h>

#include "fmgr.h"
#include "miscadmin.h"
#include "storage/ipc.h"
#include "storage/lwlock.h"
#include "storage/shmem.h"

PG_MODULE_MAGIC;

/* Uncomment to get more debugging logs */
/* #define LWLOCK_TEST_DEBUG 1 */

/*
 * Global shared state
 */
typedef struct lwtSharedState
{
	LWLock		*updater;		/* LWLock used by first backend */
	LWLock		*waiter;		/* LWLock used by second backend */
	pg_atomic_uint64	updater_var;	/* Variable updated by first backend */
	pg_atomic_uint64	waiter_var;		/* Variable updated by second backend */
} lwtSharedState;

/* Links to shared memory state */
static lwtSharedState *lwt = NULL;

/* Saved hook values in case of unload */
static shmem_request_hook_type prev_shmem_request_hook = NULL;
static shmem_startup_hook_type prev_shmem_startup_hook = NULL;

/*
 * Estimate shared memory space needed.
 */
static Size
lwt_memsize(void)
{
	Size		size;

	size = MAXALIGN(sizeof(lwtSharedState));
	return size;
}

/*
 * shmem_request hook: request additional shared resources.  We'll allocate or
 * attach to the shared resources in lwlock_shmem_startup().
 */
static void
lwt_shmem_request(void)
{
	if (prev_shmem_request_hook)
		prev_shmem_request_hook();

	RequestAddinShmemSpace(lwt_memsize());
	RequestNamedLWLockTranche("lwlock_test", 2);
}

/*
 * shmem_startup hook: allocate or attach to shared memory.
 */
static void
lwt_shmem_startup(void)
{
	bool		found = false;

	if (prev_shmem_startup_hook)
		prev_shmem_startup_hook();

	/* reset in case of a restart within the postmaster */
	lwt = NULL;

	/*
	 * Create or attach to the shared memory state, including hash table
	 */
	LWLockAcquire(AddinShmemInitLock, LW_EXCLUSIVE);

	lwt = ShmemInitStruct("lwlock_test",
						  sizeof(lwtSharedState),
						  &found);

	if (!found)
	{
		/* first time through */
		LWLockPadded *locks = GetNamedLWLockTranche("lwlock_test");

		lwt->updater = &(locks[0].lock);
		lwt->waiter = &(locks[1].lock);
		pg_atomic_init_u64(&lwt->updater_var, 0);
		pg_atomic_init_u64(&lwt->waiter_var, 0);
	}

	LWLockRelease(AddinShmemInitLock);
}

/*
 * Module load callback
 */
void
_PG_init(void)
{
	if (!process_shared_preload_libraries_in_progress)
		ereport(ERROR,
				(errmsg("cannot load \"%s\" after startup", "lwlock_test"),
				 errdetail("\"%s\" must be loaded with shared_preload_libraries.",
						   "lwlock_test")));

	prev_shmem_request_hook = shmem_request_hook;
	shmem_request_hook = lwt_shmem_request;
	prev_shmem_startup_hook = shmem_startup_hook;
	shmem_startup_hook = lwt_shmem_startup;
}

/*
 * SQL functions to check behavior of LWLock routines looking at updater_var
 * changes.  This should be used as follows, with two backends:
 * 1: SELECT lwlock_test_acquire();
 * 2: SELECT lwlock_test_wait(N);
 * 1: SELECT lwlock_test_update(N);
 * 1: SELECT lwlock_test_release(N);
 */

PG_FUNCTION_INFO_V1(lwlock_test_acquire);
PG_FUNCTION_INFO_V1(lwlock_test_release);
PG_FUNCTION_INFO_V1(lwlock_test_update);
PG_FUNCTION_INFO_V1(lwlock_test_wait);

Datum
lwlock_test_acquire(PG_FUNCTION_ARGS)
{
	if (lwt == NULL)
		ereport(ERROR,
				(errmsg("cannot use \"%s\" if \"%s\" has not been loaded with shared_preload_libraries",
						"lwlock_test_acquire", "lwlock_test")));
	LWLockAcquire(lwt->updater, LW_EXCLUSIVE);
	PG_RETURN_VOID();
}

Datum
lwlock_test_release(PG_FUNCTION_ARGS)
{
	if (lwt == NULL)
		ereport(ERROR,
				(errmsg("cannot use \"%s\" if \"%s\" has not been loaded with shared_preload_libraries",
						"lwlock_test_release", "lwlock_test")));
	LWLockReleaseClearVar(lwt->updater, &lwt->updater_var, 0);
	PG_RETURN_VOID();
}

/*
 * lwlock_test_update
 *
 * Send an update and wait from a reply from the waiter, looping N
 * times.
 */
Datum
lwlock_test_update(PG_FUNCTION_ARGS)
{
	int		loops = PG_GETARG_INT32(0);
	int		count = 0;
	uint64	oldval = 0;
	uint64	newval = 0;

	if (lwt == NULL)
		ereport(ERROR,
				(errmsg("cannot use \"%s\" if \"%s\" has not been loaded with shared_preload_libraries",
						"lwlock_test_update", "lwlock_test")));

	for (count = 0; count < loops; count++)
	{
		uint64	updater_var;

		/* increment updater_var by 1 */
#ifdef LWLOCK_TEST_DEBUG
		elog(WARNING, "lwlock_test_update: updating updater_var");
#endif
		updater_var = pg_atomic_read_u64(&lwt->updater_var) + 1;
		LWLockUpdateVar(lwt->updater, &lwt->updater_var,
						updater_var);

		/* now make sure that the waiter has received the update */
		if (LWLockWaitForVar(lwt->waiter,
							 &lwt->waiter_var,
							 oldval, &newval))
		{
			/* the lock was free, so nothing is waiting */
#ifdef LWLOCK_TEST_DEBUG
			elog(WARNING, "lwlock_test_update: lock free, so leaving");
#endif
			break;
		}

		if (oldval != newval)
		{
#ifdef LWLOCK_TEST_DEBUG
			elog(WARNING, "lwlock_test_update: update received from waiter");
#endif
			oldval = newval;
		}
	}

	PG_RETURN_VOID();
}

/*
 * lwlock_test_wait()
 *
 * Wait for variable changes coming from the updater.  This waits first
 * for the updater, then it changes its own variable, looping N times
 * while communicating with the updater.
 */
Datum
lwlock_test_wait(PG_FUNCTION_ARGS)
{
	int		waits_to_do = PG_GETARG_INT32(0);
	uint64	oldval = 0;
	uint64	newval = 0;
	int		updates_done = 0;

	if (lwt == NULL)
		ereport(ERROR,
				(errmsg("cannot use \"%s\" if \"%s\" has not been loaded with shared_preload_libraries",
						"lwlock_test_wait", "lwlock_test")));

	LWLockAcquire(lwt->waiter, LW_EXCLUSIVE);

	while (true)
	{
		uint64	waiter_var;

#ifdef LWLOCK_TEST_DEBUG
		elog(WARNING, "lwlock_test_wait: beginning");
#endif
		if (LWLockWaitForVar(lwt->updater,
							 &lwt->updater_var,
							 oldval, &newval))
		{
			/* the lock was free, so nothing is in progress, just leave */
#ifdef LWLOCK_TEST_DEBUG
			elog(WARNING, "lwlock_test_wait: free lock, so leaving");
#endif
			break;
		}

#ifdef LWLOCK_TEST_DEBUG
		elog(WARNING, "lwlock_test_wait: finished one wait");
#endif

		if (oldval != newval)
		{
			/* An update has happened, so refresh the count */
			oldval = newval;
			updates_done++;

#ifdef LWLOCK_TEST_DEBUG
			elog(WARNING, "lwlock_test_wait: update detected, now at %d", updates_done);
#endif
		}

		/* increment waiter_var by 1 */
#ifdef LWLOCK_TEST_DEBUG
		elog(WARNING, "lwlock_test_wait: updating waiter_var");
#endif
		waiter_var = pg_atomic_read_u64(&lwt->waiter_var) + 1;
		LWLockUpdateVar(lwt->waiter, &lwt->waiter_var,
						waiter_var);

		if (updates_done >= waits_to_do)
			break;
	}

	LWLockReleaseClearVar(lwt->waiter, &lwt->waiter_var, 0);

	PG_RETURN_INT32(updates_done);
}
