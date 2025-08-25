/*-------------------------------------------------------------------------
 *
 * hint_parser.h
 *
 * Copyright (c) 1996-2025, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  hint_parser/hint_parser.h
 *
 *-------------------------------------------------------------------------
 */

#ifndef HINT_PARSER_H
#define HINT_PARSER_H

#include "nodes/pg_list.h"

/* silence -Wmissing-variable-declarations */
extern int	hint_yychar;
extern int	hint_yynerrs;

typedef struct HintConfigData
{
	char	   *name;
	char	   *contents;
} HintConfigData;

/* internal scan state of lexer */
typedef struct HintScanStateData
{
	int			depth;
} HintScanStateData;

/* Abstract type for lexer's internal state */
typedef struct HintScanStateData *HintScanState;

/* communication variables for parsing */
extern PGDLLIMPORT List *hint_parse_result;
extern PGDLLIMPORT char *hint_parse_error_msg;

/*
 * Internal functions for parsing query strings in hint_gram.y and
 * hint_scanner.l.
 */
extern int	hint_yyparse(void);
extern int	hint_yylex(void);
extern void hint_yyerror(const char *str);
extern void hint_scanner_init(const char *str);
extern void hint_scanner_finish(void);

#endif							/* HINT_PARSER_H */
