/* custom_wal/custom_wal--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION custom_wal" to load this file. \quit

-- custom_wal(record_size, record_number)
CREATE FUNCTION custom_wal(int, int)
RETURNS void
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;
