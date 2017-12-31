/*-------------------------------------------------------------------------
 *
 * wal_utils.c
 *		Set of tools and utilities for handling of WAL-related data.
 *
 * Copyright (c) 1996-2018, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  wal_utils/wal_utils.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"

#include <sys/stat.h>

#include "access/timeline.h"
#include "access/xlog_internal.h"
#include "catalog/pg_type.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "utils/pg_lsn.h"

PG_MODULE_MAGIC;

static List *parseTimeLineHistory(char *buffer);

/*
 * Set of SQL-callable functions.
 */
PG_FUNCTION_INFO_V1(archive_parse_history);
PG_FUNCTION_INFO_V1(archive_build_segment_list);
PG_FUNCTION_INFO_V1(archive_get_size);
PG_FUNCTION_INFO_V1(archive_get_data);

/*
 * parseTimeLineHistory
 *
 * Using in input a buffer including a complete history file, parse its
 * data and return a list of TimeLineHistoryEntry entries filled correctly
 * with the data of the file.
 */
static List *
parseTimeLineHistory(char *buffer)
{
	char	   *fline;
	List	   *entries = NIL;
	int			nlines = 0;
	TimeLineID	lasttli = 0;
	XLogRecPtr	prevend;
	char	   *bufptr;
	bool		lastline = false;

	/*
	 * Parse the file...
	 */
	prevend = InvalidXLogRecPtr;
	bufptr = buffer;
	while (!lastline)
	{
		char	   *ptr;
		TimeLineHistoryEntry *entry;
		TimeLineID	tli;
		uint32		switchpoint_hi;
		uint32		switchpoint_lo;
		int			nfields;

		fline = bufptr;
		while (*bufptr && *bufptr != '\n')
			bufptr++;
		if (!(*bufptr))
			lastline = true;
		else
			*bufptr++ = '\0';

		/* skip leading whitespace and check for # comment */
		for (ptr = fline; *ptr; ptr++)
		{
			if (!isspace((unsigned char) *ptr))
				break;
		}
		if (*ptr == '\0' || *ptr == '#')
			continue;

		nfields = sscanf(fline, "%u\t%X/%X", &tli, &switchpoint_hi, &switchpoint_lo);

		if (nfields < 1)
		{
			/* expect a numeric timeline ID as first field of line */
			ereport(ERROR,
					(errmsg("syntax error in history file: %s", fline),
					 errhint("Expected a numeric timeline ID.")));
		}
		if (nfields != 3)
		{
			ereport(ERROR,
					(errmsg("syntax error in history file: %s", fline),
					 errhint("Expected a write-ahead log switchpoint location.")));
		}
		if (tli <= lasttli)
			ereport(ERROR,
					(errmsg("invalid data in history file: %s", fline),
					 errhint("Timeline IDs must be in increasing sequence.")));

		lasttli = tli;

		nlines++;

		entry = palloc(sizeof(TimeLineHistoryEntry));
		entry->tli = tli;
		entry->begin = prevend;
		entry->end = ((uint64) (switchpoint_hi)) << 32 | (uint64) switchpoint_lo;
		prevend = entry->end;

		entries = lappend(entries, entry);

		/* we ignore the remainder of each line */
	}

	return entries;
}


/*
 * archive_parse_history
 *
 * Parse input buffer of a history file and build a set of rows to
 * give a SQL representation of TimeLineHistoryEntry entries part
 * of a timeline history file.
 */
