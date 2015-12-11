/*-------------------------------------------------------------------------
 *
 * pg_syncrep_state.c
 *		Fetch backend status regarding synchronous replication.
 *
 * Copyright (c) 1996-2015, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  pg_syncrep_state/pg_syncrep_state.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "funcapi.h"
#include "miscadmin.h"

#include "access/htup_details.h"
#include "catalog/pg_type.h"
#include "replication/syncrep.h"
#include "replication/walreceiver.h"
#include "storage/proc.h"
#include "utils/builtins.h"
#include "utils/pg_lsn.h"
#include "utils/timestamp.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(pg_syncrep_state);
PG_FUNCTION_INFO_V1(pg_wal_receiver_state);


/*
 * List backend status regarding synchronous replication
 */
Datum
pg_syncrep_state(PG_FUNCTION_ARGS)
{
	TupleDesc	tupdesc;
	Tuplestorestate *tupstore;
	ReturnSetInfo *rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;
	MemoryContext per_query_ctx;
	MemoryContext oldcontext;
	int i;

	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 (errmsg("must be superuser to fetch synchronous replication state"))));

	/* check to see if caller supports us returning a tuplestore */
	if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("set-valued function called in context that cannot accept a set")));
	if (!(rsinfo->allowedModes & SFRM_Materialize))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("materialize mode required, but it is not " \
						"allowed in this context")));

	per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
	oldcontext = MemoryContextSwitchTo(per_query_ctx);

	/* Build tuple descriptor */
	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		elog(ERROR, "return type must be a row type");

	tupstore = tuplestore_begin_heap(true, false, work_mem);
	rsinfo->returnMode = SFRM_Materialize;
	rsinfo->setResult = tupstore;
	rsinfo->setDesc = tupdesc;
	MemoryContextSwitchTo(oldcontext);

	LWLockAcquire(SyncRepLock, LW_SHARED);
	for (i = 0; i <= ProcGlobal->allProcCount; i++)
	{
		Datum		values[3];
		bool		nulls[3];
		volatile PGPROC *proc = &ProcGlobal->allProcs[i];

		/* Ignore deleted entries */
#if PG_VERSION_NUM >= 90600
		if (proc->pgprocno == -1 || proc->pgprocno == INVALID_PGPROCNO)
#else
		if (proc->pgprocno == -1)
#endif
			continue;

		/* Ignore inactive entries */
		if (proc->backendId == InvalidBackendId)
			continue;

		/* Ignore pg_prepared_xacts entries */
		if (proc->pid == 0)
			continue;

		/* Ignore backends not connected to a database, like walsender */
		if (!OidIsValid(proc->databaseId))
			continue;

		/* Ignore backends with unassigned role */
		if (proc->roleId == InvalidOid)
			continue;

		/* Check if process really exists */
		if (kill(proc->pid, 0) != 0)
			continue;

		/* Initialize values and NULL flags arrays */
		MemSet(values, 0, sizeof(values));
		MemSet(nulls, 0, sizeof(nulls));

		/* Fill in values */
		values[0] = Int32GetDatum(proc->pid);
		if (proc->syncRepState == SYNC_REP_NOT_WAITING)
			values[1] = CStringGetTextDatum("not waiting");
		else if (proc->syncRepState == SYNC_REP_WAITING)
			values[1] = CStringGetTextDatum("waiting");
		else if (proc->syncRepState == SYNC_REP_WAIT_COMPLETE)
			values[1] = CStringGetTextDatum("wait complete");
		else
			Assert(false); /* should not happen */

		if (XLogRecPtrIsInvalid(proc->waitLSN))
			nulls[2] = true;
		else
			values[2] = LSNGetDatum(proc->waitLSN);

		tuplestore_putvalues(tupstore, tupdesc, values, nulls);
	}
	LWLockRelease(SyncRepLock);

	/* clean up and return the tuplestore */
	tuplestore_donestoring(tupstore);

	return (Datum) 0;
}

/*
 * Fetch WAL receiver state if any present.
 */
