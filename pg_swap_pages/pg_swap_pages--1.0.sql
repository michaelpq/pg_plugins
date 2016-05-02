/* pg_swap_pages/pg_swap_pages--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_swap_pages" to load this file. \quit

-- This is a pg_swap_pages
CREATE FUNCTION pg_swap_pages(oid, int4, int4)
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C;
