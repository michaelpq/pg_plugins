/* overflow/overflow--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION overflow" to load this file. \quit

-- smallint functions
CREATE FUNCTION pg_add_int16_overflow(IN v1 smallint, IN v2 smallint)
RETURNS bool
AS 'MODULE_PATHNAME'
LANGUAGE C;
CREATE FUNCTION pg_sub_int16_overflow(IN v1 smallint, IN v2 smallint)
RETURNS bool
AS 'MODULE_PATHNAME'
LANGUAGE C;
CREATE FUNCTION pg_mul_int16_overflow(IN v1 smallint, IN v2 smallint)
RETURNS bool
AS 'MODULE_PATHNAME'
LANGUAGE C;

-- int functions
CREATE FUNCTION pg_add_int32_overflow(IN v1 int, IN v2 int)
RETURNS bool
AS 'MODULE_PATHNAME'
LANGUAGE C;
CREATE FUNCTION pg_sub_int32_overflow(IN v1 int, IN v2 int)
RETURNS bool
AS 'MODULE_PATHNAME'
LANGUAGE C;
CREATE FUNCTION pg_mul_int32_overflow(IN v1 int, IN v2 int)
RETURNS bool
AS 'MODULE_PATHNAME'
LANGUAGE C;

-- bigint functions
CREATE FUNCTION pg_add_int64_overflow(IN v1 bigint, IN v2 bigint)
RETURNS bool
AS 'MODULE_PATHNAME'
LANGUAGE C;
CREATE FUNCTION pg_sub_int64_overflow(IN v1 bigint, IN v2 bigint)
RETURNS bool
AS 'MODULE_PATHNAME'
LANGUAGE C;
CREATE FUNCTION pg_mul_int64_overflow(IN v1 bigint, IN v2 bigint)
RETURNS bool
AS 'MODULE_PATHNAME'
LANGUAGE C;
