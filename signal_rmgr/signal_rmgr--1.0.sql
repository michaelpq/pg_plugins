/* signal_rmgr/signal_rmgr--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION signal_rmgr" to load this file. \quit

--
-- signal_rmgr()
--
-- Generates a WAL record that forces a signal on the standby's postmaster,
-- with optionally a "reason" logged when the record is replayed.
--
CREATE FUNCTION signal_rmgr(IN signal int,
    IN reason text DEFAULT '',
    OUT lsn pg_lsn
)
AS 'MODULE_PATHNAME', 'signal_rmgr'
LANGUAGE C STRICT PARALLEL UNSAFE;
