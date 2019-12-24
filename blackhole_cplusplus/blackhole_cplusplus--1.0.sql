/* blackhole_cplusplus/blackhole_plusplus--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION blackhole_cplusplus" to load this file. \quit

-- This is a blackhole C++ function.
CREATE FUNCTION blackhole_cplusplus()
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C;
