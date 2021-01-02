/*-------------------------------------------------------------------------
 *
 * int.c
 *		Overflow checks for signed integers
 *
 * Copyright (c) 1996-2021, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  overflow/int.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"

#include "common/int.h"

PG_MODULE_MAGIC;

/* smallint functions */
PG_FUNCTION_INFO_V1(pg_add_int16_overflow);
PG_FUNCTION_INFO_V1(pg_sub_int16_overflow);
PG_FUNCTION_INFO_V1(pg_mul_int16_overflow);

Datum
pg_add_int16_overflow(PG_FUNCTION_ARGS)
{
	int16		v1 = PG_GETARG_INT16(0);
	int16		v2 = PG_GETARG_INT16(1);
	int16		result;

	PG_RETURN_BOOL(pg_add_s16_overflow(v1, v2, &result));
}

Datum
pg_sub_int16_overflow(PG_FUNCTION_ARGS)
{
	int16		v1 = PG_GETARG_INT16(0);
	int16		v2 = PG_GETARG_INT16(1);
	int16		result;

	PG_RETURN_BOOL(pg_sub_s16_overflow(v1, v2, &result));
}

Datum
pg_mul_int16_overflow(PG_FUNCTION_ARGS)
{
	int16		v1 = PG_GETARG_INT16(0);
	int16		v2 = PG_GETARG_INT16(1);
	int16		result;

	PG_RETURN_BOOL(pg_mul_s16_overflow(v1, v2, &result));
}

/* int functions */
PG_FUNCTION_INFO_V1(pg_add_int32_overflow);
PG_FUNCTION_INFO_V1(pg_sub_int32_overflow);
PG_FUNCTION_INFO_V1(pg_mul_int32_overflow);

Datum
pg_add_int32_overflow(PG_FUNCTION_ARGS)
{
	int32		v1 = PG_GETARG_INT32(0);
	int32		v2 = PG_GETARG_INT32(1);
	int32		result;

	PG_RETURN_BOOL(pg_add_s32_overflow(v1, v2, &result));
}

Datum
pg_sub_int32_overflow(PG_FUNCTION_ARGS)
{
	int32		v1 = PG_GETARG_INT32(0);
	int32		v2 = PG_GETARG_INT32(1);
	int32		result;

	PG_RETURN_BOOL(pg_sub_s32_overflow(v1, v2, &result));
}

Datum
pg_mul_int32_overflow(PG_FUNCTION_ARGS)
{
	int32		v1 = PG_GETARG_INT32(0);
	int32		v2 = PG_GETARG_INT32(1);
	int32		result;

	PG_RETURN_BOOL(pg_mul_s32_overflow(v1, v2, &result));
}

/* bigint functions */
PG_FUNCTION_INFO_V1(pg_add_int64_overflow);
PG_FUNCTION_INFO_V1(pg_sub_int64_overflow);
PG_FUNCTION_INFO_V1(pg_mul_int64_overflow);

Datum
pg_add_int64_overflow(PG_FUNCTION_ARGS)
{
	int64		v1 = PG_GETARG_INT64(0);
	int64		v2 = PG_GETARG_INT64(1);
	int64		result;

	PG_RETURN_BOOL(pg_add_s64_overflow(v1, v2, &result));
}

Datum
pg_sub_int64_overflow(PG_FUNCTION_ARGS)
{
	int64		v1 = PG_GETARG_INT64(0);
	int64		v2 = PG_GETARG_INT64(1);
	int64		result;

	PG_RETURN_BOOL(pg_sub_s64_overflow(v1, v2, &result));
}

Datum
pg_mul_int64_overflow(PG_FUNCTION_ARGS)
{
	int64		v1 = PG_GETARG_INT64(0);
	int64		v2 = PG_GETARG_INT64(1);
	int64		result;

	PG_RETURN_BOOL(pg_mul_s64_overflow(v1, v2, &result));
}
