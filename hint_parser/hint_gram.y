%{
/*-------------------------------------------------------------------------
 *
 * hint_gram.y				- Parser for query hints
 *
 * Portions Copyright (c) 1996-2026, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  src/backend/replication/hint_gram.y
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "nodes/pg_list.h"
#include "hint_parser.h"

/* Result of parsing is returned in one of these two variables */
List	   *hint_parse_result;	/* list of hints */
char	   *hint_parse_error_msg;

static HintConfigData *create_hint_item(const char *hint_name,
										const char *hint_contents);

/*
 * Bison doesn't allocate anything that needs to live across parser calls,
 * so we can easily have it use palloc instead of malloc.  This prevents
 * memory leaks if we error out during parsing.
 */
#define YYMALLOC palloc
#define YYFREE   pfree

%}

%expect 0
%name-prefix="hint_yy"

%union
{
	char	   *str;
	List	   *list;		/* should be a list of HintConfigData */
	HintConfigData *item;
}

%token <str> NAME

%type <list> hint_list

%type <str> hint_name hint_content
%type <item> hint_item
%type <list> result

%start result

%%
result:
		hint_list				{ hint_parse_result = $1; }
	;

hint_list:
		hint_item					{ $$ = list_make1($1); }
		| hint_list hint_item		{ $$ = lappend($1, $2); }
	;

hint_item:
		hint_name '(' hint_content ')'
		{
			$$ = create_hint_item($1, $3);
		}
	;

hint_name:			NAME { $$ = $1; };
hint_content:		NAME { $$ = $1; };

%%

static HintConfigData *
create_hint_item(const char *hint_name, const char *hint_contents)
{
	HintConfigData *res = palloc0(sizeof(HintConfigData));

	res->name = pstrdup(hint_name);
	res->contents = pstrdup(hint_contents);

	return res;
}
