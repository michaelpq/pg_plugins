/*-------------------------------------------------------------------------
 *
 * mcxtalloc_test.c
 *		Set of functions to test low-level routines in change of doing
 *		memory allocation in a given memory context.
 *
 * Copyright (c) 1996-2020, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  mcxtalloc_test/mcxtalloc_test.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"
#include "utils/palloc.h"

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(mcxtalloc);
PG_FUNCTION_INFO_V1(mcxtalloc_huge);
PG_FUNCTION_INFO_V1(mcxtalloc_zero_cmp);
PG_FUNCTION_INFO_V1(mcxtalloc_extended);

/*
 * mcxtalloc
 * Wrapper to check calls of MemoryContextAlloc.
 */
Datum
mcxtalloc(PG_FUNCTION_ARGS)
{
	Size	alloc_size = PG_GETARG_UINT32(0);
	char   *ptr;

	ptr = MemoryContextAlloc(CurrentMemoryContext, alloc_size);
	Assert(ptr != NULL);
	pfree(ptr);

	PG_RETURN_NULL();
}

/*
 * mcxtalloc_huge
 * Wrapper to check calls of MemoryContextAllocHuge.
 */
Datum
mcxtalloc_huge(PG_FUNCTION_ARGS)
{
	Size	alloc_size = PG_GETARG_UINT32(0);
	char   *ptr;

	ptr = MemoryContextAllocHuge(CurrentMemoryContext, alloc_size);
	Assert(ptr != NULL);
	pfree(ptr);

	PG_RETURN_NULL();
}

/*
 * mcxtalloc_zero
 * Routine checking if MemoryContextAllocZero and MemoryContextAllocExtended
 * called with MCXT_ALLOC_ZERO are equivalent.
 */
Datum
mcxtalloc_zero_cmp(PG_FUNCTION_ARGS)
{
	Size	alloc_size = PG_GETARG_UINT32(0);
	char   *ptr1, *ptr2;
	bool	res = false;

	ptr1 = MemoryContextAllocZero(CurrentMemoryContext, alloc_size);
	ptr2 = MemoryContextAllocExtended(CurrentMemoryContext, alloc_size,
									  MCXT_ALLOC_ZERO);
	Assert(ptr1 != NULL && ptr2 != NULL);

	res = memcmp(ptr1, ptr2, alloc_size) == 0;
	pfree(ptr1);
	pfree(ptr2);

	PG_RETURN_BOOL(res);
}


/*
 * mcxtalloc_extended
 * Wrapper routine for MemoryContextAllocExtended. Returns true if pointer
 * obtained was NULL and an OOM was not wanted by caller, and false in other
 * cases.
 */
Datum
mcxtalloc_extended(PG_FUNCTION_ARGS)
{
	Size	alloc_size = PG_GETARG_UINT32(0);
	bool	is_huge = PG_GETARG_BOOL(1);
	bool	is_no_oom = PG_GETARG_BOOL(2);
	bool	is_zero = PG_GETARG_BOOL(3);
	int		flags = 0;
	char   *ptr;

	if (is_huge)
		flags |= MCXT_ALLOC_HUGE;
	if (is_no_oom)
		flags |= MCXT_ALLOC_NO_OOM;
	if (is_zero)
		flags |= MCXT_ALLOC_ZERO;
	ptr = MemoryContextAllocExtended(CurrentMemoryContext,
									 alloc_size, flags);
	if (ptr != NULL)
	{
		pfree(ptr);
		PG_RETURN_BOOL(true);
	}
	PG_RETURN_BOOL(false);
}
