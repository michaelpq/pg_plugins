/*-------------------------------------------------------------------------
 *
 * hmac_funcs.c
 *		Set of functions for HMAC.
 *
 * Copyright (c) 1996-2025, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  hmac_funcs/hmac_funcs.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"

#include "common/hmac.h"
#include "common/md5.h"
#include "common/sha1.h"
#include "common/sha2.h"
#include "varatt.h"

PG_MODULE_MAGIC;

/*
 * This is the hmac_funcs function.
 */
PG_FUNCTION_INFO_V1(hmac_md5);
PG_FUNCTION_INFO_V1(hmac_sha1);
PG_FUNCTION_INFO_V1(hmac_sha224);
PG_FUNCTION_INFO_V1(hmac_sha256);
PG_FUNCTION_INFO_V1(hmac_sha384);
PG_FUNCTION_INFO_V1(hmac_sha512);

/*
 * Internal routine to compute a HMAC with the given bytea input and key.
 */
static inline bytea *
hmac_internal(pg_cryptohash_type type, bytea *input, bytea *key)
{
	const		uint8 *data;
	const		uint8 *keydata;
	const char *typestr = NULL;
	size_t		len,
				keylen;
	pg_hmac_ctx *ctx;
	unsigned char *buf;
	bytea	   *result;
	int			digest_length = 0;

	switch (type)
	{
		case PG_MD5:
			typestr = "MD5";
			digest_length = MD5_DIGEST_LENGTH;
			break;
		case PG_SHA1:
			typestr = "SHA1";
			digest_length = SHA1_DIGEST_LENGTH;
			break;
		case PG_SHA224:
			typestr = "SHA224";
			digest_length = PG_SHA224_DIGEST_LENGTH;
			break;
		case PG_SHA256:
			typestr = "SHA256";
			digest_length = PG_SHA256_DIGEST_LENGTH;
			break;
		case PG_SHA384:
			typestr = "SHA384";
			digest_length = PG_SHA384_DIGEST_LENGTH;
			break;
		case PG_SHA512:
			typestr = "SHA512";
			digest_length = PG_SHA512_DIGEST_LENGTH;
			break;
	}

	buf = palloc0(digest_length);

	len = VARSIZE_ANY_EXHDR(input);
	data = (unsigned char *) VARDATA_ANY(input);
	keylen = VARSIZE_ANY_EXHDR(key);
	keydata = (unsigned char *) VARDATA_ANY(key);

	ctx = pg_hmac_create(type);
	if (pg_hmac_init(ctx, keydata, keylen) < 0)
		elog(ERROR, "could not initialize %s HMAC context: %s", typestr,
			 pg_hmac_error(ctx));
	if (pg_hmac_update(ctx, data, len) < 0)
		elog(ERROR, "could not update %s HMAC context: %s", typestr,
			 pg_hmac_error(ctx));
	if (pg_hmac_final(ctx, buf, digest_length) < 0)
		elog(ERROR, "could not finalize %s HMAC context: %s", typestr,
			 pg_hmac_error(ctx));
	pg_hmac_free(ctx);

	result = palloc(digest_length + VARHDRSZ);
	SET_VARSIZE(result, digest_length + VARHDRSZ);
	memcpy(VARDATA(result), buf, digest_length);

	return result;
}

Datum
hmac_md5(PG_FUNCTION_ARGS)
{
	bytea	   *result = hmac_internal(PG_MD5,
									   PG_GETARG_BYTEA_PP(0),
									   PG_GETARG_BYTEA_PP(1));

	PG_RETURN_BYTEA_P(result);
}

Datum
hmac_sha1(PG_FUNCTION_ARGS)
{
	bytea	   *result = hmac_internal(PG_SHA1,
									   PG_GETARG_BYTEA_PP(0),
									   PG_GETARG_BYTEA_PP(1));

	PG_RETURN_BYTEA_P(result);
}

Datum
hmac_sha224(PG_FUNCTION_ARGS)
{
	bytea	   *result = hmac_internal(PG_SHA224,
									   PG_GETARG_BYTEA_PP(0),
									   PG_GETARG_BYTEA_PP(1));

	PG_RETURN_BYTEA_P(result);
}

Datum
hmac_sha256(PG_FUNCTION_ARGS)
{
	bytea	   *result = hmac_internal(PG_SHA256,
									   PG_GETARG_BYTEA_PP(0),
									   PG_GETARG_BYTEA_PP(1));

	PG_RETURN_BYTEA_P(result);
}

Datum
hmac_sha384(PG_FUNCTION_ARGS)
{
	bytea	   *result = hmac_internal(PG_SHA384,
									   PG_GETARG_BYTEA_PP(0),
									   PG_GETARG_BYTEA_PP(1));

	PG_RETURN_BYTEA_P(result);
}

Datum
hmac_sha512(PG_FUNCTION_ARGS)
{
	bytea	   *result = hmac_internal(PG_SHA512,
									   PG_GETARG_BYTEA_PP(0),
									   PG_GETARG_BYTEA_PP(1));

	PG_RETURN_BYTEA_P(result);
}
