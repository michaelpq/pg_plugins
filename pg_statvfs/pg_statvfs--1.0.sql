/* pg_statvfs/pg_statvfs--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_statvfs" to load this file. \quit

-- This is a pg_statvfs

CREATE FUNCTION pg_statvfs(
    IN path text,
    OUT f_bsize bigint,
    OUT f_frsize bigint,
    OUT f_blocks bigint,
    OUT f_bfree bigint,
    OUT f_bavail bigint,
    OUT f_files bigint,
    OUT f_ffree bigint,
    OUT f_favail bigint,
    OUT f_fsid bigint,
    OUT f_namemax bigint,
    OUT flags text[]
)
RETURNS record
AS 'MODULE_PATHNAME'
LANGUAGE C VOLATILE;
