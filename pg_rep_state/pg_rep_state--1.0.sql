/* pg_rep_state/pg_rep_state--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_rep_state" to load this file. \quit

-- This is a pg_rep_state

CREATE FUNCTION pg_rep_state(
    OUT pid int,
    OUT wait_state text,
    OUT wait_lsn pg_lsn
)
RETURNS SETOF record
AS 'MODULE_PATHNAME'
LANGUAGE C VOLATILE;

-- Utility view aimed at being joined with pg_stat_activity
CREATE VIEW pg_rep_state AS
    SELECT pid,
	   wait_state,
           wait_lsn
    FROM pg_rep_state();

-- WAL receiver status
CREATE OR REPLACE FUNCTION pg_wal_receiver_state(
    OUT pid int,
    OUT status text,
    OUT receive_start_lsn pg_lsn,
    OUT receive_start_tli int,
    OUT received_up_to_lsn pg_lsn,
    OUT received_tli int,
    OUT latest_chunk_start_lsn pg_lsn,
    OUT last_msg_send_time timestamptz,
    OUT last_msg_receipt_time timestamptz,
    OUT latest_end_lsn pg_lsn,
    OUT latest_end_time timestamptz,
    OUT slot_name text
)
RETURNS record
AS 'MODULE_PATHNAME'
LANGUAGE C STABLE;

CREATE VIEW pg_wal_receiver_state AS
    SELECT pid,
           status,
	   receive_start_lsn,
	   receive_start_tli,
	   received_up_to_lsn,
	   received_tli,
	   latest_chunk_start_lsn,
	   last_msg_send_time,
	   last_msg_receipt_time,
	   latest_end_lsn,
	   latest_end_time,
	   slot_name
    FROM pg_wal_receiver_state();
