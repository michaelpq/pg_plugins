#include "postgres.h"

/* These are always necessary for a bgworker */
#include "miscadmin.h"
#include "postmaster/bgworker.h"
#include "storage/ipc.h"
#include "storage/latch.h"
#include "storage/lwlock.h"
#include "storage/proc.h"
#include "storage/shmem.h"

/* these headers are used by this particular worker's code */
#include "access/xact.h"
#include "executor/spi.h"
#include "fmgr.h"
#include "pgstat.h"
#include "lib/stringinfo.h"
#include "utils/builtins.h"
#include "utils/snapmgr.h"
#include "tcop/utility.h"
#include "commands/async.h"

#define LOG_INFO(...) elog(LOG, __VA_ARGS__)

PG_MODULE_MAGIC;

void _PG_init(void);

/* flags set by signal handlers */
static bool got_sigterm = false;
static bool got_sighup = false;

/* GUC variables */
static int config_wait_time = 60;
static char * config_database = NULL;

static void worker_spi_sigterm(SIGNAL_ARGS)
{
	int save_errno = errno;

	got_sigterm = true;
	if (MyProc)
		SetLatch(&MyProc->procLatch);

	errno = save_errno;
}

static void worker_spi_sighup(SIGNAL_ARGS)
{
	got_sighup = true;
	if (MyProc)
		SetLatch(&MyProc->procLatch);
}

static void worker_spi_main(Datum main_arg)
{
	StringInfoData buf;

	// Register functions for SIGTERM/SIGHUP management
	pqsignal(SIGHUP, worker_spi_sighup);
	pqsignal(SIGTERM, worker_spi_sigterm);

	// We're now ready to receive signals
	BackgroundWorkerUnblockSignals();

	// Connect to database
	BackgroundWorkerInitializeConnection(config_database, NULL);

	initStringInfo(&buf);
	// Build the query string
	appendStringInfo(&buf, "\
		SELECT pg_notify('hello_notify', row_to_json(q)::text)\
		FROM (\
			SELECT datname AS database, usename AS username, state, TRIM(query) AS query\
			FROM pg_stat_activity\
			WHERE query_start > NOW() - interval '1 minutes'\
		) AS q\
	");

	LOG_INFO("hello_notify: started on db %s with interval %d seconds", config_database, config_wait_time);

	while (!got_sigterm)
	{
		int	ret, rc;
		bool process_notifies;

		rc = WaitLatch(&MyProc->procLatch, WL_LATCH_SET | WL_TIMEOUT | WL_POSTMASTER_DEATH,
			(long)config_wait_time * 1000); // Wait x seconds
		ResetLatch(&MyProc->procLatch);

		// emergency bailout if postmaster has died
		if (rc & WL_POSTMASTER_DEATH) {
			LOG_INFO("hello_notify: WL_POSTMASTER_DEATH");
			proc_exit(1);
		}

		// handle signals
		if (got_sigterm) {
			LOG_INFO("hello_notify: SIGTERM");
			continue;
		}

		if (got_sighup) {
			LOG_INFO("hello_notify: SIGHUP");
			got_sighup = false;
			ProcessConfigFile(PGC_SIGHUP);
		}

		// Update NOW() to return correct timestamp
		SetCurrentStatementStartTimestamp();
		// Show query in pgstat
		pgstat_report_activity(STATE_RUNNING, "hello_notify");
		StartTransactionCommand();
		SPI_connect();
		PushActiveSnapshot(GetTransactionSnapshot());

		ret = SPI_execute(buf.data, false, 0);
		if (ret != SPI_OK_UPDATE)
			elog(FATAL, "hello_notify: SPI_execute failed with error code %d", ret);

		process_notifies = SPI_processed > 0;
		LOG_INFO("hello_notify: executed %d", SPI_processed);

		SPI_finish();
		PopActiveSnapshot();
		CommitTransactionCommand();
		// Send out notifications if necessary
		if (process_notifies)
			ProcessCompletedNotifies();
		pgstat_report_activity(STATE_IDLE, NULL);
	}

	LOG_INFO("hello_notify: finished");
	proc_exit(0);
}

void _PG_init(void)
{
	BackgroundWorker worker;

	DefineCustomStringVariable("hello_notify.database",
	                           "Database to connect",
	                           "Database to connet (default: postgres).",
	                           &config_database,
	                           "postgres",
	                           PGC_POSTMASTER,
	                           0, NULL, NULL, NULL);

	DefineCustomIntVariable(   "hello_notify.wait_seconds",
	                           "Time waited between updates",
	                           "Time waited between updates (default: 60 seconds).",
	                           &config_wait_time,
	                           60, 5, 3600,
	                           PGC_POSTMASTER,
	                           0, NULL, NULL, NULL);

	snprintf(worker.bgw_name, BGW_MAXLEN, "hello_notify");
	worker.bgw_flags = BGWORKER_SHMEM_ACCESS | BGWORKER_BACKEND_DATABASE_CONNECTION;
	worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
	worker.bgw_main = worker_spi_main;
	worker.bgw_restart_time = 10; /* interval, in seconds, that postgres should wait before restarting the process, in case it crashes or BGW_NEVER_RESTART */
	worker.bgw_main_arg = (Datum) 0;
	RegisterBackgroundWorker(&worker);
}

