/*--------------------------------------------------------------------------
 *
 * signal_rmgr.c
 *		Send signals to standbys through WAL.
 *
 * Portions Copyright (c) 1996-2023, PostgreSQL Global Development Group
 * Portions Copyright (c) 1994, Regents of the University of California
 *
 * IDENTIFICATION
 *		signal_smgr/signal_smgr.c
 *
 * The logic of the module is quite simple, so feel free to use it as a
 * template.  Note that upstream has an in-core module used for the purpose
 * of testing custom rmgrs in src/test/modules/test_custom_rgmrs/.
 *
 * -------------------------------------------------------------------------
 */

#include "postgres.h"

#include <signal.h>

#include "access/xlog.h"
#include "access/xlog_internal.h"
#include "access/xloginsert.h"
#include "fmgr.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/pg_lsn.h"
#include "varatt.h"

PG_MODULE_MAGIC;

/*
 * signal_rmgr WAL records.
 */
typedef struct xl_signal_smgr
{
	int			signal;
	Size		reason_size;	/* size of the reason given for signal */
	char		reason[FLEXIBLE_ARRAY_MEMBER];	/* payload */
}			xl_signal_rmgr;

#define SizeOfSignalRmgr	(offsetof(xl_signal_rmgr, reason))
#define XLOG_SIGNAL_RMGR	0x00

/*
 * While developing or testing, use RM_EXPERIMENTAL_ID for rmid. For a real
 * extension, reserve a new resource manager ID to avoid conflicting with
 * other extensions; see:
 * https://wiki.postgresql.org/wiki/CustomWALResourceManagers
 *
 * Note that this conflicts with upstream's test_custom_rmgrs.
 */
#define RM_SIGNAL_RMGR_ID			RM_EXPERIMENTAL_ID
#define SIGNAL_RMGR_NAME			"signal_rmgr"

/* RMGR API, see xlog_internal.h */
void		signal_rmgr_redo(XLogReaderState *record);
void		signal_rmgr_desc(StringInfo buf, XLogReaderState *record);
const char *signal_rmgr_identify(uint8 info);

static const RmgrData signal_rmgr_rmgr = {
	.rm_name = SIGNAL_RMGR_NAME,
	.rm_redo = signal_rmgr_redo,
	.rm_desc = signal_rmgr_desc,
	.rm_identify = signal_rmgr_identify
};

static void
check_signal_value(int signal)
{
	switch (signal)
	{
			/* Authorized signals */
		case SIGKILL:
		case SIGHUP:
		case SIGINT:
		case SIGTERM:
			break;
		default:
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("cannot support signal %d", signal)));
			break;
	}
}

/* RMGR API implementation */

/*
 * Execute the given signal on redo, make sure that it is valid, though.
 * The reason for the signal is logged.
 */
void
signal_rmgr_redo(XLogReaderState *record)
{
	uint8		info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;
	xl_signal_rmgr *xlrec;

	if (info != XLOG_SIGNAL_RMGR)
		elog(PANIC, "signal_rmgr_redo: unknown op code %u", info);

	xlrec = (xl_signal_rmgr *) XLogRecGetData(record);

	check_signal_value(xlrec->signal);

	elog(LOG, "signal_rmgr_redo: signal %d, reason %s",
		 xlrec->signal, xlrec->reason);

	/* Everything is fine, so signal the postmaster */
	if (kill(PostmasterPid, xlrec->signal))
	{
		ereport(WARNING,
				(errmsg("could not send signal %d (%s) to postmaster (%d): %m",
						xlrec->signal, xlrec->reason, PostmasterPid)));
	}
	else
	{
		ereport(LOG,
				(errmsg("sent signal %d (%s) to postmaster (%d)",
						xlrec->signal, xlrec->reason, PostmasterPid)));
	}
}

void
signal_rmgr_desc(StringInfo buf, XLogReaderState *record)
{
	char	   *rec = XLogRecGetData(record);
	uint8		info = XLogRecGetInfo(record) & ~XLR_INFO_MASK;

	if (info == XLOG_SIGNAL_RMGR)
	{
		xl_signal_rmgr *xlrec = (xl_signal_rmgr *) rec;

		appendStringInfo(buf, "signal %d; ", xlrec->signal);
		appendStringInfo(buf, "reason %s (%zu bytes)",
						 xlrec->reason, xlrec->reason_size);
	}
}

const char *
signal_rmgr_identify(uint8 info)
{
	if ((info & ~XLR_INFO_MASK) == XLOG_SIGNAL_RMGR)
		return "XLOG_SIGNAL_RMGR";

	return NULL;
}

/*
 * SQL function for sending a signal to a standby using custom WAL
 * resource managers.
 */
PG_FUNCTION_INFO_V1(signal_rmgr);
Datum
signal_rmgr(PG_FUNCTION_ARGS)
{
	int			signal = PG_GETARG_INT32(0);
	text	   *arg = PG_GETARG_TEXT_PP(1);
	char	   *reason = text_to_cstring(arg);
	XLogRecPtr	lsn;
	xl_signal_rmgr xlrec;

	check_signal_value(signal);

	xlrec.signal = signal;
	xlrec.reason_size = strlen(reason) + 1;

	XLogBeginInsert();
	XLogRegisterData((char *) &xlrec, SizeOfSignalRmgr);
	XLogRegisterData(reason, strlen(reason) + 1);

	/* Let's mark this record as unimportant, just in case. */
	XLogSetRecordFlags(XLOG_MARK_UNIMPORTANT);

	lsn = XLogInsert(RM_SIGNAL_RMGR_ID, XLOG_SIGNAL_RMGR);

	PG_RETURN_LSN(lsn);
}

/*
 * Module load callback
 */
void
_PG_init(void)
{
	/*
	 * A WAL resource manager has to be loaded with shared_preload_libraries.
	 */
	RegisterCustomRmgr(RM_SIGNAL_RMGR_ID, &signal_rmgr_rmgr);
}
