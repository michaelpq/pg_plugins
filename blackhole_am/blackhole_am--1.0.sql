/* blackhole_am/blackhole_am--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION blackhole_am" to load this file. \quit

CREATE FUNCTION blackhole_am_handler(internal)
RETURNS table_am_handler
AS 'MODULE_PATHNAME'
LANGUAGE C;

-- Access method
CREATE ACCESS METHOD blackhole_am TYPE TABLE HANDLER blackhole_am_handler;
COMMENT ON ACCESS METHOD blackhole_am IS 'template table AM eating all data';
