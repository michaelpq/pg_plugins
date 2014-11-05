/* pgmpc/pgmpc--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pgmpc" to load this file. \quit

CREATE FUNCTION mpd_status(OUT title text,
	OUT artist text,
	OUT album text,
	OUT elapsed_time int,
	OUT total_time int,
	OUT song_pos int,
	OUT volume int)
AS 'MODULE_PATHNAME', 'pgmpc_status'
LANGUAGE C;

COMMENT ON FUNCTION mpd_status()
IS 'Show status of mpd server';

CREATE FUNCTION mpd_ls()
RETURNS SETOF text
AS 'MODULE_PATHNAME', 'pgmpc_ls'
LANGUAGE C;
CREATE FUNCTION mpd_ls(path text)
RETURNS SETOF text
AS 'MODULE_PATHNAME', 'pgmpc_ls'
LANGUAGE C;

COMMENT ON FUNCTION mpd_ls()
IS 'Get list of all songs on server';
COMMENT ON FUNCTION mpd_ls(text)
IS 'Get list of all songs on server with given path';

CREATE FUNCTION mpd_play()
RETURNS void
AS 'MODULE_PATHNAME', 'pgmpc_play'
LANGUAGE C;

COMMENT ON FUNCTION mpd_play()
IS 'Play current song again from beginning';

CREATE FUNCTION mpd_prev()
RETURNS void
AS 'MODULE_PATHNAME', 'pgmpc_prev'
LANGUAGE C;

COMMENT ON FUNCTION mpd_prev()
IS 'Switch to previous song';

CREATE FUNCTION mpd_update()
RETURNS void
AS 'MODULE_PATHNAME', 'pgmpc_update'
LANGUAGE C;
CREATE FUNCTION mpd_update(path text)
RETURNS void
AS 'MODULE_PATHNAME', 'pgmpc_update'
LANGUAGE C;

COMMENT ON FUNCTION mpd_update()
IS 'Update remote database';
COMMENT ON FUNCTION mpd_update(text)
IS 'Update remote database for given path';

CREATE FUNCTION mpd_next()
RETURNS void
AS 'MODULE_PATHNAME', 'pgmpc_next'
LANGUAGE C;

COMMENT ON FUNCTION mpd_next()
IS 'Switch to next song';

CREATE FUNCTION mpd_pause()
RETURNS bool
AS 'MODULE_PATHNAME', 'pgmpc_pause'
LANGUAGE C;

COMMENT ON FUNCTION mpd_pause()
IS 'Toggle play and pause';

CREATE FUNCTION mpd_random()
RETURNS bool
AS 'MODULE_PATHNAME', 'pgmpc_random'
LANGUAGE C;

COMMENT ON FUNCTION mpd_random()
IS 'Switch random mode';

CREATE FUNCTION mpd_repeat()
RETURNS bool
AS 'MODULE_PATHNAME', 'pgmpc_repeat'
LANGUAGE C;

COMMENT ON FUNCTION mpd_repeat()
IS 'Switch repeat mode';

CREATE FUNCTION mpd_single()
RETURNS bool
AS 'MODULE_PATHNAME', 'pgmpc_single'
LANGUAGE C;

COMMENT ON FUNCTION mpd_single()
IS 'Switch single mode';

CREATE FUNCTION mpd_consume()
RETURNS bool
AS 'MODULE_PATHNAME', 'pgmpc_consume'
LANGUAGE C;

COMMENT ON FUNCTION mpd_consume()
IS 'Switch consume mode';

CREATE FUNCTION mpd_set_volume(volume int)
RETURNS void
AS 'MODULE_PATHNAME', 'pgmpc_set_volume'
LANGUAGE C;

COMMENT ON FUNCTION mpd_set_volume(int)
IS 'Set volume between 0 and 100';
