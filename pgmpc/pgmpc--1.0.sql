/* pgmpc/pgmpc--1.0.sql */

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pgmpc" to load this file. \quit

CREATE FUNCTION pgmpc_current(OUT title text,
	OUT artist text,
	OUT album text,
	OUT elapsed_time int,
	OUT total_time int,
	OUT song_pos int,
	OUT volume int)
AS 'MODULE_PATHNAME'
LANGUAGE C;

COMMENT ON FUNCTION pgmpc_current()
IS 'Show status of mpd server';

CREATE FUNCTION pgmpc_play()
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C;

COMMENT ON FUNCTION pgmpc_play()
IS 'Play current song again from beginning';

CREATE FUNCTION pgmpc_prev()
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C;

COMMENT ON FUNCTION pgmpc_prev()
IS 'Switch to previous song';

CREATE FUNCTION pgmpc_update()
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C;
CREATE FUNCTION pgmpc_update(path text)
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C;

COMMENT ON FUNCTION pgmpc_update()
IS 'Update remote database';
COMMENT ON FUNCTION pgmpc_update(text)
IS 'Update remote database for given path';

CREATE FUNCTION pgmpc_next()
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C;

COMMENT ON FUNCTION pgmpc_next()
IS 'Switch to next song';

CREATE FUNCTION pgmpc_pause()
RETURNS bool
AS 'MODULE_PATHNAME'
LANGUAGE C;

COMMENT ON FUNCTION pgmpc_pause()
IS 'Toggle play and pause';

CREATE FUNCTION pgmpc_random()
RETURNS bool
AS 'MODULE_PATHNAME'
LANGUAGE C;

COMMENT ON FUNCTION pgmpc_random()
IS 'Switch random mode';

CREATE FUNCTION pgmpc_repeat()
RETURNS bool
AS 'MODULE_PATHNAME'
LANGUAGE C;

COMMENT ON FUNCTION pgmpc_repeat()
IS 'Switch repeat mode';

CREATE FUNCTION pgmpc_single()
RETURNS bool
AS 'MODULE_PATHNAME'
LANGUAGE C;

COMMENT ON FUNCTION pgmpc_single()
IS 'Switch single mode';

CREATE FUNCTION pgmpc_consume()
RETURNS bool
AS 'MODULE_PATHNAME'
LANGUAGE C;

COMMENT ON FUNCTION pgmpc_consume()
IS 'Switch consume mode';

CREATE FUNCTION pgmpc_set_volume(volume int)
RETURNS text
AS 'MODULE_PATHNAME'
LANGUAGE C;

COMMENT ON FUNCTION pgmpc_set_volume(int)
IS 'Set volume between 0 and 100';
