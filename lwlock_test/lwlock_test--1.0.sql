/* lwlock_test/lwlock_test--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION lwlock_test" to load this file. \quit

--
-- lwlock_test functions
--
CREATE FUNCTION lwlock_test_acquire()
RETURNS VOID
AS 'MODULE_PATHNAME', 'lwlock_test_acquire'
LANGUAGE C STRICT PARALLEL UNSAFE;

CREATE FUNCTION lwlock_test_release()
RETURNS VOID
AS 'MODULE_PATHNAME', 'lwlock_test_release'
LANGUAGE C STRICT PARALLEL UNSAFE;

CREATE FUNCTION lwlock_test_update(int)
RETURNS VOID
AS 'MODULE_PATHNAME', 'lwlock_test_update'
LANGUAGE C STRICT PARALLEL UNSAFE;

CREATE FUNCTION lwlock_test_wait(int)
RETURNS int
AS 'MODULE_PATHNAME', 'lwlock_test_wait'
LANGUAGE C STRICT PARALLEL UNSAFE;