Datum
archive_parse_history(PG_FUNCTION_ARGS)
{
	char	   *history_buf = TextDatumGetCString(PG_GETARG_DATUM(0));
	TupleDesc   tupdesc;
	Tuplestorestate *tupstore;
	ReturnSetInfo *rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;
	MemoryContext per_query_ctx;
	MemoryContext oldcontext;
	List	   *entries = NIL;
	ListCell   *entry;

	/* check to see if caller supports us returning a tuplestore */
	if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("set-valued function called in context that cannot accept a set")));
	if (!(rsinfo->allowedModes & SFRM_Materialize))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("materialize mode required, but it is not "
						"allowed in this context")));

	per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
	oldcontext = MemoryContextSwitchTo(per_query_ctx);

	/* Build tuple descriptor */
	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		elog(ERROR, "return type must be a row type");

	tupstore = tuplestore_begin_heap(true, false, work_mem);
	rsinfo->returnMode = SFRM_Materialize;
	rsinfo->setResult = tupstore;
	rsinfo->setDesc = tupdesc;
	MemoryContextSwitchTo(oldcontext);

	/* parse the history file */
	entries = parseTimeLineHistory(history_buf);

	/* represent its data as a set of tuples */
	foreach(entry, entries)
	{
		Datum		values[3];
		bool		nulls[3];
		TimeLineHistoryEntry *history = (TimeLineHistoryEntry *) lfirst(entry);

		/* Initialize values and NULL flags arrays */
		MemSet(values, 0, sizeof(values));
		MemSet(nulls, 0, sizeof(nulls));

		/* timeline number */
		values[0] = Int32GetDatum(history->tli);

		/* begin position */
		if (XLogRecPtrIsInvalid(history->begin))
			nulls[1] = true;
		else
			values[1] = LSNGetDatum(history->begin);

		/* end position */
		if (XLogRecPtrIsInvalid(history->end))
			nulls[2] = true;
		else
			values[2] = LSNGetDatum(history->end);

		tuplestore_putvalues(tupstore, tupdesc, values, nulls);
	}

	/* clean up and return the tuplestore */
	tuplestore_donestoring(tupstore);

	return (Datum) 0;
}

/*
 * archive_build_segment_list
 *
 * Taking in input an origin timeline and LSN, as well as a target timeline
 * and LSN, build a list of WAL segments able to allow a standby pointing to
 * the origin timeline to reach the target timeline.
 *
 * Note that the origin and the target timelines need to be direct parents,
 * and user needs to provide in input a buffer corresponding to a history
 * file in text format, on which is performed a set of tests, checking for
 * timeline jumps to build the correct list of segments to join the origin
 * and the target.
 *
 * The target timeline needs normally to match the history file name given
 * in input, but this is let up to the user to combine both correctly for
 * flexibility, still this routine checks if the target LSN is newer than
 * the last entry in the history file, as well as it checks if the last
 * timeline entry is higher than the target.
 */
