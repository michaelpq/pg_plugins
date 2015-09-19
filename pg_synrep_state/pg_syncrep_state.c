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

#include "replication/syncrep.h"
#include "storage/proc.h"
#include "utils/builtins.h"
#include "utils/pg_lsn.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(pg_syncrep_state);


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

		/* Ignore inactive entries */
		if (proc->backendId == InvalidBackendId)
			continue;

		/* Ignore pg_prepared_xacts entries */
		if (proc->pid == 0)
			continue;

		/* Ignore backends not connected to a database, like walsender */
		if (proc->databaseId == 0)
			continue;

		/* Ignore backends with unassigned role */
		if (proc->roleId == InvalidOid)
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
