/*-------------------------------------------------------------------------
 *
 * compression_test.c
 *	  Set of utilities to test compression.
 *
 * Copyright (c) 1996-2025, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  compression_test/compression_test.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/heapam.h"
#include "access/htup_details.h"
#include "catalog/catalog.h"
#include "catalog/namespace.h"
#include "catalog/pg_type.h"
#include "common/pg_lzcompress.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "storage/bufmgr.h"
#include "utils/builtins.h"
#include "utils/rel.h"

PG_MODULE_MAGIC;

/* maximum size for compression buffer of block image */
#define PGLZ_MAX_BLCKSZ		PGLZ_MAX_OUTPUT(BLCKSZ)

PG_FUNCTION_INFO_V1(get_raw_page);
PG_FUNCTION_INFO_V1(compress_data);
PG_FUNCTION_INFO_V1(decompress_data);
PG_FUNCTION_INFO_V1(bytea_size);

/*
 * get_raw_page
 *
 * Returns a copy of a page from shared buffers as a bytea, with hole
 * filled with zeros or simply without hole, with the length of the page
 * offset to be able to reconstitute the page entirely using the data
 * returned by this function.
 */
Datum
get_raw_page(PG_FUNCTION_ARGS)
{
	Oid			relid = PG_GETARG_OID(0);
	uint32		blkno = PG_GETARG_UINT32(1);
	bool		with_hole = PG_GETARG_BOOL(2);
	bytea	   *raw_page;
	Relation	rel;
	char		raw_page_data[BLCKSZ];
	Buffer		buf;
	TupleDesc	tupdesc;
	Datum		result;
	Datum		values[2];
	bool		nulls[2];
	HeapTuple	tuple;
	PageHeader	page_header;
	int16		hole_offset,
				hole_length;

	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 (errmsg("must be superuser to use raw functions"))));

	rel = relation_open(relid, AccessShareLock);

	/* Check that this relation has storage */
	if (rel->rd_rel->relkind == RELKIND_VIEW)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("cannot get raw page from view \"%s\"",
						RelationGetRelationName(rel))));
	if (rel->rd_rel->relkind == RELKIND_COMPOSITE_TYPE)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("cannot get raw page from composite type \"%s\"",
						RelationGetRelationName(rel))));
	if (rel->rd_rel->relkind == RELKIND_FOREIGN_TABLE)
		ereport(ERROR,
				(errcode(ERRCODE_WRONG_OBJECT_TYPE),
				 errmsg("cannot get raw page from foreign table \"%s\"",
						RelationGetRelationName(rel))));

	/*
	 * Reject attempts to read non-local temporary relations; we would be
	 * likely to get wrong data since we have no visibility into the owning
	 * session's local buffers.
	 */
	if (RELATION_IS_OTHER_TEMP(rel))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("cannot access temporary tables of other sessions")));

	if (blkno >= RelationGetNumberOfBlocksInFork(rel, MAIN_FORKNUM))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("block number %u is out of range for relation \"%s\"",
						blkno, RelationGetRelationName(rel))));

	/* Build a tuple descriptor for our result type */
	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		elog(ERROR, "return type must be a row type");

	/* Take a copy of the page to work on */
	buf = ReadBufferExtended(rel, MAIN_FORKNUM, blkno, RBM_NORMAL, NULL);
	LockBuffer(buf, BUFFER_LOCK_SHARE);
	memcpy(raw_page_data, BufferGetPage(buf), BLCKSZ);
	LockBuffer(buf, BUFFER_LOCK_UNLOCK);
	ReleaseBuffer(buf);
	relation_close(rel, AccessShareLock);

	page_header = (PageHeader) raw_page_data;
	hole_length = page_header->pd_upper - page_header->pd_lower;
	hole_offset = page_header->pd_lower;

	/*
	 * If hole is wanted in the page returned, fill it with zeros. If not,
	 * copy to the return buffer the page without the hole.
	 */
	if (with_hole)
	{
		raw_page = (bytea *) palloc(BLCKSZ + VARHDRSZ);
		SET_VARSIZE(raw_page, BLCKSZ + VARHDRSZ);
		memcpy(VARDATA(raw_page), raw_page_data, BLCKSZ);
		MemSet(raw_page_data + hole_offset, 0, hole_length);
	}
	else
	{
		raw_page = (bytea *) palloc(BLCKSZ + VARHDRSZ - hole_length);
		SET_VARSIZE(raw_page, BLCKSZ + VARHDRSZ - hole_length);
		memcpy(VARDATA(raw_page), raw_page_data, hole_offset);
		memcpy(VARDATA(raw_page) + hole_offset,
			   raw_page_data + hole_offset + hole_length,
			   BLCKSZ - (hole_offset + hole_length));
	}

	/* Build and return the tuple. */
	values[0] = PointerGetDatum(raw_page);
	if (with_hole)
		values[1] = UInt16GetDatum(0);
	else
		values[1] = UInt16GetDatum(hole_offset);

	memset(nulls, 0, sizeof(nulls));

	tuple = heap_form_tuple(tupdesc, values, nulls);
	result = HeapTupleGetDatum(tuple);
	PG_RETURN_DATUM(result);
}

