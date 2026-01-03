/*-------------------------------------------------------------------------
 *
 * hint_parser.c
 *		Set of functions to parse query hints from query strings.
 *
 * Copyright (c) 1996-2026, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  hint_parser/hint_parser.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "hint_parser.h"

#include "fmgr.h"
#include "funcapi.h"
#include "utils/builtins.h"

PG_MODULE_MAGIC;

/*
 * This is the hint_parser function.
 */
PG_FUNCTION_INFO_V1(hint_parser);

/*
 * hint_parser
 *
 * Parse and return as a SRF all the hints found in a given string.  The
 * content given in input should be extracted from a query.
 */
Datum
hint_parser(PG_FUNCTION_ARGS)
{
	char	   *string = text_to_cstring(PG_GETARG_TEXT_PP(0));
	int			parse_rc;
	ListCell   *item;
	List	   *hints = NIL;
	ReturnSetInfo *rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;

	InitMaterializedSRF(fcinfo, 0);

	/* Results from the parsing */
	hint_parse_result = NULL;
	hint_parse_error_msg = NULL;

	/* Parse the query */
	hint_scanner_init(string);
	parse_rc = hint_yyparse();
	hint_scanner_finish();

	if (parse_rc != 0 || hint_parse_result == NULL)
	{
		if (hint_parse_error_msg)
			elog(ERROR, "%s", hint_parse_error_msg);
		else
			elog(ERROR, "hint parser failed");
	}

	hints = hint_parse_result;

	/* Grab and store all the hint items */
	foreach(item, hints)
	{
		HintConfigData *hint = lfirst(item);
		Datum		values[2];
		bool		nulls[2];

		values[0] = CStringGetTextDatum(hint->name);
		values[1] = CStringGetTextDatum(hint->contents);
		memset(nulls, 0, sizeof(nulls));

		tuplestore_putvalues(rsinfo->setResult, rsinfo->setDesc, values, nulls);
	}

	return (Datum) 0;
}
