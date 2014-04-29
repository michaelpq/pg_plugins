/*-------------------------------------------------------------------------
 *
 * decoder_raw.c
 *		Logical decoding output plugin generating SQL queries based
 *		on things decoded.
 *
 * Copyright (c) 2012-2014, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  decoder_raw/decoder_raw.c
 *
 *-------------------------------------------------------------------------
 */
#include "postgres.h"

#include "access/sysattr.h"

#include "catalog/pg_class.h"
#include "catalog/pg_type.h"

#include "nodes/parsenodes.h"

#include "replication/output_plugin.h"
#include "replication/logical.h"

#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/memutils.h"
#include "utils/rel.h"
#include "utils/relcache.h"
#include "utils/syscache.h"
#include "utils/typcache.h"


PG_MODULE_MAGIC;

/* These must be available to pg_dlsym() */
extern void		_PG_init(void);
extern void		_PG_output_plugin_init(OutputPluginCallbacks *cb);

/*
 * Structure storing the plugin specifications and options.
 */
typedef struct
{
	MemoryContext context;
	bool		include_transaction;
} TestDecodingData;

static void decoder_raw_startup(LogicalDecodingContext *ctx, OutputPluginOptions *opt,
							  bool is_init);
static void decoder_raw_shutdown(LogicalDecodingContext *ctx);
static void decoder_raw_begin_txn(LogicalDecodingContext *ctx,
					ReorderBufferTXN *txn);
static void decoder_raw_commit_txn(LogicalDecodingContext *ctx,
					 ReorderBufferTXN *txn, XLogRecPtr commit_lsn);
static void decoder_raw_change(LogicalDecodingContext *ctx,
				 ReorderBufferTXN *txn, Relation rel,
				 ReorderBufferChange *change);

void
_PG_init(void)
{
	/* other plugins can perform things here */
}

/* specify output plugin callbacks */
void
_PG_output_plugin_init(OutputPluginCallbacks *cb)
{
	AssertVariableIsOfType(&_PG_output_plugin_init, LogicalOutputPluginInit);

	cb->startup_cb = decoder_raw_startup;
	cb->begin_cb = decoder_raw_begin_txn;
	cb->change_cb = decoder_raw_change;
	cb->commit_cb = decoder_raw_commit_txn;
	cb->shutdown_cb = decoder_raw_shutdown;
}


/* initialize this plugin */
static void
decoder_raw_startup(LogicalDecodingContext *ctx, OutputPluginOptions *opt,
				  bool is_init)
{
	ListCell   *option;
	TestDecodingData *data;

	data = palloc(sizeof(TestDecodingData));
	data->context = AllocSetContextCreate(ctx->context,
										  "Raw decoder context",
										  ALLOCSET_DEFAULT_MINSIZE,
										  ALLOCSET_DEFAULT_INITSIZE,
										  ALLOCSET_DEFAULT_MAXSIZE);
	data->include_transaction = false;

	ctx->output_plugin_private = data;

	opt->output_type = OUTPUT_PLUGIN_TEXTUAL_OUTPUT;

	foreach(option, ctx->output_plugin_options)
	{
		DefElem    *elem = lfirst(option);

		Assert(elem->arg == NULL || IsA(elem->arg, String));

		if (strcmp(elem->defname, "include-transaction") == 0)
		{
			/* if option does not provide a value, it means its value is true */
			if (elem->arg == NULL)
				data->include_transaction = true;
			else if (!parse_bool(strVal(elem->arg), &data->include_transaction))
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("could not parse value \"%s\" for parameter \"%s\"",
								strVal(elem->arg), elem->defname)));
		}
		else
		{
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 errmsg("option \"%s\" = \"%s\" is unknown",
							elem->defname,
							elem->arg ? strVal(elem->arg) : "(null)")));
		}
	}
}

/* cleanup this plugin's resources */
static void
decoder_raw_shutdown(LogicalDecodingContext *ctx)
{
	TestDecodingData *data = ctx->output_plugin_private;

	/* cleanup our own resources via memory context reset */
	MemoryContextDelete(data->context);
}

/* BEGIN callback */
static void
decoder_raw_begin_txn(LogicalDecodingContext *ctx, ReorderBufferTXN *txn)
{
	TestDecodingData *data = ctx->output_plugin_private;

	OutputPluginPrepareWrite(ctx, true);
	if (data->include_transaction)
		appendStringInfoString(ctx->out, "BEGIN;");
	OutputPluginWrite(ctx, true);
}

/* COMMIT callback */
static void
decoder_raw_commit_txn(LogicalDecodingContext *ctx, ReorderBufferTXN *txn,
					 XLogRecPtr commit_lsn)
{
	TestDecodingData *data = ctx->output_plugin_private;

	OutputPluginPrepareWrite(ctx, true);
	if (data->include_transaction)
		appendStringInfoString(ctx->out, "COMMIT;");
	OutputPluginWrite(ctx, true);
}

