/*-------------------------------------------------------------------------
 *
 * pgmpc.c
 *		mpd client for PostgreSQL
 *
 * Copyright (c) 2013-2014, Michael Paquier
 * Copyright (c) 1996-2014, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  pgmpc/pgmpc.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"
#include "fmgr.h"
#include "funcapi.h"

#include "access/htup.h"
#include "access/htup_details.h"
#include "access/tupdesc.h"
#include "utils/builtins.h"

/* mpd stuff */
#include "mpd/client.h"

PG_MODULE_MAGIC;

/* MPD variables */
static struct mpd_connection *mpd_conn = NULL;
static struct mpd_status *mpd_status = NULL;

/* Connection parameters */
static char *mpd_host = "localhost";
static int mpd_port = 6600;
static char *mpd_password = NULL; /* TODO use it as a parameter */

/* Utility functions */
static void pgmpc_init(void);
static void pgmpc_print_error(void);
static void pgmpc_reset(void);

/* List of interface functions */
PG_FUNCTION_INFO_V1(pgmpc_current);
PG_FUNCTION_INFO_V1(pgmpc_play);
PG_FUNCTION_INFO_V1(pgmpc_pause);
PG_FUNCTION_INFO_V1(pgmpc_next);
PG_FUNCTION_INFO_V1(pgmpc_prev);
PG_FUNCTION_INFO_V1(pgmpc_random);
PG_FUNCTION_INFO_V1(pgmpc_repeat);
PG_FUNCTION_INFO_V1(pgmpc_single);
PG_FUNCTION_INFO_V1(pgmpc_consume);

/*
 * pgmpc_init
 *
 * Initialize connection to mpd server.
 */
static void
pgmpc_init(void)
{
	Assert(mpd_conn == NULL);

	/* Establish connection to mpd server */
	mpd_conn = mpd_connection_new(mpd_host, mpd_port, 0);
	if (mpd_connection_get_error(mpd_conn) != MPD_ERROR_SUCCESS)
		pgmpc_print_error();

	/* Send password if any */
	if (mpd_password)
	{
		if (!mpd_run_password(mpd_conn, mpd_password))
			pgmpc_print_error();
	}
}

/*
 * pgmpc_reset
 *
 * Cleanup existing mpd context.
 */
static void
pgmpc_reset(void)
{
	if (mpd_conn)
		mpd_connection_free(mpd_conn);
	if (mpd_status)
		mpd_status_free(mpd_status);
	mpd_conn = NULL;
	mpd_status = NULL;
}

/*
 * pgmpc_print_error
 *
 * Relay an error from mpd to Postgres.
 */
static void
pgmpc_print_error(void)
{
	const char *message;

	Assert(mpd_connection_get_error(mpd_conn) != MPD_ERROR_SUCCESS);

	/* Obtain error message */
	message = mpd_connection_get_error_message(mpd_conn);

	/* Cleanup */
	pgmpc_reset();

	/* Report error */
	ereport(ERROR,
			(errcode(ERRCODE_SYSTEM_ERROR),
			 errmsg("mpd command failed: %s",
					message)));
}

/*
 * pgmpc_current
 *
 * Show current song and status.
 */
Datum
pgmpc_current(PG_FUNCTION_ARGS)
{
	Datum		values[6];
	bool		nulls[6];
	TupleDesc	tupdesc;
	HeapTuple	tuple;
	Datum		result;

	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		elog(ERROR, "return type must be a row type");

	pgmpc_init();

	/* Initialize the values of return tuple */
	memset(values, 0, sizeof(values));
	memset(nulls, true, sizeof(nulls));

	/*
	 * Send all necessary commands at once to avoid unnecessary round
	 * trips. The following information is obtained in an aync way:
	 * - current status of server
	 * - current song run on server
	 */
	if (!mpd_command_list_begin(mpd_conn, true) ||
		!mpd_send_status(mpd_conn) ||
		!mpd_send_current_song(mpd_conn) ||
		!mpd_command_list_end(mpd_conn))
		pgmpc_print_error();

	/* Obtain status from server and check it */
	mpd_status = mpd_recv_status(mpd_conn);
	if (mpd_status == NULL)
		pgmpc_print_error();

	/* Show current song if any */
	if (mpd_status_get_state(mpd_status) == MPD_STATE_PLAY ||
		mpd_status_get_state(mpd_status) == MPD_STATE_PAUSE)
	{
		struct mpd_song *song;

		/* There should be a next response, in this case a song */
		if (!mpd_response_next(mpd_conn))
			pgmpc_print_error();

		/* And now get it */
		song = mpd_recv_song(mpd_conn);
		if (song != NULL)
		{
			/* Get information about the current song */
			const char *title = mpd_song_get_tag(song, MPD_TAG_TITLE, 0);
			const char *artist = mpd_song_get_tag(song, MPD_TAG_ARTIST, 0);
			const char *album = mpd_song_get_tag(song, MPD_TAG_ALBUM, 0);
			unsigned int elapsed_time = mpd_status_get_elapsed_time(mpd_status);
			unsigned int total_time = mpd_status_get_total_time(mpd_status);
			int song_pos = mpd_status_get_song_pos(mpd_status) + 1;

			/* Build tuple using this information */
			if (title)
			{
				nulls[0] = false;
				values[0] =  CStringGetTextDatum(title);
			}
			if (artist)
			{
				nulls[1] = false;
				values[1] =  CStringGetTextDatum(artist);
			}
			if (album)
			{
				nulls[2] = false;
				values[2] =  CStringGetTextDatum(album);
			}
			nulls[3] = false;
			values[3] = UInt32GetDatum(elapsed_time);
			nulls[4] = false;
			values[4] = UInt32GetDatum(total_time);
			nulls[5] = false;
			values[5] = Int32GetDatum(song_pos);

			/* Song data is no more needed */
			mpd_song_free(song);
		}

		if (!mpd_response_finish(mpd_conn))
			pgmpc_print_error();
	}

	/* Cleanup MPD status */
	pgmpc_reset();

	/* Form result and return it */
	tuple = heap_form_tuple(tupdesc, values, nulls);
	result = HeapTupleGetDatum(tuple);

	PG_RETURN_DATUM(result);
}

