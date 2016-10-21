/* pg_fix_truncation/pg_fix_truncation--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_fix_truncation" to load this file. \quit

-- Truncate FSM up to size of relation
CREATE FUNCTION pg_truncate_fsm(regclass)
RETURNS void
AS 'MODULE_PATHNAME', 'pg_truncate_fsm'
LANGUAGE C STRICT
PARALLEL UNSAFE;
