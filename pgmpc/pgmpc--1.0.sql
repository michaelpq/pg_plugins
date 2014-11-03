/* pgmpc/pgmpc--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pgmpc" to load this file. \quit

-- This is a pgmpc
CREATE FUNCTION pgmpc_current(OUT title text,
	OUT artist text,
	OUT album text,
	OUT elapsed_time int,
	OUT total_time int,
	OUT song_pos int)
AS 'MODULE_PATHNAME'
LANGUAGE C;