/*
 * compress_data
 *
 * Compress the bytea buffer and return the result as bytea.
 */
Datum
compress_data(PG_FUNCTION_ARGS)
{
	bytea	   *raw_data = PG_GETARG_BYTEA_P(0);
	bytea	   *res;
	int32		compressed_len;
	char	   *compressed_data;
	PGLZ_Strategy strategy;

	memcpy(&strategy, (PGLZ_Strategy *) PGLZ_strategy_always,
		   sizeof(PGLZ_Strategy));

	/* Get custom values if specified by user */
	if (PG_NARGS() == 7)
	{
		strategy.min_input_size = PG_GETARG_INT32(1);
		strategy.max_input_size = PG_GETARG_INT32(2);
		strategy.min_comp_rate = PG_GETARG_INT32(3);
		strategy.first_success_by = PG_GETARG_INT32(4);
		strategy.match_size_good = PG_GETARG_INT32(5);
		strategy.match_size_drop = PG_GETARG_INT32(6);
	}

	/* Compress data in build */
	compressed_data = palloc(PGLZ_MAX_OUTPUT(VARSIZE(raw_data) - VARHDRSZ));
	compressed_len = pglz_compress(VARDATA(raw_data),
								   VARSIZE(raw_data) - VARHDRSZ,
								   compressed_data,
								   &strategy);

	/* if compression failed return the original data */
	if (compressed_len < 0)
		PG_RETURN_BYTEA_P(raw_data);

	/* Build result */
	res = (bytea *) palloc(VARHDRSZ + compressed_len);
	SET_VARSIZE(res, compressed_len + VARHDRSZ);
	memcpy(VARDATA(res), compressed_data, compressed_len);
	pfree(compressed_data);
	PG_RETURN_BYTEA_P(res);
}

/*
 * decompress_data
 *
 * Decompress the bytea buffer and return result as bytea, this may be a page
 * with its hole filled with zeros or a page without a hole.
 */
Datum
decompress_data(PG_FUNCTION_ARGS)
{
	bytea	   *compress_data = PG_GETARG_BYTEA_P(0);
	int16		raw_len = PG_GETARG_INT16(1);
	bytea	   *res;
	char	   *uncompress_buffer;

	uncompress_buffer = palloc(raw_len);
	if (pglz_decompress(VARDATA(compress_data),
						VARSIZE(compress_data) - VARHDRSZ,
						uncompress_buffer, raw_len, true) < 0)
		ereport(ERROR, (errmsg("Decompression failed...")));

	/* Build result */
	res = (bytea *) palloc(raw_len + VARHDRSZ);
	SET_VARSIZE(res, raw_len + VARHDRSZ);
	memcpy(VARDATA(res), uncompress_buffer, raw_len);
	pfree(uncompress_buffer);
	PG_RETURN_BYTEA_P(res);
}

/*
 * bytea_size
 *
 * Return the size of a bytea field, this data is useful to pass for
 * to a function doing decompression like decompress_data() above.
 */
Datum
bytea_size(PG_FUNCTION_ARGS)
{
	bytea	   *data = PG_GETARG_BYTEA_P(0);

	PG_RETURN_INT32(VARSIZE(data) - VARHDRSZ);
}
