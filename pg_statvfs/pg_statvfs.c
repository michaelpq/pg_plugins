/*-------------------------------------------------------------------------
 *
 * pg_statvfs.c
 *		Wrapper for system call to statvfs()
 *
 * Copyright (c) 1996-2017, PostgreSQL Global Development Group
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
#include "postmaster/syslogger.h"
#include "storage/lock.h"
#include "utils/array.h"
#include "utils/builtins.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(pg_statvfs);


/*
 * Convert a "text" filename argument to C string, and check it's allowable.
 *
 * Filename may be absolute or relative to the DataDir, but we only allow
 * absolute paths that match DataDir or Log_directory.
 */
static char *
convert_and_check_filename(text *arg)
{
	char       *filename;

	filename = text_to_cstring(arg);
	canonicalize_path(filename);    /* filename can change length here */

	if (is_absolute_path(filename))
	{
		/* Disallow '/a/b/data/..' */
		if (path_contains_parent_reference(filename))
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 (errmsg("reference to parent directory (\"..\") not allowed"))));

		/*
		 * Allow absolute paths if within DataDir or Log_directory, even
		 * though Log_directory might be outside DataDir.
		 */
		if (!path_is_prefix_of_path(DataDir, filename) &&
			(!is_absolute_path(Log_directory) ||
			 !path_is_prefix_of_path(Log_directory, filename)))
			ereport(ERROR,
					(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
					 (errmsg("absolute path not allowed"))));
	}
	else if (!path_is_relative_and_below_cwd(filename))
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 (errmsg("path must be in or below the current directory"))));

	return filename;
}

/*
 * pg_statvfs
 * Wrapper to statvfs.
 */

Datum
pg_statvfs(PG_FUNCTION_ARGS)
{
	text	   *path_t = PG_GETARG_TEXT_P(0);
	TupleDesc	tupdesc;
	Datum		values[11];
	bool		nulls[11];
	struct statvfs fsdata;
	char	   *path;
	Datum       flags[16];
	int         nflags = 0;

	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 (errmsg("must be superuser to read files"))));

	/* Convert path name into something appropriate */
	path = convert_and_check_filename(path_t);

	/* Initialize values and NULL flags arrays */
	MemSet(values, 0, sizeof(values));
	MemSet(nulls, 0, sizeof(nulls));

	/* Build a tuple descriptor for our result type */
	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		elog(ERROR, "return type must be a row type");

	/* Now fetch statistics for this call */
	if (statvfs(path, &fsdata) < 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not stat filesystem path \"%s\": %m", path)));

	/* Convert the flag bit masks to an array of human-readable names */
	if (fsdata.f_flag & ST_NOSUID)
		flags[nflags++] = CStringGetTextDatum("nosuid");
	if (fsdata.f_flag & ST_RDONLY)
		flags[nflags++] = CStringGetTextDatum("rdonly");
#if defined(__linux__)
	if (fsdata.f_flag & ST_MANDLOCK)
		flags[nflags++] = CStringGetTextDatum("mandlock");
	if (fsdata.f_flag & ST_NOATIME)
		flags[nflags++] = CStringGetTextDatum("noatime");
	if (fsdata.f_flag & ST_NODEV)
		flags[nflags++] = CStringGetTextDatum("nodev");
	if (fsdata.f_flag & ST_NODIRATIME)
		flags[nflags++] = CStringGetTextDatum("nodiratime");
	if (fsdata.f_flag & ST_NOEXEC)
		flags[nflags++] = CStringGetTextDatum("noexec");
	if (fsdata.f_flag & ST_RELATIME)
		flags[nflags++] = CStringGetTextDatum("relatime");
	if (fsdata.f_flag & ST_SYNCHRONOUS)
		flags[nflags++] = CStringGetTextDatum("synchronous");
#endif

	/* Fill in values */
	values[0] = Int64GetDatum(fsdata.f_bsize);
	values[1] = Int64GetDatum(fsdata.f_frsize);
	values[2] = Int64GetDatum(fsdata.f_blocks);
	values[3] = Int64GetDatum(fsdata.f_bfree);
	values[4] = Int64GetDatum(fsdata.f_bavail);
	values[5] = Int64GetDatum(fsdata.f_files);
	values[6] = Int64GetDatum(fsdata.f_ffree);
	values[7] = Int64GetDatum(fsdata.f_favail);
	values[8] = Int64GetDatum(fsdata.f_fsid);
	values[9] = Int64GetDatum(fsdata.f_namemax);
	values[10] = PointerGetDatum(
					construct_array(flags, nflags, TEXTOID, -1, false, 'i'));

	/* Returns the record as Datum */
	PG_RETURN_DATUM(
		HeapTupleGetDatum(heap_form_tuple(tupdesc, values, nulls)));
}
