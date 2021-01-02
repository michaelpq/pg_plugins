/*-------------------------------------------------------------------------
 *
 * uint.c
 *		Overflow checks for unsigned integers
 *
 * Copyright (c) 1996-2021, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  overflow/uint.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"

#include "common/int.h"

/* smallint functions */
PG_FUNCTION_INFO_V1(pg_add_uint16_overflow);
PG_FUNCTION_INFO_V1(pg_sub_uint16_overflow);
PG_FUNCTION_INFO_V1(pg_mul_uint16_overflow);

Datum
pg_add_uint16_overflow(PG_FUNCTION_ARGS)
{
	uint16		v1 = PG_GETARG_UINT16(0);
	uint16		v2 = PG_GETARG_UINT16(1);
	uint16		result;

	PG_RETURN_BOOL(pg_add_u16_overflow(v1, v2, &result));
}

Datum
pg_sub_uint16_overflow(PG_FUNCTION_ARGS)
{
	uint16		v1 = PG_GETARG_UINT16(0);
	uint16		v2 = PG_GETARG_UINT16(1);
	uint16		result;

	PG_RETURN_BOOL(pg_sub_u16_overflow(v1, v2, &result));
}

Datum
pg_mul_uint16_overflow(PG_FUNCTION_ARGS)
{
	uint16		v1 = PG_GETARG_UINT16(0);
	uint16		v2 = PG_GETARG_UINT16(1);
	uint16		result;

	PG_RETURN_BOOL(pg_mul_u16_overflow(v1, v2, &result));
}

/* int functions */
PG_FUNCTION_INFO_V1(pg_add_uint32_overflow);
PG_FUNCTION_INFO_V1(pg_sub_uint32_overflow);
PG_FUNCTION_INFO_V1(pg_mul_uint32_overflow);

Datum
pg_add_uint32_overflow(PG_FUNCTION_ARGS)
{
	uint32		v1 = PG_GETARG_UINT32(0);
	uint32		v2 = PG_GETARG_UINT32(1);
	uint32		result;

	PG_RETURN_BOOL(pg_add_u32_overflow(v1, v2, &result));
}

Datum
pg_sub_uint32_overflow(PG_FUNCTION_ARGS)
{
	uint32		v1 = PG_GETARG_UINT32(0);
	uint32		v2 = PG_GETARG_UINT32(1);
	uint32		result;

	PG_RETURN_BOOL(pg_sub_u32_overflow(v1, v2, &result));
}

Datum
pg_mul_uint32_overflow(PG_FUNCTION_ARGS)
{
	uint32		v1 = PG_GETARG_UINT32(0);
	uint32		v2 = PG_GETARG_UINT32(1);
	uint32		result;

	PG_RETURN_BOOL(pg_mul_u32_overflow(v1, v2, &result));
}

/* bigint functions */
PG_FUNCTION_INFO_V1(pg_add_uint64_overflow);
PG_FUNCTION_INFO_V1(pg_sub_uint64_overflow);
PG_FUNCTION_INFO_V1(pg_mul_uint64_overflow);

Datum
pg_add_uint64_overflow(PG_FUNCTION_ARGS)
{
	uint64		v1 = (uint64) PG_GETARG_INT64(0);
	uint64		v2 = (uint64) PG_GETARG_INT64(1);
	uint64		result;

	PG_RETURN_BOOL(pg_add_u64_overflow(v1, v2, &result));
}

Datum
pg_sub_uint64_overflow(PG_FUNCTION_ARGS)
{
	uint64		v1 = (uint64) PG_GETARG_INT64(0);
	uint64		v2 = (uint64) PG_GETARG_INT64(1);
	uint64		result;

	PG_RETURN_BOOL(pg_sub_u64_overflow(v1, v2, &result));
}

Datum
pg_mul_uint64_overflow(PG_FUNCTION_ARGS)
{
	uint64		v1 = (uint64) PG_GETARG_INT64(0);
	uint64		v2 = (uint64) PG_GETARG_INT64(1);
	uint64		result;

	PG_RETURN_BOOL(pg_mul_u64_overflow(v1, v2, &result));
}
