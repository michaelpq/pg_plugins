/*-------------------------------------------------------------------------
 *
 * pg_trunc2del.c
 *		Extension that rewrite TRUNCATE statements to DELETE statements.
 *
 * Copyright (c) 1996-2015, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  pg_trunc2del/pg_trunc2del.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"
#include "optimizer/planner.h"
#include "parser/analyze.h"
#include "tcop/tcopprot.h"
#include "utils/snapmgr.h"
#include "fmgr.h"

PG_MODULE_MAGIC;


/*--- Functions --- */

void	_PG_init(void);
void	_PG_fini(void);

static PlannedStmt *t2d_planner_hook(Query *parse, int cursorOptions, ParamListInfo boundParams);
static planner_hook_type prev_planner_hook = NULL;

static void t2d_post_parse_analyze_hook(ParseState *pstate, Query *query);
static post_parse_analyze_hook_type prev_post_parse_analyze_hook = NULL;

static bool t2d_snapshot_set = false;

void
_PG_init(void)
{

	/* Install hook */
	prev_planner_hook = planner_hook;
	planner_hook = t2d_planner_hook;

	prev_post_parse_analyze_hook = post_parse_analyze_hook;
	post_parse_analyze_hook = t2d_post_parse_analyze_hook;
}

void
_PG_fini(void)
{
	/* uninstall hook */
	planner_hook = prev_planner_hook;
	post_parse_analyze_hook = prev_post_parse_analyze_hook;
}

static PlannedStmt *
t2d_planner_hook(Query *parse,
				 int cursorOptions,
				 ParamListInfo boundParams)
{
	PlannedStmt *plannedStmt = NULL;

	if (prev_planner_hook)
	{
		plannedStmt = prev_planner_hook(parse, cursorOptions, boundParams);
	} else {
		plannedStmt = standard_planner(parse, cursorOptions, boundParams);
	}

	/* pop the snapshot if we added one after parse analysis */
	if (t2d_snapshot_set)
	{
		PopActiveSnapshot();
		t2d_snapshot_set = false;
	}

	return plannedStmt;
}

static void
t2d_post_parse_analyze_hook(ParseState *pstate, Query *query){
	if (prev_post_parse_analyze_hook)
		prev_post_parse_analyze_hook(pstate, query);

	if ((query->commandType == CMD_UTILITY) &&
			IsA(query->utilityStmt, TruncateStmt))
	{
		TruncateStmt *stmt = (TruncateStmt *) query->utilityStmt;
		RangeVar *rv = (RangeVar *) linitial(stmt->relations);
		List *parsetree_list;
		Node *parsetree;
		StringInfoData sql;
		ParseState *new_pstate;
		Query	   *new_query;

		/*
		 * Planning a DELETE statement needs a snapshot, but postgres won't set
		 * up one because it thinks it's an UTILITY statement.
		 */
		PushActiveSnapshot(GetTransactionSnapshot());
		t2d_snapshot_set = true;

		initStringInfo(&sql);
		appendStringInfo(&sql, "DELETE FROM ");

		if (rv->schemaname)
			appendStringInfo(&sql,"\"%s\".", rv->schemaname);

		appendStringInfo(&sql, "\"%s\"", rv->relname);

		parsetree_list = pg_parse_query(sql.data);
		parsetree = (Node *) linitial(parsetree_list);

		new_pstate = make_parsestate(NULL);
		new_pstate->p_sourcetext = sql.data;
		new_query = transformTopLevelStmt(new_pstate, parsetree);

		/* Don't do that at home :) */
		memcpy(&(query->type), &(new_query->type), sizeof(Query));
		memcpy(&(pstate->parentParseState), &(new_pstate->parentParseState),
				sizeof(ParseState));
	}
}
