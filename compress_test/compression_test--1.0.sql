/* compression_test/compression_test--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION compression_test" to load this file. \quit

-- Prototype for raw page obtention
CREATE FUNCTION get_raw_page(IN relid oid,
	IN blkno int4,
	IN with_hole bool,
	OUT page bytea,
	OUT hole_offset smallint)
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

-- Routine useful for decompression to get size of a bytea field
CREATE FUNCTION bytea_size(bytea)
RETURNS int
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

-- Compression routine
CREATE FUNCTION compress_data(bytea)
RETURNS bytea
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

-- Decompression routine
CREATE FUNCTION decompress_data(bytea, smallint)
RETURNS bytea
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;
