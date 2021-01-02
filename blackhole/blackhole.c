/*-------------------------------------------------------------------------
 *
 * blackhole.c
 *		Set of functions for a minimal extension template
 *
 * Copyright (c) 1996-2021, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  blackhole/blackhole.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"

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
