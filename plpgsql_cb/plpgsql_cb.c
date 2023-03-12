/*-------------------------------------------------------------------------
 *
 * plpgsql_cb.c
 *		Simple template making use of PLpgSQL plugin structure with its
 *		callbacks for statement and function controls.
 *
 * Copyright (c) 1996-2023, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		plpgsql_cb/plpgsql_cb.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "plpgsql.h"

/* Essential for shared libs! */
PG_MODULE_MAGIC;

/* Entry point of library loading */
void		_PG_init(void);

/* Exit point of library loading */
void		_PG_fini(void);

static void plpgsql_cb_func_setup(PLpgSQL_execstate *estate,
								  PLpgSQL_function *func);
static void plpgsql_cb_func_beg(PLpgSQL_execstate *estate,
								PLpgSQL_function *func);
static void plpgsql_cb_func_end(PLpgSQL_execstate *estate,
								PLpgSQL_function *func);
static void plpgsql_cb_stmt_beg(PLpgSQL_execstate *estate,
								PLpgSQL_stmt *stmt);
static void plpgsql_cb_stmt_end(PLpgSQL_execstate *estate,
								PLpgSQL_stmt *stmt);

static PLpgSQL_plugin
			plugin_funcs = {
	plpgsql_cb_func_setup,
	plpgsql_cb_func_beg,
	plpgsql_cb_func_end,
	plpgsql_cb_stmt_beg,
	plpgsql_cb_stmt_end,
	NULL,
	NULL
};

static void
plpgsql_cb_func_setup(PLpgSQL_execstate *estate, PLpgSQL_function *func)
{
	elog(NOTICE, "function setup: \"%s\"", func->fn_signature);
}

static void
plpgsql_cb_func_beg(PLpgSQL_execstate *estate, PLpgSQL_function *func)
{
	elog(NOTICE, "function beg: \"%s\"", func->fn_signature);
}

static void
plpgsql_cb_func_end(PLpgSQL_execstate *estate, PLpgSQL_function *func)
{
	elog(NOTICE, "function end: \"%s\"", func->fn_signature);
}

static void
plpgsql_cb_stmt_beg(PLpgSQL_execstate *estate, PLpgSQL_stmt *stmt)
{
	elog(NOTICE, "statement beg - ln: %d", stmt->lineno);
}

static void
plpgsql_cb_stmt_end(PLpgSQL_execstate *estate, PLpgSQL_stmt *stmt)
{
	elog(NOTICE, "statement end - ln: %d", stmt->lineno);
}

/*
 * _PG_init
 *
 * Load point of library.
 */
void
_PG_init(void)
{
	/* Set up a rendezvous point with instrumentation plugin */
	PLpgSQL_plugin **var_ptr = (PLpgSQL_plugin **)
	find_rendezvous_variable("PLpgSQL_plugin");

	*var_ptr = &plugin_funcs;
}

/*
 * _PG_fini
 *
 * Unload point of library.
 */
void
_PG_fini(void)
{
}
