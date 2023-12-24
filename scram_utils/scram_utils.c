/*-------------------------------------------------------------------------
 *
 * scram_utils.c
 *		Set of functions for SCRAM authentication
 *
 * Copyright (c) 1996-2024, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  scram_utils/scram_utils.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"

#include "access/heapam.h"
#include "access/htup.h"
#include "access/htup_details.h"
#include "catalog/indexing.h"
#include "catalog/pg_authid.h"
#include "common/saslprep.h"
#include "common/scram-common.h"
#include "miscadmin.h"
#include "utils/builtins.h"
#include "utils/rel.h"
#include "utils/syscache.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(scram_utils_verifier);

/*
 * scram_utils_verifier
 *
 * Generate a verifier for SCRAM-SHA-256 authentication and update the
 * related user's pg_authid entry as per RFC 7677.
 */
Datum
scram_utils_verifier(PG_FUNCTION_ARGS)
{
	pg_saslprep_rc rc;
	char	   *username = text_to_cstring(PG_GETARG_TEXT_PP(0));
	const char *password = text_to_cstring(PG_GETARG_TEXT_PP(1));
	int			iterations = PG_GETARG_INT32(2);
	int			saltlen = PG_GETARG_INT32(3);
	char	   *prep_password = NULL;
	char	   *saltbuf;
	char	   *verifier;
	HeapTuple	oldtuple,
				newtuple;
	TupleDesc	dsc;
	Relation	rel;
	Datum		repl_val[Natts_pg_authid];
	bool		repl_null[Natts_pg_authid];
	bool		repl_repl[Natts_pg_authid];
	const char *errstr = NULL;

	if (!superuser())
		ereport(ERROR,
				(errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
				 (errmsg("must be superuser to update one's SCRAM verifier"))));

	/* Control iteration number and salt length */
	if (iterations <= 0)
	{
		ereport(WARNING,
				(errmsg("Incorrect iteration number, defaulting to %d",
						SCRAM_SHA_256_DEFAULT_ITERATIONS)));
		iterations = SCRAM_SHA_256_DEFAULT_ITERATIONS;
	}

	if (saltlen <= 0)
	{
		ereport(WARNING,
				(errmsg("Incorrect salt length number, defaulting to %d",
						SCRAM_DEFAULT_SALT_LEN)));
		saltlen = SCRAM_DEFAULT_SALT_LEN;
	}

	/*
	 * Normalize the password with SASLprep.  If that doesn't work, because
	 * the password isn't valid UTF-8 or contains prohibited characters, just
	 * proceed with the original password.  (See comments at top of file.)
	 */
	rc = pg_saslprep(password, &prep_password);
	if (rc == SASLPREP_OOM)
		elog(ERROR, "out of memory");
	if (rc == SASLPREP_SUCCESS)
		password = (const char *) prep_password;

	/* Generate a random salt */
	saltbuf = palloc(sizeof(char) * saltlen);
	if (!pg_strong_random(saltbuf, saltlen))
		elog(ERROR, "Failed to generate random salt");

	/* Build verifier */
	verifier = scram_build_secret(PG_SHA256, SCRAM_SHA_256_KEY_LEN,
								  saltbuf, saltlen, iterations,
								  password, &errstr);

	if (prep_password)
		pfree(prep_password);

	/* Verifier is built, so update pg_authid with it */
	rel = table_open(AuthIdRelationId, RowExclusiveLock);

	oldtuple = SearchSysCache1(AUTHNAME, CStringGetDatum(username));
	if (!HeapTupleIsValid(oldtuple))
		ereport(ERROR,
				(errcode(ERRCODE_UNDEFINED_OBJECT),
				 errmsg("role \"%s\" does not exist", username)));

	/* OK, construct the modified tuple with new password */
	memset(repl_repl, false, sizeof(repl_repl));
	memset(repl_null, false, sizeof(repl_null));

	repl_repl[Anum_pg_authid_rolpassword - 1] = true;
	repl_val[Anum_pg_authid_rolpassword - 1] = CStringGetTextDatum(verifier);
	repl_null[Anum_pg_authid_rolpassword - 1] = false;

	dsc = RelationGetDescr(rel);
	newtuple = heap_modify_tuple(oldtuple, dsc, repl_val, repl_null, repl_repl);
	CatalogTupleUpdate(rel, &oldtuple->t_self, newtuple);

	ReleaseSysCache(oldtuple);

	/*
	 * Close pg_authid, but keep lock till commit.
	 */
	table_close(rel, NoLock);

	PG_RETURN_NULL();
}
