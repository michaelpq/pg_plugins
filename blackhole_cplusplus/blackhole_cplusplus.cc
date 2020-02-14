/*-------------------------------------------------------------------------
 *
 * blackhole_cplusplus.cc
 *		Set of functions for a minimal C++ extension template
 *
 * Copyright (c) 1996-2020, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  blackhole_cplusplus/blackhole_cplusplus.cc
 *
 *-------------------------------------------------------------------------
 */

#ifdef __cplusplus
extern "C" {
#endif
#include "postgres.h"
#include "fmgr.h"

PG_MODULE_MAGIC;

/*
 * This is the blackhole function.
 */
PG_FUNCTION_INFO_V1(blackhole_cplusplus);

Datum
blackhole_cplusplus(PG_FUNCTION_ARGS)
{
	PG_RETURN_NULL();
}

#ifdef __cplusplus
}
#endif

/* This space can be used for C++ code */
