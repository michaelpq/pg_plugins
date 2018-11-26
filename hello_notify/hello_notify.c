/*-------------------------------------------------------------------------
 *
 * hello_notify.c
 *		Notify backends running queries taking a too long amount of
 *		time to execute. This facility could be used as a base for
 *		servers that want to notify queries taking a too long amount
 *		of time to execute.
 *
 * Copyright (c) 1996-2018, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		hello_notify/hello_notify.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "miscadmin.h"
#include "pgstat.h"
#include "access/xact.h"
#include "commands/async.h"
#include "executor/spi.h"
#include "postmaster/bgworker.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/lwlock.h"
#include "storage/proc.h"
#include "storage/shmem.h"
#include "tcop/utility.h"
#include "utils/snapmgr.h"

PG_MODULE_MAGIC;

void _PG_init(void);
void hello_notify_main(Datum main_arg) pg_attribute_noreturn();

/* Worker name */
static const char *hello_notify_name = "hello_notify";

/* flags set by signal handlers */
static volatile sig_atomic_t got_sigterm = false;
static volatile sig_atomic_t got_sighup = false;

/* GUC variables */
static int notify_nap_time = 60;
static char *notify_database = NULL;
static char *notify_channel = NULL;

/*
 * hello_notify_sigterm
 *
 * SIGTERM handler
 */
static void
hello_notify_sigterm(SIGNAL_ARGS)
{
	int save_errno = errno;

	got_sigterm = true;
	if (MyProc)
		SetLatch(&MyProc->procLatch);

	errno = save_errno;
}

/*
 * hello_notify_sighup
 *
 * SIGHUP handler
 */
static void
hello_notify_sighup(SIGNAL_ARGS)
{
	got_sighup = true;
	if (MyProc)
		SetLatch(&MyProc->procLatch);
}

/*
 * hello_notify_build_query
 *
 * Build query to send notify request. Buffer used here should be
 * initialized properly before calling this function.
 */
static void
hello_notify_build_query(StringInfoData *buf)
{
	/* Leave if there is nothing here */
	if (buf == NULL)
		return;

	/*
	 * Build query depending on nap time and channel name. A notification
	 * is only sent to queries running for longer than naptime.
	 */
	appendStringInfo(buf, "\
		SELECT pg_notify('%s', row_to_json(q)::text)\
		FROM ( \
			SELECT datname AS database,\
				usename AS username,\
				state,\
				TRIM(query) AS query\
			FROM pg_stat_activity\
			WHERE xact_start < NOW() - interval '%d s' AND\
				state = 'active' AND\
				pid != pg_backend_pid()\
		) AS q", notify_channel, notify_nap_time);
}

/*
 * hello_notify_main
 *
 * Main loop processing notify requests.
 */
void
hello_notify_main(Datum main_arg)
{
	StringInfoData buf;

	/* Register functions for SIGTERM/SIGHUP management */
	pqsignal(SIGHUP, hello_notify_sighup);
	pqsignal(SIGTERM, hello_notify_sigterm);

	/* We're now ready to receive signals */
	BackgroundWorkerUnblockSignals();

	/* Connect to database */
	BackgroundWorkerInitializeConnection(notify_database, NULL, 0);

	/* Build the query string */
	initStringInfo(&buf);
	hello_notify_build_query(&buf);

	elog(LOG, "hello_notify: Started on db %s with interval %d seconds",
			 notify_database, notify_nap_time);

	/* Main processing loop */
	while (!got_sigterm)
	{
		int	ret;
		bool process_notifies;

		/* Take a nap... */
		WaitLatch(&MyProc->procLatch,
				  WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH,
				  notify_nap_time * 1000,
				  PG_WAIT_EXTENSION);
		ResetLatch(&MyProc->procLatch);

		/* Handle signal SIGHUP */
		if (got_sighup)
		{
			elog(LOG, "bgworker hello_notify: processing SIGHUP");
			got_sighup = false;
			ProcessConfigFile(PGC_SIGHUP);

			/* Rebuild NOTIFY query, perhaps some parameters have changed */
			hello_notify_build_query(&buf);
		}

		/*
		 * Handle signal SIGTERM if it has come up after entering this
		 * loop.
		 */
		if (got_sigterm)
		{
			/* Simply exit */
			elog(LOG, "hello signal: processed SIGTERM, leaving...");
			proc_exit(0);
		}

		/* Update NOW() to return correct timestamp */
		SetCurrentStatementStartTimestamp();

		/* Show query status in pg_stat_activity */
		pgstat_report_activity(STATE_RUNNING, "hello_notify");
		StartTransactionCommand();
		SPI_connect();
		PushActiveSnapshot(GetTransactionSnapshot());

		/* Execute NOTIFY requests */
		ret = SPI_execute(buf.data, false, 0);
		if (ret != SPI_OK_SELECT)
			elog(FATAL, "hello_notify: SPI_execute failed with error code %d", ret);
		process_notifies = SPI_processed > 0;
		elog(LOG, "hello_notify: executed " UINT64_FORMAT, SPI_processed);

		/* Terminate transaction */
		SPI_finish();
		PopActiveSnapshot();
		CommitTransactionCommand();

		/*
		 * Send out notifications. This is mandatory after previous
		 * transaction has committed.
		 */
		if (process_notifies)
			ProcessCompletedNotifies();
		pgstat_report_activity(STATE_IDLE, NULL);
	}

	elog(LOG, "hello_notify: finished");
	proc_exit(0);
}

static void
hello_notify_load_params(void)
{
	/*
	 * Defines database where to connect and send the NOTIFY requests,
	 * needs to remain constant after worker startup.
	 */
	DefineCustomStringVariable("hello_notify.database",
	                           "Database where NOTIFY is sent.",
	                           "Default value is \"postgres\".",
	                           &notify_database,
	                           "postgres",
	                           PGC_POSTMASTER,
	                           0, NULL, NULL, NULL);

	/* Channel name used for NOTIFY messages */
	DefineCustomStringVariable("hello_notify.channel_name",
	                           "Channel name of NOTIFY requests.",
	                           "Default value is the worker name.",
	                           &notify_channel,
	                           hello_notify_name,
	                           PGC_SIGHUP,
	                           0, NULL, NULL, NULL);

	/* Nap time between two updates */
	DefineCustomIntVariable("hello_notify.nap_time",
							"Nap time between two successive updates (seconds)",
							"Default value set to 60 seconds).",
							&notify_nap_time,
							60, 1, 3600,
							PGC_SIGHUP,
							0, NULL, NULL, NULL);
}

/*
 * Entry point for worker loading
 */
void
_PG_init(void)
{
	BackgroundWorker worker;

	/* Load parameters */
	hello_notify_load_params();

	/* Register this worker */
	MemSet(&worker, 0, sizeof(BackgroundWorker));
	snprintf(worker.bgw_library_name, BGW_MAXLEN, "hello_notify");
	snprintf(worker.bgw_function_name, BGW_MAXLEN, "hello_notify_main");
	snprintf(worker.bgw_name, BGW_MAXLEN, "%s", hello_notify_name);
	worker.bgw_flags = BGWORKER_SHMEM_ACCESS |
		BGWORKER_BACKEND_DATABASE_CONNECTION;
	worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
	worker.bgw_restart_time = 10;
	worker.bgw_main_arg = (Datum) 0;
	worker.bgw_notify_pid = 0;
	RegisterBackgroundWorker(&worker);
}
