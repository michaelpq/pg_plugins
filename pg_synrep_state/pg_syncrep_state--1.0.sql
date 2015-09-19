/* pg_syncrep_state/pg_syncrep_state--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_syncrep_state" to load this file. \quit

-- This is a pg_syncrep_state

CREATE FUNCTION pg_syncrep_state(
    OUT pid int,
    OUT wait_state text,
    OUT wait_lsn pg_lsn
)
RETURNS SETOF record
AS 'MODULE_PATHNAME'
LANGUAGE C VOLATILE;

-- Utility view aimed at being joined with pg_stat_activity
CREATE VIEW pg_syncrep_state AS
    SELECT pid,
	   wait_state,
           wait_lsn
    FROM pg_syncrep_state();
