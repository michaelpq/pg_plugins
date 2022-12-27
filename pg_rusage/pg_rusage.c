/*-------------------------------------------------------------------------
 *
 * pg_rusage.c
 *		Set of functions to snapshot CPU usage.
 *
 * Copyright (c) 1996-2023, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  pg_rusage/pg_rusage.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"

#include "utils/pg_rusage.h"

PG_MODULE_MAGIC;

/* Initial snapshot */
static PGRUsage    ru0 = {0};

PG_FUNCTION_INFO_V1(pg_rusage_reset);
PG_FUNCTION_INFO_V1(pg_rusage_print);

Datum
pg_rusage_reset(PG_FUNCTION_ARGS)
{
	pg_rusage_init(&ru0);
	PG_RETURN_VOID();
}

Datum
pg_rusage_print(PG_FUNCTION_ARGS)
{
	const char	*result = pg_rusage_show(&ru0);
	elog(WARNING, "pg_rusage_print %s", result);
	PG_RETURN_VOID();
}
