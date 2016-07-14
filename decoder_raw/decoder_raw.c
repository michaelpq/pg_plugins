/*-------------------------------------------------------------------------
 *
 * decoder_raw.c
 *		Logical decoding output plugin generating SQL queries based
 *		on things decoded.
 *
 * Copyright (c) 1996-2016, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  decoder_raw/decoder_raw.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/genam.h"
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
} DecoderRawData;

static void decoder_raw_startup(LogicalDecodingContext *ctx,
								OutputPluginOptions *opt,
								bool is_init);
static void decoder_raw_shutdown(LogicalDecodingContext *ctx);
static void decoder_raw_begin_txn(LogicalDecodingContext *ctx,
								  ReorderBufferTXN *txn);
static void decoder_raw_commit_txn(LogicalDecodingContext *ctx,
								   ReorderBufferTXN *txn,
								   XLogRecPtr commit_lsn);
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
	DecoderRawData *data;

	data = palloc(sizeof(DecoderRawData));
	data->context = AllocSetContextCreate(ctx->context,
										  "Raw decoder context",
										  ALLOCSET_DEFAULT_MINSIZE,
										  ALLOCSET_DEFAULT_INITSIZE,
										  ALLOCSET_DEFAULT_MAXSIZE);
	data->include_transaction = false;

	ctx->output_plugin_private = data;

	/* Default output format */
	opt->output_type = OUTPUT_PLUGIN_TEXTUAL_OUTPUT;

	foreach(option, ctx->output_plugin_options)
	{
		DefElem    *elem = lfirst(option);

		Assert(elem->arg == NULL || IsA(elem->arg, String));

		if (strcmp(elem->defname, "include_transaction") == 0)
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
		else if (strcmp(elem->defname, "output_format") == 0)
		{
			char	   *format = NULL;

			if (elem->arg == NULL)
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("No value specified for parameter \"%s\"",
								elem->defname)));

			format = strVal(elem->arg);

			if (strcmp(format, "textual") == 0)
				opt->output_type = OUTPUT_PLUGIN_TEXTUAL_OUTPUT;
			else if (strcmp(format, "binary") == 0)
				opt->output_type = OUTPUT_PLUGIN_BINARY_OUTPUT;
			else
				ereport(ERROR,
						(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
						 errmsg("Incorrect value \"%s\" for parameter \"%s\"",
								format, elem->defname)));
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
	DecoderRawData *data = ctx->output_plugin_private;

	/* cleanup our own resources via memory context reset */
	MemoryContextDelete(data->context);
}

/* BEGIN callback */
static void
decoder_raw_begin_txn(LogicalDecodingContext *ctx, ReorderBufferTXN *txn)
{
	DecoderRawData *data = ctx->output_plugin_private;

	/* Write to the plugin only if there is */
	if (data->include_transaction)
	{
		OutputPluginPrepareWrite(ctx, true);
		appendStringInfoString(ctx->out, "BEGIN;");
		OutputPluginWrite(ctx, true);
	}
}