/*
 * Print literal `outputstr' already represented as string of type `typid'
 * into stringbuf `s'.
 *
 * Some builtin types aren't quoted, the rest is quoted. Escaping is done as
 * if standard_conforming_strings were enabled.
 */
static void
print_literal(StringInfo s, Oid typid, char *outputstr)
{
	const char *valptr;

	switch (typid)
	{
		case BOOLOID:
		case INT2OID:
		case INT4OID:
		case INT8OID:
		case OIDOID:
		case FLOAT4OID:
		case FLOAT8OID:
		case NUMERICOID:
			/* NB: We don't care about Inf, NaN et al. */
			appendStringInfoString(s, outputstr);
			break;

		case BITOID:
		case VARBITOID:
			appendStringInfo(s, "B'%s'", outputstr);
			break;

		default:
			appendStringInfoChar(s, '\'');
			for (valptr = outputstr; *valptr; valptr++)
			{
				char		ch = *valptr;

				if (SQL_STR_DOUBLE(ch, false))
					appendStringInfoChar(s, ch);
				appendStringInfoChar(s, ch);
			}
			appendStringInfoChar(s, '\'');
			break;
	}
}

/*
 * Print a relation name into the StringInfo provided by caller.
 */
static void
print_relname(StringInfo s, Relation rel)
{
	Form_pg_class	class_form = RelationGetForm(rel);

	appendStringInfoString(s,
		quote_qualified_identifier(
				get_namespace_name(
						   get_rel_namespace(RelationGetRelid(rel))),
			NameStr(class_form->relname)));
}

/*
 * Print a value into the StringInfo provided by caller.
 */
static void
print_value(StringInfo s, Datum origval, Oid typid, bool isnull)
{
	Oid					typoutput;
	bool				typisvarlena;

	/* Query output function */
	getTypeOutputInfo(typid,
					  &typoutput, &typisvarlena);

	/* Print value */
	if (isnull)
		appendStringInfoString(s, "null");
	else if (typisvarlena && VARATT_IS_EXTERNAL_ONDISK(origval))
		appendStringInfoString(s, "unchanged-toast-datum");
	else if (!typisvarlena)
		print_literal(s, typid,
					  OidOutputFunctionCall(typoutput, origval));
	else
	{
		/* Definitely detoasted Datum */
		Datum		val;
		val = PointerGetDatum(PG_DETOAST_DATUM(origval));
		print_literal(s, typid, OidOutputFunctionCall(typoutput, val));
	}
}

/*
 * Decode an INSERT entry
 */
static void
decoder_raw_insert(StringInfo s,
				   Relation relation,
				   HeapTuple tuple)
{
	TupleDesc		tupdesc = RelationGetDescr(relation);
	int				natt;
	bool			first_column = true;
	StringInfo		values = makeStringInfo();

	/* Initialize string info for values */
	initStringInfo(values);

	/* Query header */
	appendStringInfo(s, "INSERT INTO ");
	print_relname(s, relation);
	appendStringInfo(s, " (");

	/* Build column names and values */
	for (natt = 0; natt < tupdesc->natts; natt++)
	{
		Form_pg_attribute	attr;
		Datum				origval;
		bool				isnull;

		attr = tupdesc->attrs[natt];

		/* Skip dropped columns and system columns */
		if (attr->attisdropped || attr->attnum < 0)
			continue;

		/* Skip comma for first colums */
		if (!first_column)
		{
			appendStringInfoString(s, ", ");
			appendStringInfoString(values, ", ");
		}
		else
			first_column = false;

		/* Print attribute name */
		appendStringInfo(s, "%s", quote_identifier(NameStr(attr->attname)));

		/* Get Datum from tuple */
		origval = fastgetattr(tuple, natt + 1, tupdesc, &isnull);

		/* Get output function */
		print_value(values, origval, attr->atttypid, isnull);
	}

	/* Append values  */
	appendStringInfo(s, ") VALUES (%s);", values->data);

	/* Clean up */
	resetStringInfo(values);
}

/*
 * Decode a DELETE entry
 */
static void
decoder_raw_delete(StringInfo s,
				   Relation relation,
				   HeapTuple tuple)
{
	TupleDesc		tupdesc = RelationGetDescr(relation);
	int				natt;
	bool			first_column = true;

	appendStringInfo(s, "DELETE FROM ");
	print_relname(s, relation);
	appendStringInfo(s, " WHERE ");

	/* Append values and column names */
	for (natt = 0; natt < tupdesc->natts; natt++)
	{
		Form_pg_attribute	attr;
		Datum				origval;
		bool				isnull;

		attr = tupdesc->attrs[natt];

		/* Skip dropped columns and system columns */
		if (attr->attisdropped || attr->attnum < 0)
			continue;

		/* Skip comma for first colums */
		if (!first_column)
		{
			appendStringInfoString(s, "AND ");
		}
		else
			first_column = false;

		/* Print attribute name */
		appendStringInfo(s, "%s = ", quote_identifier(NameStr(attr->attname)));

		/* Get Datum from tuple */
		origval = fastgetattr(tuple, natt + 1, tupdesc, &isnull);

		/* Get output function */
		print_value(s, origval, attr->atttypid, isnull);
		appendStringInfoString(s, " ");
	}

	appendStringInfoString(s, ";");
}


