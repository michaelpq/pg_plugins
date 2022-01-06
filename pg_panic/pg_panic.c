/*-------------------------------------------------------------------------
 *
 * pg_panic.c
 *		Kick a random PANIC at server level to test robustness of an
 *		installation.
 *
 * Copyright (c) 1996-2022, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  pg_panic/pg_panic.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"

#include "optimizer/planner.h"
#include "utils/guc.h"

PG_MODULE_MAGIC;

static double luck_factor = 0.001;

planner_hook_type prev_planner_hook = NULL;

void _PG_init(void);
void _PG_fini(void);

static PlannedStmt *
panic_hook(Query *parse, const char *query_string,
		   int cursorOptions, ParamListInfo boundParams)
{
	PlannedStmt *result;

	/* should we panic for this query? */
	if (random() < luck_factor * PG_INT32_MAX)
		elog(PANIC, "Jinx! Bad luck for today.");

	if (prev_planner_hook)
		result = (*prev_planner_hook) (parse, query_string, cursorOptions,
									   boundParams);
	else
		result = standard_planner(parse, query_string, cursorOptions,
								  boundParams);

	return result;
}

static void
pg_panic_load_params(void)
{
	DefineCustomRealVariable("pg_panic.luck_factor",
							 "percentage of triggering PANIC when planning query",
							 "Default of 0.001, range of values being [0..1]",
							 &luck_factor,
							 0.001,
							 0.0,
							 1.0,
							 PGC_USERSET,
							 0,
							 NULL,
							 NULL,
							 NULL);
}

void
_PG_init(void)
{
	pg_panic_load_params();

	/* switch hooks */
	prev_planner_hook = planner_hook;
	planner_hook = panic_hook;
}

void
_PG_fini(void)
{
	/* reinstall default hook */
	planner_hook = prev_planner_hook;
}
