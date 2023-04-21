/*-------------------------------------------------------------------------
 *
 * hint_parser.c
 *		Set of functions to parse query hints from query strings.
 *
 * Copyright (c) 1996-2023, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  hint_parser/hint_parser.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "hint_parser.h"

#include "fmgr.h"
#include "utils/builtins.h"

PG_MODULE_MAGIC;

/*
 * This is the hint_parser function.
 */
PG_FUNCTION_INFO_V1(hint_parser);

Datum
hint_parser(PG_FUNCTION_ARGS)
{
	char	   *query = text_to_cstring(PG_GETARG_TEXT_PP(0));
	int			parse_rc;
	ListCell   *item;
	List	   *hints = NIL;

	hint_parse_result = NULL;
	hint_parse_error_msg = NULL;

	/* Parse the query */
	hint_scanner_init(query);
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

	/* Grab and print all the items parsed */
	foreach(item, hints)
	{
		HintConfigData *hint = lfirst(item);

		elog(WARNING, "Hint found: name %s contents %s",
			 hint->name, hint->contents);
	}

	PG_RETURN_VOID();
}
