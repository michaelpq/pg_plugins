/* wal_utils/wal_utils--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION wal_utils" to load this file. \quit

-- Parse a WAL history file, input is a buffer including the data of
-- a history file ready to be parsed. This returns a SQL representation
-- of a list of TimeLineHistoryEntry.
CREATE FUNCTION parse_wal_history(
	IN history_data text,
	OUT timeline int,
	OUT begin_lsn pg_lsn,
	OUT end_lsn pg_lsn)
RETURNS SETOF record
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;