Datum
archive_build_segment_list(PG_FUNCTION_ARGS)
{
	TimeLineID	origin_tli;
	XLogRecPtr	origin_lsn;
	TimeLineID	target_tli;
	XLogRecPtr	target_lsn;
	char	   *history_buf;
	ReturnSetInfo  *rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;
	MemoryContext	per_query_ctx;
	MemoryContext	oldcontext;
	TupleDesc		tupdesc;
	Tuplestorestate *tupstore;
	List		   *entries = NIL;
	ListCell	   *entry;
	TimeLineHistoryEntry *history;
	bool			history_match = false;
	XLogRecPtr		current_seg_lsn;
	TimeLineID		current_tli;
	char			xlogfname[MAXFNAMELEN];
	Datum			values[1];
	bool			nulls[1];
	XLogSegNo		logSegNo;

	/* Sanity checks for arguments */
	if (PG_ARGISNULL(0) || PG_ARGISNULL(1) ||
		PG_ARGISNULL(2) || PG_ARGISNULL(3))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("origin or target data cannot be NULL")));

	origin_tli = PG_GETARG_INT32(0);
	origin_lsn = PG_GETARG_LSN(1);
	target_tli = PG_GETARG_INT32(2);
	target_lsn = PG_GETARG_LSN(3);
	history_buf = PG_ARGISNULL(4) ? NULL :
		TextDatumGetCString(PG_GETARG_DATUM(4));

	/* check to see if caller supports us returning a tuplestore */
	if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("set-valued function called in context that cannot accept a set")));
	if (!(rsinfo->allowedModes & SFRM_Materialize))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("materialize mode required, but it is not "
						"allowed in this context")));

	per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
	oldcontext = MemoryContextSwitchTo(per_query_ctx);
	tupdesc = CreateTemplateTupleDesc(1, false);

	TupleDescInitEntry(tupdesc, (AttrNumber) 1, "wal_segs", TEXTOID, -1, 0);

	tupstore = tuplestore_begin_heap(true, false, work_mem);
	rsinfo->returnMode = SFRM_Materialize;
	rsinfo->setResult = tupstore;
	rsinfo->setDesc = tupdesc;

	MemoryContextSwitchTo(oldcontext);

	/* First do sanity checks on target and origin data */
	if (origin_lsn > target_lsn)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("origin LSN %X/%X newer than target LSN %X/%X",
						(uint32) (origin_lsn >> 32),
						(uint32) origin_lsn,
						(uint32) (target_lsn >> 32),
						(uint32) target_lsn)));
	if (origin_tli > target_tli)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("origin timeline %u newer than target timeline %u",
						origin_tli, target_tli)));

	/*
	 * Check parentage of the target and origin timelines if a history file
	 * has been given by caller.
	 */
	if (history_buf)
	{
		/* parse the history file */
		entries = parseTimeLineHistory(history_buf);

		if (entries == NIL)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("timeline history found empty after parsing")));

		/*
		 * Check that the target data is newer than the last entry in the history
		 * file. Better safe than sorry.
		 */
		history = (TimeLineHistoryEntry *) llast(entries);
		if (history->tli >= target_tli)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("timeline of last history entry %u newer than or "
							"equal to target timeline %u",
						history->tli, target_tli)));
		if (history->end > target_lsn)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("LSN %X/%X of last history entry newer than target LSN %X/%X",
							(uint32) (history->end >> 32),
							(uint32) history->end,
							(uint32) (target_lsn >> 32),
							(uint32) target_lsn)));
		/*
		 * Check that origin and target are direct parents, we already know that
		 * the target fits with the history file.
		 */
		foreach(entry, entries)
		{
			history = (TimeLineHistoryEntry *) lfirst(entry);

			if (history->begin <= origin_lsn &&
				history->end >= origin_lsn &&
				history->tli == origin_tli)
			{
				history_match = true;
				break;
			}
		}

		if (!history_match)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("origin data not a direct parent of target")));

		/*
		 * Abuse this variable as temporary storage, we want the beginning
		 * of the last, target timeline to match the end of the last timeline
		 * tracked in the history file.
		 */
		current_seg_lsn = history->end;
	}
	else
	{
		/* Here the origin and target timeline match */
		if (origin_tli != target_tli)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("origin and target timelines not matching without history file")));

		current_seg_lsn = origin_lsn;
	}

	/*
	 * Before listing the list of files, add a last history entry using the
	 * target data, this simplifies the logic below to build the segment list.
	 */
	history = (TimeLineHistoryEntry *) palloc(sizeof(TimeLineHistoryEntry));
	history->tli = target_tli;
	history->begin = current_seg_lsn;
	history->end = target_lsn;
	entries = lappend(entries, history);

	/*
	 * Fill in the data by finding all segments between the origin and the
	 * target. First segment is the one of the origin LSN, with origin
	 * timeline. Note that when jumping to a new timeline, Postgres
	 * switches immediately to a new segment with the new timeline, giving
	 * up on the last, partial segment.
	 */

	/* Begin tracking at the beginning of the next segment */
	current_seg_lsn = origin_lsn + XLOG_SEG_SIZE;
	current_seg_lsn -= current_seg_lsn % XLOG_SEG_SIZE;
	current_tli = origin_tli;

	foreach(entry, entries)
	{
		history = (TimeLineHistoryEntry *) lfirst(entry);

		current_tli = history->tli;

		/* save the segment value */
		while (current_seg_lsn >= history->begin &&
			   current_seg_lsn < history->end)
		{
			XLByteToPrevSeg(current_seg_lsn, logSegNo);
			XLogFileName(xlogfname, current_tli, logSegNo);
			nulls[0] = false;
			values[0] = CStringGetTextDatum(xlogfname);
			tuplestore_putvalues(tupstore, tupdesc, values, nulls);

			/*
			 * Add equivalent of one segment, and just track the beginning
			 * of it.
			 */
			current_seg_lsn += XLOG_SEG_SIZE;
			current_seg_lsn -= current_seg_lsn % XLOG_SEG_SIZE;
		}
	}

	/*
	 * Add as well the last segment possible, this is needed to reach
	 * consistency up to the target point.
	 */
	XLByteToPrevSeg(target_lsn, logSegNo);
	XLogFileName(xlogfname, target_tli, logSegNo);
	nulls[0] = false;
	values[0] = CStringGetTextDatum(xlogfname);
	tuplestore_putvalues(tupstore, tupdesc, values, nulls);

	/* clean up and return the tuplestore */
	tuplestore_donestoring(tupstore);

	return (Datum) 0;
}

