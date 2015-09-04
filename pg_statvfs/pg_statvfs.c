/*-------------------------------------------------------------------------
 *
 * pg_statvfs.c
 *		Wrapper for system call to statvfs()
 *
 * Copyright (c) 1996-2015, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  pg_statvfs/pg_statvfs.c
 *
 *-------------------------------------------------------------------------
 */

#include <sys/statvfs.h>

#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"
#include "miscadmin.h"

#include "access/htup_details.h"
#include "catalog/pg_type.h"
#include "storage/lock.h"
#include "utils/builtins.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(pg_statvfs);

/*
 * pg_statvfs
 * Wrapper to statvfs.
 */

Datum
pg_statvfs(PG_FUNCTION_ARGS)
{
	text	   *path_t = PG_GETARG_TEXT_P(0);
	TupleDesc	tupdesc;
	Datum		values[6];
	bool		nulls[6];
	struct statvfs statvfs_data;
	char	   *path;

	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 (errmsg("must be superuser to read files"))));

	/* Sanity checks on path given by user */
	path = text_to_cstring(path_t);
	canonicalize_path(path);	/* path can change length here */
	if (!is_absolute_path(path))
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 (errmsg("Relative path not allowed"))));

	/* Initialize values and NULL flags arrays */
	MemSet(values, 0, sizeof(values));
	MemSet(nulls, 0, sizeof(nulls));

	/* Initialize attributes information in the tuple descriptor */
	tupdesc = CreateTemplateTupleDesc(6, false);
	TupleDescInitEntry(tupdesc, (AttrNumber) 1, "f_bsize",
					   INT8OID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 2, "f_frsize",
					   INT8OID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 3, "f_blocks",
					   INT8OID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 4, "f_bfree",
					   INT8OID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 5, "f_bavail",
					   INT8OID, -1, 0);
	TupleDescInitEntry(tupdesc, (AttrNumber) 6, "f_fsid",
					   INT8OID, -1, 0);
	BlessTupleDesc(tupdesc);

	/* Now fetch statistics for this call */
	if (statvfs(path, &statvfs_data) < 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not stat file \"%s\": %m", path)));

	/* Fill in values */
	values[0] = Int64GetDatum(statvfs_data.f_bsize);
	values[1] = Int64GetDatum(statvfs_data.f_frsize);
	values[2] = Int64GetDatum(statvfs_data.f_blocks);
	values[3] = Int64GetDatum(statvfs_data.f_bfree);
	values[4] = Int64GetDatum(statvfs_data.f_bavail);
	values[5] = Int64GetDatum(statvfs_data.f_fsid);

	/* Returns the record as Datum */
	PG_RETURN_DATUM(
		HeapTupleGetDatum(heap_form_tuple(tupdesc, values, nulls)));
}
