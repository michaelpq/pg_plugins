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

	StaticAssertStmt(true, "static assert test");
	StaticAssertExpr(true, "expression assert test");
}

/* declaration out of file scope */
StaticAssertDecl(true, "declaration assert test");

#ifdef __cplusplus
}
#endif
