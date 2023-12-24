/*-------------------------------------------------------------------------
 *
 * pg_trunc2del.c
 *		Extension that executes TRUNCATE statements as DELETE statements.
 *
 * Copyright (c) 1996-2024, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  pg_trunc2del/pg_trunc2del.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/heapam.h"
#include "executor/spi.h"
#include "fmgr.h"
#include "tcop/utility.h"
#include "utils/builtins.h"
#include "utils/snapmgr.h"

PG_MODULE_MAGIC;

/*--- Functions --- */

void		_PG_init(void);
void		_PG_fini(void);

static ProcessUtility_hook_type prev_utility_hook = NULL;

static void
trunc2del(PlannedStmt *pstmt,
		  const char *queryString,
		  bool readOnlyTree,
		  ProcessUtilityContext context,
		  ParamListInfo params,
		  QueryEnvironment *queryEnv,
		  DestReceiver *dest,
		  QueryCompletion *qc)
{
	Node	   *parsetree = pstmt->utilityStmt;

	/*
	 * Do custom processing for TRUNCATE. Note that this is not aimed at doing
	 * much for TRUNCATE CASCADE and triggers that should fire here. This
	 * becomes even more a mess should a DELETE trigger be defined on this
	 * relation.
	 */
	switch (nodeTag(parsetree))
	{
		case T_TruncateStmt:
			{
				TruncateStmt *stmt = (TruncateStmt *) parsetree;
				RangeVar   *rv = (RangeVar *) linitial(stmt->relations);
				int			ret;
				StringInfoData buf;
				Relation	rel;

				/*
				 * Check existence of relation queried, this is important in
				 * case of an unexistent relation to not let the user know of
				 * this run switch. As we are faking a TRUNCATE, it is as well
				 * important to take a exclusive lock on the relation operated
				 * on.
				 */
				rel = table_openrv(rv, AccessExclusiveLock);

				SPI_connect();
				initStringInfo(&buf);

				appendStringInfo(&buf, "DELETE FROM ");

				if (rv->schemaname)
					appendStringInfo(&buf, "%s.",
									 quote_identifier(rv->schemaname));

				appendStringInfo(&buf, "%s;", quote_identifier(rv->relname));

				ret = SPI_execute(buf.data, false, 0);

				if (ret != SPI_OK_DELETE)
					elog(ERROR, "Error while executing TRUNCATE (really?)");

				SPI_finish();

				/* keep lock until the end of transaction */
				table_close(rel, NoLock);
				return;
			}
		default:
			break;
	}

	/*
	 * Fallback to normal process, be it the previous hook loaded or the
	 * in-core code path if the previous hook does not exist.
	 */
	if (prev_utility_hook)
		(*prev_utility_hook) (pstmt, queryString, readOnlyTree,
							  context, params, queryEnv,
							  dest, qc);
	else
		standard_ProcessUtility(pstmt, queryString, readOnlyTree,
								context, params, queryEnv,
								dest, qc);
}

void
_PG_init(void)
{
	prev_utility_hook = ProcessUtility_hook;
	ProcessUtility_hook = trunc2del;
}

void
_PG_fini(void)
{
	ProcessUtility_hook = prev_utility_hook;
}
