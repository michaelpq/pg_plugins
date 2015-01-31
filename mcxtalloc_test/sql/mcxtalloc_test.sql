--
-- Test for mcxtalloc_test
--

-- Depending on the environment where this test suite is run, the output may
-- defer.

-- Initialize environment
CREATE EXTENSION mcxtalloc_test;

-- Error, incorrect size
SELECT mcxtalloc(1024 * 1024 * 1024);
-- May fail because of OOM
SELECT mcxtalloc(1024 * 1024 * 1024 - 1);

-- May fail because of OOM, size correct
SELECT mcxtalloc_huge(1024 * 1024 * 1024);

-- Should return true
SELECT mcxtalloc_zero_cmp(1024);

-- Always failure because of incorrect size, memory requested is higher than 1GB
SELECT mcxtalloc_extended(1024 * 1024 * 1024, false, false, false);
-- Success because size is fine, or failure with OOM
SELECT mcxtalloc_extended(1024 * 1024 * 1024 - 1, false, false, false);
-- Always success, size is correct, and OOM is bypassed
-- Returns false if OOM occurred as allocated pointer was NULL.
SELECT mcxtalloc_extended(1024 * 1024 * 1024 - 1, false, true, false);
-- Always success, huge size is correct, and OOM is bypassed
-- Returns false if OOM occurred as allocated pointer was NULL.
SELECT mcxtalloc_extended(1024 * 1024 * 1024, true, true, false);
