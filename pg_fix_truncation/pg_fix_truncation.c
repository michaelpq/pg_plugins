/*-------------------------------------------------------------------------
 *
 * pg_fix_truncation.c
 *		Set of functions for a minimal extension template
 *
 * Copyright (c) 1996-2020, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  pg_fix_truncation/pg_fix_truncation.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"

#include "access/heapam.h"
#include "access/xlog.h"
#include "catalog/storage_xlog.h"
#include "storage/bufmgr.h"
#include "storage/freespace.h"
#include "storage/smgr.h"
#include "utils/rel.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(pg_truncate_fsm);

/*
 * Truncate FSM file of a relation up to the size of its parent relation.
 * This takes an exclusive lock on the parent relation, still that's less
 * costly than having to shut down the server.
 */
Datum
pg_truncate_fsm(PG_FUNCTION_ARGS)
{
	Oid			relid = PG_GETARG_OID(0);
	Relation	rel;
	BlockNumber	tgt_blk;

	rel = relation_open(relid, AccessExclusiveLock);

	if (rel->rd_rel->relkind != RELKIND_RELATION &&
		rel->rd_rel->relkind != RELKIND_MATVIEW &&
		rel->rd_rel->relkind != RELKIND_TOASTVALUE)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
		   errmsg("\"%s\" is not a table, materialized view, or TOAST table",
				  RelationGetRelationName(rel))));

	RelationOpenSmgr(rel);
	tgt_blk = RelationGetNumberOfBlocksInFork(rel, MAIN_FORKNUM);

	/*
	 * WAL-log the truncation before actually doing it. This will prevent
	 * torn pages if there is a crash in-between.
	 */
	if (RelationNeedsWAL(rel))
	{
		xl_smgr_truncate xlrec;

		xlrec.blkno = tgt_blk;
		xlrec.rnode = rel->rd_node;
		xlrec.flags = SMGR_TRUNCATE_FSM;

		XLogBeginInsert();
		XLogRegisterData((char *) &xlrec, sizeof(xlrec));

		XLogInsert(RM_SMGR_ID, XLOG_SMGR_TRUNCATE | XLR_SPECIAL_REL_UPDATE);
	}

	FreeSpaceMapPrepareTruncateRel(rel, tgt_blk);

	/*
	 * Release the lock right away, and not at commit time.
	 * See similar comments in pg_visibility...
	 */
	relation_close(rel, AccessExclusiveLock);

	/* Nothing to return. */
	PG_RETURN_VOID();
}
