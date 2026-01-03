/*-------------------------------------------------------------------------
 *
 * custom_wal.c
 *		SQL function to generate custom WAL records.
 *
 * Copyright (c) 1996-2026, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  custom_wal/custom_wal.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"

#include "replication/message.h"

PG_MODULE_MAGIC;

/*
 * Wrapper function on top of LogLogicalMessage() to generate custom
 * WAL records.  This uses two parameters: the number of records to
 * generate and the record size.  A flush is enforced once after all
 * the records are generated.
 */
PG_FUNCTION_INFO_V1(custom_wal);
Datum
custom_wal(PG_FUNCTION_ARGS)
{
	int			record_size = PG_GETARG_INT32(0);
	int			record_number = PG_GETARG_INT32(1);
	char	   *message;
	int			count;
	XLogRecPtr	lsn;

	message = palloc0(sizeof(char) * record_size);
	for (count = 0; count < record_size; count++)
		message[count] = 'a';

	/* Loop and generate the number of records wanted */
	for (count = 0; count < record_number; count++)
	{
		/* The first argument is a prefix, keep it minimal */
		lsn = LogLogicalMessage("", message, record_size, false, false);
	}

	XLogFlush(lsn);

	pfree(message);
	PG_RETURN_VOID();
}
