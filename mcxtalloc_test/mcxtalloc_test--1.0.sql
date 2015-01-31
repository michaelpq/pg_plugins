/* mcxtalloc_test/mcxtalloc_test--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION mcxtalloc_test" to load this file. \quit

CREATE FUNCTION mcxtalloc(size int)
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FUNCTION mcxtalloc_huge(size int)
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FUNCTION mcxtalloc_zero_cmp(size int)
RETURNS bool
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;

CREATE FUNCTION mcxtalloc_extended(size int,
	is_huge bool,
	is_no_oom bool,
	is_zero bool)
RETURNS bool
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;
