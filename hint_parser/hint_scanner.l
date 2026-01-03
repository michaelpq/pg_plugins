%top{
/*-------------------------------------------------------------------------
 *
 * hint_scanner.l
 *	  a lexical scanner for query hints
 *
 * Portions Copyright (c) 1996-2026, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 *
 * IDENTIFICATION
 *	  hint_parser/hint_scanner.l
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "lib/stringinfo.h"
#include "nodes/pg_list.h"

/*
 * NB: include hint_gram.h only AFTER including hint.h, because hint.h
 * includes node definitions needed for YYSTYPE.
 */
#include "hint_parser.h"
#include "hint_gram.h"
}

%{
/* Avoid exit() on fatal scanner errors (a bit ugly -- see yy_fatal_error) */
#undef fprintf
#define fprintf(file, fmt, msg)  fprintf_to_ereport(fmt, msg)

static void
fprintf_to_ereport(const char *fmt, const char *msg)
{
	ereport(ERROR, (errmsg_internal("%s", msg)));
}

/* Handles to the buffer that the lexer uses internally */
static YY_BUFFER_STATE scanbufhandle;

/* LCOV_EXCL_START */

%}

%option 8bit
%option never-interactive
%option nodefault
%option noinput
%option nounput
%option noyywrap
%option warn
%option prefix="hint_yy"

space			[ \t\n\r\f\v]

%%

{space}+	{ /* ignore */ }

[A-Za-z0-9]+	{
			hint_yylval.str = pstrdup(yytext);
			return NAME;
		}

"("		{ return '('; }
")"		{ return ')'; }

<<EOF>>		{
			yyterminate();
		}

.		{ /* ignore */ }

%%

/* LCOV_EXCL_STOP */

/* Needs to be here for access to yytext */
void
hint_yyerror(const char *message)
{
	/* report only the first error in a parse operation */
	if (hint_parse_error_msg)
		return;
	if (yytext[0])
		hint_parse_error_msg = psprintf("%s at or near \"%s\"",
						message, yytext);
	else
		hint_parse_error_msg = psprintf("%s at end of input",
						message);
}

void
hint_scanner_init(const char *str)
{
	Size		slen = strlen(str);
	char	   *scanbuf;

	/*
	 * Might be left over after ereport()
	 */
	if (YY_CURRENT_BUFFER)
		yy_delete_buffer(YY_CURRENT_BUFFER);

	/*
	 * Make a scan buffer with special termination needed by flex.
	 */
	scanbuf = (char *) palloc(slen + 2);
	memcpy(scanbuf, str, slen);
	scanbuf[slen] = scanbuf[slen + 1] = YY_END_OF_BUFFER_CHAR;
	scanbufhandle = yy_scan_buffer(scanbuf, slen + 2);

	/* Make sure we start in proper state */
	BEGIN(INITIAL);
}

void
hint_scanner_finish(void)
{
	yy_delete_buffer(scanbufhandle);
	scanbufhandle = NULL;
}
