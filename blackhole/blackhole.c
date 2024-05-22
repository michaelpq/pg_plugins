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

PG_FUNCTION_INFO_V1(blackhole_srf_tuplestore);

/*
 * Example of SQL function returning a set of records with a tuplestore.
 */
Datum
blackhole_srf_tuplestore(PG_FUNCTION_ARGS)
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