/*
 * pgmpc_play
 * Play a song.
 */
Datum
pgmpc_play(PG_FUNCTION_ARGS)
{
	pgmpc_init();
	/*
	 * Enforce disabling of pause. We could here check for the server
	 * status before doing anything but is it worth the round trip?
	 */
	if (!mpd_run_pause(mpd_conn, false))
		pgmpc_print_error();
	pgmpc_reset();
	PG_RETURN_NULL();
}

/*
 * pgmpc_pause
 * Pause current song
 */
Datum
pgmpc_pause(PG_FUNCTION_ARGS)
{
	pgmpc_init();

	/* Get first status of server to determine next action */
	mpd_status = mpd_run_status(mpd_conn);
	if (mpd_status == NULL)
		pgmpc_print_error();

	/* If song is being played, do a pause. If not disable pause. */
	if (mpd_status_get_state(mpd_status) == MPD_STATE_PLAY)
	{
		if (!mpd_run_pause(mpd_conn, true))
			pgmpc_print_error();
	}
	else if (mpd_status_get_state(mpd_status) == MPD_STATE_PAUSE)
	{
		if (!mpd_run_pause(mpd_conn, false))
			pgmpc_print_error();
	}

	pgmpc_reset();
	PG_RETURN_NULL();
}

/*
 * pgmpc_next
 * Play next song.
 */
Datum
pgmpc_next(PG_FUNCTION_ARGS)
{
	pgmpc_init();
	if (!mpd_run_next(mpd_conn))
		pgmpc_print_error();
	pgmpc_reset();
	PG_RETURN_NULL();
}

/*
 * pgmpc_prev
 * Play previous song.
 */
Datum
pgmpc_prev(PG_FUNCTION_ARGS)
{
	pgmpc_init();
	if (!mpd_run_previous(mpd_conn))
		pgmpc_print_error();
	pgmpc_reset();
	PG_RETURN_NULL();
}

/*
 * pgmpc_random
 * Switch random mode.
 */
Datum
pgmpc_random(PG_FUNCTION_ARGS)
{
	bool is_random;

	pgmpc_init();

	/* Get first status of server to determine next action */
	mpd_status = mpd_run_status(mpd_conn);
	if (mpd_status == NULL)
		pgmpc_print_error();

	/* Reverse random mode */
	is_random = mpd_status_get_random(mpd_status);
	if (!mpd_run_random(mpd_conn, !is_random))
		pgmpc_print_error();
	pgmpc_reset();
	PG_RETURN_BOOL(!is_random);
}

/*
 * pgmpc_repeat
 * Switch repeat mode.
 */
Datum
pgmpc_repeat(PG_FUNCTION_ARGS)
{
	bool is_repeat;

	pgmpc_init();

	/* Get first status of server to determine next action */
	mpd_status = mpd_run_status(mpd_conn);
	if (mpd_status == NULL)
		pgmpc_print_error();

	/* Reverse repeat mode */
	is_repeat = mpd_status_get_repeat(mpd_status);
	if (!mpd_run_repeat(mpd_conn, !is_repeat))
		pgmpc_print_error();
	pgmpc_reset();
	PG_RETURN_BOOL(!is_repeat);
}

/*
 * pgmpc_single
 * Switch single mode.
 */
Datum
pgmpc_single(PG_FUNCTION_ARGS)
{
	bool is_single;

	pgmpc_init();

	/* Get first status of server to determine next action */
	mpd_status = mpd_run_status(mpd_conn);
	if (mpd_status == NULL)
		pgmpc_print_error();

	/* Reverse single mode */
	is_single = mpd_status_get_single(mpd_status);
	if (!mpd_run_single(mpd_conn, !is_single))
		pgmpc_print_error();
	pgmpc_reset();
	PG_RETURN_BOOL(!is_single);
}

/*
 * pgmpc_consume
 * Switch consume mode.
 */
Datum
pgmpc_consume(PG_FUNCTION_ARGS)
{
	bool is_consume;

	pgmpc_init();

	/* Get first status of server to determine next action */
	mpd_status = mpd_run_status(mpd_conn);
	if (mpd_status == NULL)
		pgmpc_print_error();

	/* Reverse consume mode */
	is_consume = mpd_status_get_consume(mpd_status);
	if (!mpd_run_consume(mpd_conn, !is_consume))
		pgmpc_print_error();
	pgmpc_reset();
	PG_RETURN_BOOL(!is_consume);
}
