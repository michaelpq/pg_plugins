/*-------------------------------------------------------------------------
 *
 * jsonlog.c
 *		Facility using hook controlling logging output of a Postgres
 *		able to generate JSON logs
 *
 * Copyright (c) 2013-2014, Michael Paquier
 * Copyright (c) 1996-2014, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		jsonlog.c/jsonlog.c
 *
 *-------------------------------------------------------------------------
 */

#include <unistd.h>

#include "postgres.h"

#include "fmgr.h"
#include "miscadmin.h"
#include "access/xact.h"
#include "lib/stringinfo.h"
#include "postmaster/syslogger.h"
#include "utils/elog.h"
#include "utils/json.h"

/* Allow load of this module in shared libs */
PG_MODULE_MAGIC;

void _PG_init(void);
void _PG_fini(void);

static emit_log_hook_type prev_log_hook = NULL;

static const char *error_severity(int elevel);
static void write_jsonlog(ErrorData *edata);

/*
 * error_severity
 * Print string showing error severity based on integer level.
 * Taken from elog.c.
 */
static const char *
error_severity(int elevel)
{
	const char *prefix;

	switch (elevel)
	{
		case DEBUG1:
		case DEBUG2:
		case DEBUG3:
		case DEBUG4:
		case DEBUG5:
			prefix = _("DEBUG");
			break;
		case LOG:
		case COMMERROR:
			prefix = _("LOG");
			break;
		case INFO:
			prefix = _("INFO");
			break;
		case NOTICE:
			prefix = _("NOTICE");
			break;
		case WARNING:
			prefix = _("WARNING");
			break;
		case ERROR:
			prefix = _("ERROR");
			break;
		case FATAL:
			prefix = _("FATAL");
			break;
		case PANIC:
			prefix = _("PANIC");
			break;
		default:
			prefix = "???";
			break;
	}

	return prefix;
}

/*
 * write_pipe_chunks
 * Send data to the syslogger using the chunked protocol. Taken from
 * elog.c.
 */
static void
write_pipe_chunks(char *data, int len, int dest)
{
	PipeProtoChunk p;
	int		 fd = fileno(stderr);
	int		 rc;

	Assert(len > 0);

	p.proto.nuls[0] = p.proto.nuls[1] = '\0';
	p.proto.pid = MyProcPid;

	/* write all but the last chunk */
	while (len > PIPE_MAX_PAYLOAD)
	{
		p.proto.is_last = (dest == LOG_DESTINATION_CSVLOG ? 'F' : 'f');
		p.proto.len = PIPE_MAX_PAYLOAD;
		memcpy(p.proto.data, data, PIPE_MAX_PAYLOAD);
		rc = write(fd, &p, PIPE_HEADER_SIZE + PIPE_MAX_PAYLOAD);
		(void) rc;
		data += PIPE_MAX_PAYLOAD;
		len -= PIPE_MAX_PAYLOAD;
	}

	/* write the last chunk */
	p.proto.is_last = (dest == LOG_DESTINATION_CSVLOG ? 'T' : 't');
	p.proto.len = len;
	memcpy(p.proto.data, data, len);
	rc = write(fd, &p, PIPE_HEADER_SIZE + len);
	(void) rc;
}

/*
 * appendJSONLiteral
 * Append to given StringInfo a JSON with a given key and a value
 * not yet made literal.
 */
static void
appendJSONLiteral(StringInfo buf, char *key, char *value)
{
	StringInfoData literal_json;

	initStringInfo(&literal_json);
	Assert(key && value);

	/*
	 * Call in-core function able to generate wanted strings, there is
	 * no need to reinvent the wheel.
	 */
	escape_json(&literal_json, value);

	/* Now append the field */
	appendStringInfo(buf, "\"%s\":%s", key, literal_json.data);

	/* Clean up */
	pfree(literal_json.data);
}

/*
 * write_jsonlog
 * Write logs in json format.
 */
static void
write_jsonlog(ErrorData *edata)
{
	StringInfoData	buf;

	initStringInfo(&buf);

	/* Initialize string */
	appendStringInfoChar(&buf, '{');

	/* Process ID */
	if (MyProcPid != 0)
		appendStringInfo(&buf, "\"pid\":%d,", MyProcPid);

	/* Transaction id */
	appendStringInfo(&buf, "\"txid\":%u", GetTopTransactionIdIfAny());
	appendStringInfoChar(&buf, ',');

	/* Error severity */
	appendJSONLiteral(&buf, "error_severity",
					  (char *) error_severity(edata->elevel));
	appendStringInfoChar(&buf, ',');

	/* SQL state code */
	appendJSONLiteral(&buf, "state_code",
					 unpack_sql_state(edata->sqlerrcode));
	appendStringInfoChar(&buf, ',');

	/* errmessage */
	appendJSONLiteral(&buf, "message", edata->message);

	/* Finish string */
	appendStringInfoChar(&buf, '}');
	appendStringInfoChar(&buf, '\n');

	/* Block logging on server, priority is given to JSON format */
	edata->output_to_server = false;

	/* If in the syslogger process, try to write messages direct to file */
	if (am_syslogger)
		write_syslogger_file(buf.data, buf.len, LOG_DESTINATION_STDERR);
	else
		write_pipe_chunks(buf.data, buf.len, LOG_DESTINATION_STDERR);

	/* Cleanup */
	pfree(buf.data);
}

/*
 * _PG_init
 * Entry point loading hooks
 */
void
_PG_init(void)
{
	prev_log_hook = emit_log_hook;
	emit_log_hook = write_jsonlog;
}

/*
 * _PG_fini
 * Exit point unloading hooks
 */
void
_PG_fini(void)
{
	emit_log_hook = prev_log_hook;
}
