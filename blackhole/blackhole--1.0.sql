/* blackhole/blackhole--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION blackhole" to load this file. \quit

-- This is a blackhole
CREATE FUNCTION blackhole()
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C;

-- This is an example of SRF function, using a tuplestore.
CREATE FUNCTION blackhole_srf_tuplestore(IN num int,
  OUT id int,
  OUT data text)
RETURNS SETOF record
AS 'MODULE_PATHNAME'
LANGUAGE C;
