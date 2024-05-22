/*-------------------------------------------------------------------------
 *
 * blackhole.c
 *		Set of functions for a minimal extension template
 *
 * Copyright (c) 1996-2024, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  blackhole/blackhole.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "fmgr.h"
#include "funcapi.h"
#include "utils/builtins.h"

PG_MODULE_MAGIC;

/*
 * This is the blackhole function.
 */
PG_FUNCTION_INFO_V1(blackhole);

Datum
blackhole(PG_FUNCTION_ARGS)
{
	PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(blackhole_tuplestore);

/*
 * Example of SQL function returning a set of records with a tuplestore.
 */
Datum
blackhole_tuplestore(PG_FUNCTION_ARGS)
{
	int		num = PG_GETARG_INT32(0);
	ReturnSetInfo *rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;

	InitMaterializedSRF(fcinfo, 0);

	for (int count = 0; count < num; count++)
	{
		Datum		values[2];
		bool		nulls[2];
		char		buf[32];

		values[0] = Int32GetDatum(count);
		snprintf(buf, 32, "data %d", count);
		values[1] = CStringGetTextDatum(buf);

		memset(nulls, 0, sizeof(nulls));

		tuplestore_putvalues(rsinfo->setResult, rsinfo->setDesc, values, nulls);
	}

	return (Datum) 0;
}

PG_FUNCTION_INFO_V1(blackhole_value_per_call);

Datum
blackhole_value_per_call(PG_FUNCTION_ARGS)
{
	int		num = PG_GETARG_INT32(0);
	FuncCallContext *funcctx;

	/* First call of this function */
	if (SRF_IS_FIRSTCALL())
	{
		MemoryContext oldcxt;
		TupleDesc	tupdesc;

		/* create a function context for cross-call persistence */
		funcctx = SRF_FIRSTCALL_INIT();

		/* switch to memory context appropriate for multiple function calls */
		oldcxt = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

		if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
			elog(ERROR, "return type must be a row type");
		funcctx->tuple_desc = tupdesc;

		/* No state required for funcctx->user_fctx */
		MemoryContextSwitchTo(oldcxt);
	}

	/* stuff done on every call of the function */
	funcctx = SRF_PERCALL_SETUP();

	/* Number of tuples returned is restricted by the number in input */
	if (funcctx->call_cntr < num)
	{
		Datum		values[2];
		bool		nulls[2];
		char		buf[32];
		Datum		result;
		HeapTuple	tuple;

		values[0] = Int32GetDatum(funcctx->call_cntr);
		snprintf(buf, 32, "data %lld", (long long) funcctx->call_cntr);
		values[1] = CStringGetTextDatum(buf);

		tuple = heap_form_tuple(funcctx->tuple_desc, values, nulls);
		result = HeapTupleGetDatum(tuple);
		SRF_RETURN_NEXT(funcctx, result);
	}

	/* done when there are no more elements left */
	SRF_RETURN_DONE(funcctx);
}