/*
 * Check the defined file name, looking at if it is an absolute path
 * and if it contains references to a parent directory. Then build
 * a full path name using the path defined for the archives which is
 * enforced by the environment where Postgres is running.
 */
static char *
check_and_build_filepath(char *filename)
{
	char	   *filepath;
	char	   *archive_path;

	if (is_absolute_path(filename))
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 (errmsg("absolute path not allowed"))));
	if (path_contains_parent_reference(filename))
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 (errmsg("reference to parent directory (\"..\") not allowed"))));

	archive_path = getenv("PGARCHIVE");
	if (archive_path == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("archive path is not defined"),
				 errhint("Check value of environment variable %s",
						 "PGARCHIVE")));

	filepath = (char *) palloc(MAXPGPATH);
	snprintf(filepath, MAXPGPATH, "%s/%s", archive_path, filename);

	/* length can change here */
	canonicalize_path(filepath);

	return filepath;
}

/*
 * archive_get_size
 *
 * Look at a file in the archives whose path is defined by the environment
 * variable PGARCHIVE and get its size. This is useful when combined with
 * archive_get_data to evaluate a set of chunks to be used during any
 * data transfer from the archives.
 */
Datum
archive_get_size(PG_FUNCTION_ARGS)
{
	char	   *filename = text_to_cstring(PG_GETARG_TEXT_PP(0));
	char	   *filepath;
	struct stat fst;

	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 (errmsg("must be superuser to read files"))));

	filepath = check_and_build_filepath(filename);
	pfree(filename);

	/* get needed information about the file */
	if (stat(filepath, &fst) < 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not stat file \"%s\": %m", filepath)));

	PG_RETURN_INT64((int64) fst.st_size);
}

/*
 * archive_get_data
 *
 * Read a portion of data in an archive folder defined by PGARCHIVE, and
 * return it as bytea. If bytes_to_read is negative or higher than the
 * file's size, read the whole file.
 *
 * Even if data is returned in binary format, it is always possible to
 * convert it to text using encode(data, 'escape'), which is recommended
 * way of doing for small files like timeline history files, so we don't
 * bother having a text version of this function.
 */
Datum
archive_get_data(PG_FUNCTION_ARGS)
{
	char	   *filename = text_to_cstring(PG_GETARG_TEXT_PP(0));
	char	   *filepath;
	int64		seek_offset = 0;
	int64		bytes_to_read = -1;
	bytea	   *result;
	size_t		nbytes;
	FILE	   *file;

	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 (errmsg("must be superuser to read files"))));

	seek_offset = PG_GETARG_INT64(1);
	bytes_to_read = PG_GETARG_INT64(2);

	if (bytes_to_read < 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("requested length cannot be negative")));

	filepath = check_and_build_filepath(filename);
	pfree(filename);

	/*
	 * Read the file, the whole file is read if bytes_to_read is
	 * negative.
	 */
	if (bytes_to_read < 0)
	{
		if (seek_offset < 0)
			bytes_to_read = -seek_offset;
		else
		{
			struct stat fst;

			if (stat(filepath, &fst) < 0)
				ereport(ERROR,
						(errcode_for_file_access(),
						 errmsg("could not stat file \"%s\": %m", filepath)));

			bytes_to_read = fst.st_size - seek_offset;
		}
	}

	/* not sure why anyone thought that int64 length was a good idea */
	if (bytes_to_read > (MaxAllocSize - VARHDRSZ))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("requested length too large")));

	if ((file = AllocateFile(filepath, PG_BINARY_R)) == NULL)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not open file \"%s\" for reading: %m",
						filepath)));

	if (fseeko(file, (off_t) seek_offset,
			   (seek_offset >= 0) ? SEEK_SET : SEEK_END) != 0)
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not seek in file \"%s\": %m", filepath)));

	result = (bytea *) palloc((Size) bytes_to_read + VARHDRSZ);

	nbytes = fread(VARDATA(result), 1, (size_t) bytes_to_read, file);

	if (ferror(file))
		ereport(ERROR,
				(errcode_for_file_access(),
				 errmsg("could not read file \"%s\": %m", filepath)));

	SET_VARSIZE(result, nbytes + VARHDRSZ);

	FreeFile(file);

	PG_RETURN_BYTEA_P(result);
}
