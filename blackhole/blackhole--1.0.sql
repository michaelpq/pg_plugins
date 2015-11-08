/* blackhole/blackhole--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION blackhole" to load this file. \quit

-- Those are the blackhole functions
CREATE FUNCTION blackhole_volatile()
RETURNS text
AS 'MODULE_PATHNAME','blackhole'
LANGUAGE C VOLATILE;

CREATE FUNCTION blackhole_stable()
RETURNS text
AS 'MODULE_PATHNAME','blackhole'
LANGUAGE C STABLE;

CREATE FUNCTION blackhole_immutable()
RETURNS text
AS 'MODULE_PATHNAME','blackhole'
LANGUAGE C IMMUTABLE;