/*
 * Decode an UPDATE entry
 */
static void
decoder_raw_update(StringInfo s,
				   Relation relation,
				   HeapTuple oldtuple,
				   HeapTuple newtuple)
{
	TupleDesc		tupdesc = RelationGetDescr(relation);
	int				natt;
	bool			first_column = true;

	/* If there are no new values, simply leave as there is nothing to do */
	if (newtuple == NULL)
		return;

	appendStringInfo(s, "UPDATE ");
	print_relname(s, relation);

	/* Build the SET clause with the new values */
	appendStringInfo(s, " SET ");
	for (natt = 0; natt < tupdesc->natts; natt++)
	{
		Form_pg_attribute	attr;
		Datum				origval;
		bool				isnull;

		attr = tupdesc->attrs[natt];

		/* Skip dropped columns and system columns */
		if (attr->attisdropped || attr->attnum < 0)
			continue;

		/* Skip comma for first colums */
		if (!first_column)
		{
			appendStringInfoString(s, ", ");
		}
		else
			first_column = false;

		/* Print attribute name */
		appendStringInfo(s, "%s = ", quote_identifier(NameStr(attr->attname)));

		/* Get Datum from tuple */
		origval = fastgetattr(newtuple, natt + 1, tupdesc, &isnull);

		/* Get output function */
		print_value(s, origval, attr->atttypid, isnull);
	}

	/*
	 * If there are no old value, we're basically done. User should
	 * be aware that the selectivity of tuples can be compromised
	 * severely if REPLICA IDENTITY is very laxist but this is not
	 * the problem of this plugin.
	 */
	if (oldtuple == NULL)
	{
		appendStringInfoString(s, ";");
		return;
	}

	/* Build the WHERE clause */
	first_column = true;
	appendStringInfoString(s, " WHERE ");
	for (natt = 0; natt < tupdesc->natts; natt++)
	{
		Form_pg_attribute	attr;
		Datum				origval;
		bool				isnull;

		attr = tupdesc->attrs[natt];

		/* Skip dropped columns and system columns */
		if (attr->attisdropped || attr->attnum < 0)
			continue;

		/* Skip comma for first colums */
		if (!first_column)
		{
			appendStringInfoString(s, "AND ");
		}
		else
			first_column = false;

		/* Print attribute name */
		appendStringInfo(s, "%s = ", quote_identifier(NameStr(attr->attname)));

		/* Get Datum from tuple */
		origval = fastgetattr(oldtuple, natt + 1, tupdesc, &isnull);

		/* Get output function */
		print_value(s, origval, attr->atttypid, isnull);
		appendStringInfoString(s, " ");
	}
	appendStringInfoString(s, ";");
}

/*
 * Callback for individual changed tuples
 */
static void
decoder_raw_change(LogicalDecodingContext *ctx, ReorderBufferTXN *txn,
				 Relation relation, ReorderBufferChange *change)
{
	TestDecodingData *data;
	MemoryContext old;

	data = ctx->output_plugin_private;

	/* Avoid leaking memory by using and resetting our own context */
	old = MemoryContextSwitchTo(data->context);

	OutputPluginPrepareWrite(ctx, true);

	/* Decode entry depending on its type */
	switch (change->action)
	{
		case REORDER_BUFFER_CHANGE_INSERT:
			if (change->data.tp.newtuple != NULL)
				decoder_raw_insert(ctx->out,
								   relation,
								   &change->data.tp.newtuple->tuple);
			break;
		case REORDER_BUFFER_CHANGE_UPDATE:
			if (change->data.tp.newtuple != NULL ||
				change->data.tp.oldtuple != NULL)
			{
				HeapTuple oldtuple = change->data.tp.oldtuple != NULL ?
					&change->data.tp.oldtuple->tuple : NULL;
				HeapTuple newtuple = change->data.tp.newtuple != NULL ?
					&change->data.tp.newtuple->tuple : NULL;

				decoder_raw_update(ctx->out,
								   relation,
								   oldtuple,
								   newtuple);
			}
			break;
		case REORDER_BUFFER_CHANGE_DELETE:
			if (change->data.tp.oldtuple != NULL)
				decoder_raw_delete(ctx->out,
								   relation,
								   &change->data.tp.oldtuple->tuple);
			break;
		default:
			/* Should not come here */
			Assert(0);
			break;
	}

	MemoryContextSwitchTo(old);
	MemoryContextReset(data->context);
	OutputPluginWrite(ctx, true);
}
