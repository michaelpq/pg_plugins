/* hint_parser/hint_parser--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION hint_parser" to load this file. \quit

-- Entry point to parse a query string and extract a list of hints from it.
CREATE FUNCTION hint_parser(text)
RETURNS void
AS 'MODULE_PATHNAME'
LANGUAGE C;
