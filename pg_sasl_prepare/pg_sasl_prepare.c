/*-------------------------------------------------------------------------
 *
 * pg_sasl_prepare.c
 *		Wrapper on top of upstream implementation of SASLprep, changing
 *		a UTF-8 string into a prepared string for a SCRAM exchange.
 *
 * Copyright (c) 1996-2017, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  pg_sasl_prepare/pg_sasl_prepare.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "common/saslprep.h"
#include "fmgr.h"
#include "funcapi.h"
#include "mb/pg_wchar.h"
#include "utils/builtins.h"

PG_MODULE_MAGIC;

/*
 * pg_sasl_prepare
 *
 * Perform SASLprepare (NKFC) on a integer array identifying individual
 * multibyte UTF-8 characters. This is a simple wrapper on top of
 * PostgreSQL implementation.
 */
PG_FUNCTION_INFO_V1(pg_sasl_prepare);
Datum
pg_sasl_prepare(PG_FUNCTION_ARGS)
{
	char	   *password = text_to_cstring(PG_GETARG_TEXT_PP(0));
	char	   *prep_password = NULL;

	if (GetDatabaseEncoding() != PG_UTF8)
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("Database encoding is not UTF-8")));

	if (pg_saslprep(password, &prep_password) != SASLPREP_SUCCESS)
		ereport(ERROR,
				(errcode(ERRCODE_INTERNAL_ERROR),
				 errmsg("Error while processing SASLprep")));

	PG_RETURN_TEXT_P(cstring_to_text(prep_password));
}
