/*-------------------------------------------------------------------------
 *
 * count_relations.c
 *		Simple background worker code scanning the number of relations
 *		present in database.
 *
 * Copyright (c) 1996-2022, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		count_relations/count_relations.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

/* These are always necessary for a bgworker */
#include "miscadmin.h"
#include "pgstat.h"
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
#include "lib/stringinfo.h"
#include "utils/builtins.h"
#include "utils/snapmgr.h"

PG_MODULE_MAGIC;

void _PG_init(void);
PGDLLEXPORT void count_relations_main(Datum main_arg) pg_attribute_noreturn();

static volatile sig_atomic_t got_sigterm = false;

static void
count_relations_sigterm(SIGNAL_ARGS)
{
	int			save_errno = errno;

	got_sigterm = true;
	if (MyProc)
		SetLatch(&MyProc->procLatch);

	errno = save_errno;
}

static void
count_relations_sighup(SIGNAL_ARGS)
{
	elog(LOG, "got sighup");
	if (MyProc)
		SetLatch(&MyProc->procLatch);
}

void
count_relations_main(Datum main_arg)
{
	/* Register functions for SIGTERM/SIGHUP management */
	pqsignal(SIGHUP, count_relations_sighup);
	pqsignal(SIGTERM, count_relations_sigterm);

	/* We're now ready to receive signals */
	BackgroundWorkerUnblockSignals();

	/* Connect to our database */
	BackgroundWorkerInitializeConnection("postgres", NULL, 0);

	while (!got_sigterm)
	{
		int		ret;
		StringInfoData	buf;

		/*
		 * Background workers mustn't call usleep() or any direct equivalent:
		 * instead, they may wait on their process latch, which sleeps as
		 * necessary, but is awakened if postmaster dies.  That way the
		 * background process goes away immediately in an emergency.
		 */
		WaitLatch(&MyProc->procLatch,
				  WL_LATCH_SET | WL_TIMEOUT | WL_EXIT_ON_PM_DEATH,
				  1000L,
				  PG_WAIT_EXTENSION);
		ResetLatch(&MyProc->procLatch);

		StartTransactionCommand();
		SPI_connect();
		PushActiveSnapshot(GetTransactionSnapshot());

		initStringInfo(&buf);

		/* Build the query string */
		appendStringInfo(&buf,
						 "SELECT count(*) FROM pg_class;");

		ret = SPI_execute(buf.data, true, 0);

		/* Some error messages in case of incorrect handling */
		if (ret != SPI_OK_SELECT)
			elog(FATAL, "SPI_execute failed: error code %d", ret);

		if (SPI_processed > 0)
		{
			int32	count;
			bool	isnull;

			count = DatumGetInt32(SPI_getbinval(SPI_tuptable->vals[0],
												 SPI_tuptable->tupdesc,
												 1, &isnull));
			elog(LOG, "Currently %d relations in database",
				 count);
		}

		SPI_finish();
		PopActiveSnapshot();
		CommitTransactionCommand();
	}

	proc_exit(0);
}

/*
 * Entrypoint of this module.
 */
void
_PG_init(void)
{
	BackgroundWorker	worker;

	/* register the worker processes */
	MemSet(&worker, 0, sizeof(BackgroundWorker));
	worker.bgw_flags = BGWORKER_SHMEM_ACCESS |
		BGWORKER_BACKEND_DATABASE_CONNECTION;
	worker.bgw_start_time = BgWorkerStart_RecoveryFinished;
	snprintf(worker.bgw_library_name, BGW_MAXLEN, "count_relations");
	snprintf(worker.bgw_function_name, BGW_MAXLEN, "count_relations_main");
	snprintf(worker.bgw_name, BGW_MAXLEN, "count relations");
	worker.bgw_restart_time = BGW_NEVER_RESTART;
	worker.bgw_main_arg = (Datum) 0;
	worker.bgw_notify_pid = 0;
	RegisterBackgroundWorker(&worker);
}
