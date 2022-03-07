/* pg_rusage/pg_rusage--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_rusage" to load this file. \quit


CREATE FUNCTION pg_rusage_reset()
RETURNS void
AS 'MODULE_PATHNAME'
LANGUAGE C;
CREATE FUNCTION pg_rusage_print()
RETURNS void
AS 'MODULE_PATHNAME'
LANGUAGE C;
