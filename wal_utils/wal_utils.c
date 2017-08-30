/*-------------------------------------------------------------------------
 *
 * wal_utils.c
 *		Set of tools and utilities for handling of WAL-related data.
 *
 * Copyright (c) 1996-2017, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  wal_utils/wal_utils.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"

#include "access/timeline.h"
#include "access/xlog_internal.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/pg_lsn.h"

PG_MODULE_MAGIC;

static List *parseTimeLineHistory(char *buffer);

/*
 * Set of callable functions.
 */
PG_FUNCTION_INFO_V1(parse_wal_history);

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
 * parse_wal_history
 *
 * Parse input buffer of a history file and build a set of rows to
 * give a SQL representation of TimeLineHistoryEntry entries part
 * of a timeline history file.
 */
Datum
parse_wal_history(PG_FUNCTION_ARGS)
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
		TimeLineHistoryEntry *history = lfirst(entry);

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
