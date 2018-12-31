/*-------------------------------------------------------------------------
 *
 * pg_swap_pages.c
 *		Extension switching pages of a relation via WAL replay.
 *
 * Copyright (c) 1996-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  pg_swap_pages/pg_swap_pages.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"

#include "access/generic_xlog.h"
#include "access/heapam.h"
#include "storage/bufmgr.h"
#include "storage/bufpage.h"
#include "utils/relcache.h"

PG_MODULE_MAGIC;

/*
 * Switch pages of a relation and WAL-log it. Corrupts easily a system.
 */
PG_FUNCTION_INFO_V1(pg_swap_pages);

Datum
pg_swap_pages(PG_FUNCTION_ARGS)
{
	Oid			relid = PG_GETARG_OID(0);
	uint32		blkno1 = PG_GETARG_UINT32(1);
	uint32		blkno2 = PG_GETARG_UINT32(2);
	Relation	rel;
	Buffer		buf1, buf2;
	Page		page1, page2;
	char		raw_page[BLCKSZ];
	GenericXLogState *state;

	rel = relation_open(relid, AccessShareLock);

	/* Some sanity checks */
	if (blkno1 > MaxBlockNumber)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("invalid block number 1")));
	if (blkno2 > MaxBlockNumber)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("invalid block number 2")));

    if (blkno1 >= RelationGetNumberOfBlocksInFork(rel, MAIN_FORKNUM))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("block number 1 %u is out of range for relation \"%s\"",
						blkno1, RelationGetRelationName(rel))));
    if (blkno2 >= RelationGetNumberOfBlocksInFork(rel, MAIN_FORKNUM))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("block number 2 %u is out of range for relation \"%s\"",
						blkno2, RelationGetRelationName(rel))));

	/* Take copy of buffer 1 */
	buf1 = ReadBufferExtended(rel, MAIN_FORKNUM, blkno1, RBM_NORMAL, NULL);
	LockBuffer(buf1, BUFFER_LOCK_SHARE);

	/* And buffer 2 */
	buf2 = ReadBufferExtended(rel, MAIN_FORKNUM, blkno2, RBM_NORMAL, NULL);
	LockBuffer(buf2, BUFFER_LOCK_SHARE);

	/* Now generate WAL records registering both buffers and swapping them */
	state = GenericXLogStart(rel);
	page1 = GenericXLogRegisterBuffer(state, buf1, GENERIC_XLOG_FULL_IMAGE);
	page2 = GenericXLogRegisterBuffer(state, buf2, GENERIC_XLOG_FULL_IMAGE);

	/* Switch the pages' contents */
	memcpy(raw_page, page1, BLCKSZ);
	memcpy(page1, page2, BLCKSZ);
	memcpy(page2, raw_page, BLCKSZ);

	/* Time to log the changes */
	GenericXLogFinish(state);

	/* cleanup and finish */
	LockBuffer(buf1, BUFFER_LOCK_UNLOCK);
	LockBuffer(buf2, BUFFER_LOCK_UNLOCK);
	ReleaseBuffer(buf1);
	ReleaseBuffer(buf2);

	relation_close(rel, AccessShareLock);
	PG_RETURN_NULL();
}