/* COMMIT callback */
static void
decoder_raw_commit_txn(LogicalDecodingContext *ctx, ReorderBufferTXN *txn,
					 XLogRecPtr commit_lsn)
{
	DecoderRawData *data = ctx->output_plugin_private;

	/* Write to the plugin only if there is */
	if (data->include_transaction)
	{
		OutputPluginPrepareWrite(ctx, true);
		appendStringInfoString(ctx->out, "COMMIT;");
		OutputPluginWrite(ctx, true);
	}
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
			if (outputstr[0] == 't')
				appendStringInfoString(s, "true");
			else
				appendStringInfoString(s, "false");
			break;

		case INT2OID:
		case INT4OID:
		case INT8OID:
		case OIDOID:
			/* NB: We don't care about Inf, NaN et al. */
			appendStringInfoString(s, outputstr);
			break;
		case FLOAT4OID:
		case FLOAT8OID:
		case NUMERICOID:
			/*
			 * Numeric can have NaN. Float can have Nan, Infinity and
			 * -Infinity. These need to be quoted.
			 */
			if (strcmp(outputstr, "NaN") == 0 ||
				strcmp(outputstr, "Infinity") == 0 ||
				strcmp(outputstr, "-Infinity") == 0)
				appendStringInfo(s, "'%s'", outputstr);
			else
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
 * Print a WHERE clause item
 */
static void
print_where_clause_item(StringInfo s,
						Relation relation,
						HeapTuple tuple,
						int natt,
						bool *first_column)
{
	Form_pg_attribute	attr;
	Datum				origval;
	bool				isnull;
	TupleDesc			tupdesc = RelationGetDescr(relation);

	attr = tupdesc->attrs[natt - 1];

	/* Skip dropped columns and system columns */
	if (attr->attisdropped || attr->attnum < 0)
		return;

	/* Skip comma for first colums */
	if (!*first_column)
		appendStringInfoString(s, " AND ");
	else
		*first_column = false;

	/* Print attribute name */
	appendStringInfo(s, "%s = ", quote_identifier(NameStr(attr->attname)));

	/* Get Datum from tuple */
	origval = heap_getattr(tuple, natt, tupdesc, &isnull);

	/* Get output function */
	print_value(s, origval, attr->atttypid, isnull);
}

/*
 * Generate a WHERE clause for UPDATE or DELETE.
 */
static void
print_where_clause(StringInfo s,
				   Relation relation,
				   HeapTuple oldtuple,
				   HeapTuple newtuple)
{
	TupleDesc		tupdesc = RelationGetDescr(relation);
	int				natt;
	bool			first_column = true;

	Assert(relation->rd_rel->relreplident == REPLICA_IDENTITY_DEFAULT ||
		   relation->rd_rel->relreplident == REPLICA_IDENTITY_FULL ||
		   relation->rd_rel->relreplident == REPLICA_IDENTITY_INDEX);

	/* Build the WHERE clause */
	appendStringInfoString(s, " WHERE ");

	RelationGetIndexList(relation);
	/* Generate WHERE clause using new values of REPLICA IDENTITY */
	if (OidIsValid(relation->rd_replidindex))
	{
		Relation    indexRel;
		int			key;

		/* Use all the values associated with the index */
		indexRel = index_open(relation->rd_replidindex, ShareLock);
		for (key = 0; key < indexRel->rd_index->indnatts; key++)
		{
			int	relattr = indexRel->rd_index->indkey.values[key];

			/*
			 * For a relation having REPLICA IDENTITY set at DEFAULT
			 * or INDEX, if one of the columns used for tuple selectivity
			 * is changed, the old tuple data is not NULL and need to
			 * be used for tuple selectivity. If no such columns are
			 * updated, old tuple data is NULL.
			 */
			print_where_clause_item(s, relation,
									oldtuple ? oldtuple : newtuple,
									relattr, &first_column);
		}
		index_close(indexRel, NoLock);
		return;
	}

	/* We need absolutely some values for tuple selectivity now */
	Assert(oldtuple != NULL &&
		   relation->rd_rel->relreplident == REPLICA_IDENTITY_FULL);

	/*
	 * Fallback to default case, use of old values and print WHERE clause
	 * using all the columns. This is actually the code path for FULL.
	 */
	for (natt = 0; natt < tupdesc->natts; natt++)
		print_where_clause_item(s, relation, oldtuple,
								natt + 1, &first_column);
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
		origval = heap_getattr(tuple, natt + 1, tupdesc, &isnull);

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
	appendStringInfo(s, "DELETE FROM ");
	print_relname(s, relation);

	/*
	 * Here the same tuple is used as old and new values, selectivity will
	 * be properly reduced by relation uses DEFAULT or INDEX as REPLICA
	 * IDENTITY.
	 */
	print_where_clause(s, relation, tuple, tuple);
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
		Oid					typoutput;
		bool				typisvarlena;

		attr = tupdesc->attrs[natt];

		/* Skip dropped columns and system columns */
		if (attr->attisdropped || attr->attnum < 0)
			continue;

		/* Get Datum from tuple */
		origval = heap_getattr(newtuple, natt + 1, tupdesc, &isnull);

		/* Get output type so we can know if it's varlena */
		getTypeOutputInfo(attr->atttypid,
						  &typoutput, &typisvarlena);

		/*
		 * TOASTed datum, but it's not changed. So we can skip this in
		 * the UPDATE.
		 */
		if (!isnull && typisvarlena && VARATT_IS_EXTERNAL_ONDISK(origval))
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

		/* Get output function */
		print_value(s, origval, attr->atttypid, isnull);
	}

	/* Print WHERE clause */
	print_where_clause(s, relation, oldtuple, newtuple);

	appendStringInfoString(s, ";");
}

/*
 * Callback for individual changed tuples
 */
static void
decoder_raw_change(LogicalDecodingContext *ctx, ReorderBufferTXN *txn,
				 Relation relation, ReorderBufferChange *change)
{
	DecoderRawData *data;
	MemoryContext	old;
	char			replident = relation->rd_rel->relreplident;
	bool			is_rel_non_selective;

	data = ctx->output_plugin_private;

	/* Avoid leaking memory by using and resetting our own context */
	old = MemoryContextSwitchTo(data->context);

	/*
	 * Determine if relation is selective enough for WHERE clause generation
	 * in UPDATE and DELETE cases. A non-selective relation uses REPLICA
	 * IDENTITY set as NOTHING, or DEFAULT without an available replica
	 * identity index.
	 */
	RelationGetIndexList(relation);
	is_rel_non_selective = (replident == REPLICA_IDENTITY_NOTHING ||
							(replident == REPLICA_IDENTITY_DEFAULT &&
							 !OidIsValid(relation->rd_replidindex)));

	/* Decode entry depending on its type */
	switch (change->action)
	{
		case REORDER_BUFFER_CHANGE_INSERT:
			if (change->data.tp.newtuple != NULL)
			{
				OutputPluginPrepareWrite(ctx, true);
				decoder_raw_insert(ctx->out,
								   relation,
								   &change->data.tp.newtuple->tuple);
				OutputPluginWrite(ctx, true);
			}
			break;
		case REORDER_BUFFER_CHANGE_UPDATE:
			if (!is_rel_non_selective)
			{
				HeapTuple oldtuple = change->data.tp.oldtuple != NULL ?
					&change->data.tp.oldtuple->tuple : NULL;
				HeapTuple newtuple = change->data.tp.newtuple != NULL ?
					&change->data.tp.newtuple->tuple : NULL;

				OutputPluginPrepareWrite(ctx, true);
				decoder_raw_update(ctx->out,
								   relation,
								   oldtuple,
								   newtuple);
				OutputPluginWrite(ctx, true);
			}
			break;
		case REORDER_BUFFER_CHANGE_DELETE:
			if (!is_rel_non_selective)
			{
				OutputPluginPrepareWrite(ctx, true);
				decoder_raw_delete(ctx->out,
								   relation,
								   &change->data.tp.oldtuple->tuple);
				OutputPluginWrite(ctx, true);
			}
			break;
		default:
			/* Should not come here */
			Assert(0);
			break;
	}

	MemoryContextSwitchTo(old);
	MemoryContextReset(data->context);
}
