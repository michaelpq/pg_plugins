/*
 * hello_world.c
 *		bgworker logging hello world on server
 *
 * Copyright (c) 2013, Michael Paquier
 * Portions Copyright (c) 1996-2013, PostgreSQL Global Development Group
 */

/* Minimum set of headers */
#include "postgres.h"
#include "postmaster/bgworker.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/proc.h"
#include "fmgr.h"

/* Essential for shared libs! */
PG_MODULE_MAGIC;

/* Entry point of library loading */
void _PG_init(void);

/* Signal handling */
static bool got_sigterm = false;

/*
 * hello_sigterm
 *
 * SIGTERM handler.
 */
static void
hello_sigterm(SIGNAL_ARGS)
{
	int save_errno = errno;
	got_sigterm = true;
	if (MyProc)
		SetLatch(&MyProc->procLatch);
	errno = save_errno;
}

/*
 * hello_main
 *
 * Main loop processing.
 */
static void
hello_main(void *main_arg)
{
	/* We're now ready to receive signals */
	BackgroundWorkerUnblockSignals();
	while (!got_sigterm)
	{
		int rc;

		/* Wait 10s */
		rc = WaitLatch(&MyProc->procLatch,
					   WL_LATCH_SET | WL_TIMEOUT | WL_POSTMASTER_DEATH,
					   10000L);
		ResetLatch(&MyProc->procLatch);

		/* Emergency bailout if postmaster has died */
		if (rc & WL_POSTMASTER_DEATH)
			proc_exit(1);

		elog(LOG, "Hello World!"); /* Say Hello to the world */
	}
	proc_exit(0);
}

/*
 * _PG_init
 *
 * Load point of library.
 */
void
_PG_init(void)
{
	BackgroundWorker worker;

	/* Register the worker processes */
	worker.bgw_flags = BGWORKER_SHMEM_ACCESS;
	worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
	worker.bgw_main = hello_main;
	worker.bgw_sighup = NULL;
	worker.bgw_sigterm = hello_sigterm;
	worker.bgw_name = "hello world";
	worker.bgw_restart_time = BGW_NEVER_RESTART;
	worker.bgw_main_arg = NULL;
	RegisterBackgroundWorker(&worker);
}
