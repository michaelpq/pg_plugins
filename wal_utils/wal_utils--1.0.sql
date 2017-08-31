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

-- Build a list of WAL segments necessary to join the given origin LSN
-- and timeline to their targets. Note that the origin needs to be a
-- direct parent of the target as specified by the history data.
CREATE FUNCTION build_wal_segment_list(
	IN origin_tli int,
	IN origin_lsn pg_lsn,
	IN target_tli int,
	IN target_lsn pg_lsn,
	IN history_data text,
	OUT wal_segs text)
RETURNS SETOF text
AS 'MODULE_PATHNAME'
LANGUAGE C;

-- Set of routines for archive data fetching
-- Get data from the archive path. The base path where the lookup is
-- done uses as environment variable PGARCHIVE which points to a local
-- path where the archives are located. This should be a variable loaded
-- by Postgres. Note that there is no restriction on the file name that
-- caller can use here, a segment file could be compressed, and the
-- archive could be used as well to store some custom metadata. The path
-- defined cannot be absolute as well.
-- CREATE FUNCTION archive_get_data(
-- 	IN filename text,
-- 	IN begin bigint,
-- 	IN offset bigint,
-- 	OUT data bytea)
-- RETURNS bytea
-- AS 'MODULE_PATHNAME'
-- LANGUAGE C STRICT;
-- Get the size of a file in archives.
CREATE FUNCTION archive_get_size(
	IN filename text,
	OUT size bigint)
RETURNS bigint
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;