Datum
pg_wal_receiver_state(PG_FUNCTION_ARGS)
{
	TupleDesc	tupdesc;
	Datum		values[12];
	bool		nulls[12];
	WalRcvData *walrcv = WalRcv;

	SpinLockAcquire(&walrcv->mutex);

	if (walrcv->pid == 0)
	{
		SpinLockRelease(&walrcv->mutex);
		PG_RETURN_NULL();
	}

    /* Initialise values and NULL flags arrays */
	MemSet(values, 0, sizeof(values));
	MemSet(nulls, 0, sizeof(nulls));

	/* Initialise attributes information in the tuple descriptor */
	tupdesc = CreateTemplateTupleDesc(7, false);
	TupleDescInitEntry(tupdesc, (AttrNumber) 1, "pid",
					   INT4OID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 2, "status",
					   TEXTOID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 3, "receive_start_lsn",
					   LSNOID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 4, "receive_start_tli",
					   INT4OID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 5, "received_up_to_lsn",
					   LSNOID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 6, "received_tli",
					   INT4OID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 7, "latest_chunk_start_lsn",
					   LSNOID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 8, "last_msg_send_time",
					   TIMESTAMPTZOID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 9, "last_msg_receipt_time",
					   TIMESTAMPTZOID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 10, "latest_end_lsn",
					   LSNOID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 11, "latest_end_time",
					   TIMESTAMPTZOID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 12, "slot_name",
					   TEXTOID, -1, 0);

	BlessTupleDesc(tupdesc);

	/* Fetch values */
	values[0] = Int32GetDatum(walrcv->pid);

	switch (walrcv->walRcvState)
	{
		case WALRCV_STOPPING:
			values[1] = CStringGetTextDatum("stopping");
			break;
		case WALRCV_STOPPED:
			values[1] = CStringGetTextDatum("stopped");
			break;
		case WALRCV_STARTING:
			values[1] = CStringGetTextDatum("starting");
			break;
		case WALRCV_WAITING:
			values[1] = CStringGetTextDatum("waiting");
			break;
		case WALRCV_STREAMING:
			values[1] = CStringGetTextDatum("streaming");
			break;
		case WALRCV_RESTARTING:
			values[1] = CStringGetTextDatum("restarting");
			break;
		default:
			Assert(0);
	}

	if (XLogRecPtrIsInvalid(walrcv->receiveStart))
		nulls[2] = true;
	else
		values[2] = LSNGetDatum(walrcv->receiveStart);
	values[3] = Int32GetDatum(walrcv->receiveStartTLI);
	if (XLogRecPtrIsInvalid(walrcv->receivedUpto))
		nulls[4] = true;
	else
		values[4] = LSNGetDatum(walrcv->receivedUpto);
	values[5] = Int32GetDatum(walrcv->receivedTLI);
	if (XLogRecPtrIsInvalid(walrcv->latestChunkStart))
		nulls[6] = true;
	else
		values[6] = LSNGetDatum(walrcv->latestChunkStart);
	if (walrcv->lastMsgSendTime == 0)
		nulls[7] = true;
	else
		values[7] = TimestampTzGetDatum(walrcv->lastMsgSendTime);
	if (walrcv->lastMsgReceiptTime == 0)
		nulls[8] = true;
	else
		values[8] = TimestampTzGetDatum(walrcv->lastMsgReceiptTime);
	if (XLogRecPtrIsInvalid(walrcv->latestWalEnd))
		nulls[9] = true;
	else
		values[9] = LSNGetDatum(walrcv->latestWalEnd);
	if (walrcv->latestWalEndTime == 0)
		nulls[10] = true;
	else
		values[10] = TimestampTzGetDatum(walrcv->latestWalEndTime);
	if (*(walrcv->slotname) == '\0')
		nulls[11] = true;
	else
		values[11] = CStringGetTextDatum(walrcv->slotname);

	SpinLockRelease(&walrcv->mutex);

	/* Returns the record as Datum */
	PG_RETURN_DATUM(HeapTupleGetDatum(
						  heap_form_tuple(tupdesc, values, nulls)));
}
