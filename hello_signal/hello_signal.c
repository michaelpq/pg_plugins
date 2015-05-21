/*-------------------------------------------------------------------------
 *
 * hello_signal.c
 *		bgworker logging message when receiving SIGHUP or SIGTERM.
 *
 * Copyright (c) 1996-2015, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		hello_signal/hello_signal.c
 *
 *-------------------------------------------------------------------------
 */

/* Some general headers for custom bgworker facility */
#include "postgres.h"
#include "fmgr.h"
#include "postmaster/bgworker.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "utils/guc.h"

/* Allow load of this module in shared libs */
PG_MODULE_MAGIC;

/* Entry point of library loading */
void _PG_init(void);

/* SIGTERM handling */
static bool got_sigterm = false;
static bool got_sighup = false;

/* The latch used for this worker to manage sleep correctly */
static Latch signalLatch;

/* Worker name */
static char *worker_name = "hello signal worker";

static void
hello_sigterm(SIGNAL_ARGS)
{
	int save_errno = errno;
	got_sigterm = true;
	SetLatch(&signalLatch);
	errno = save_errno;
}

static void
hello_sighup(SIGNAL_ARGS)
{
	int save_errno = errno;
	got_sighup = true;
	SetLatch(&signalLatch);
	errno = save_errno;
}

static void
hello_main(Datum main_arg)
{
	/*
	 * Initialize latch for this worker. Note that this initialization needs to
	 * be done absolutely before unblocking signals.
	 */
	InitializeLatchSupport();
	InitLatch(&signalLatch);

	/* Register functions for SIGTERM/SIGHUP management */
	pqsignal(SIGHUP, hello_sighup);
	pqsignal(SIGTERM, hello_sigterm);

	/* We're now ready to receive signals */
	BackgroundWorkerUnblockSignals();

	while (true)
	{
		int rc;

		/* Wait 1s */
		rc = WaitLatch(&signalLatch,
					   WL_LATCH_SET | WL_TIMEOUT | WL_POSTMASTER_DEATH,
					   1000L);
		ResetLatch(&signalLatch);

		/* Emergency bailout if postmaster has died */
		if (rc & WL_POSTMASTER_DEATH)
			proc_exit(1);

		/* Process signals */
		if (got_sighup)
		{
			/* Process config file */
			ProcessConfigFile(PGC_SIGHUP);
			got_sighup = false;
			ereport(LOG, (errmsg("hello signal: processed SIGHUP")));
		}

		if (got_sigterm)
		{
			/* Simply exit */
			ereport(LOG, (errmsg("hello signal: processed SIGTERM")));
			proc_exit(0);
		}
	}

	/* No problems, so clean exit */
	proc_exit(0);
}


void
_PG_init(void)
{
	BackgroundWorker worker;

	worker.bgw_flags = 0;
	worker.bgw_start_time = BgWorkerStart_PostmasterStart;
	worker.bgw_main = hello_main;
	snprintf(worker.bgw_name, BGW_MAXLEN, "%s", worker_name);
	/* Wait 10 seconds for restart before crash */
	worker.bgw_restart_time = 10;
	worker.bgw_main_arg = (Datum) 0;
#if PG_VERSION_NUM >= 90400
	/*
	 * Notify PID is present since 9.4. If this is not initialized
	 * a static background worker cannot start properly.
	 */
	worker.bgw_notify_pid = 0;
#endif
	RegisterBackgroundWorker(&worker);
}
