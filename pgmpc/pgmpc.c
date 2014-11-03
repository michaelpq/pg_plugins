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

/* Utility functions */
static void pgmpc_print_error(struct mpd_connection *conn,
							  struct mpd_status *status);

/* List of interface functions */
PG_FUNCTION_INFO_V1(pgmpc_current);

static void
pgmpc_print_error(struct mpd_connection *conn, struct mpd_status *status)
{
	const char *message;

	Assert(mpd_connection_get_error(conn) != MPD_ERROR_SUCCESS);

	/* Obtain error message */
	message = mpd_connection_get_error_message(conn);

	/* Cleanup */
	mpd_connection_free(conn);
	mpd_status_free(status);

	/* Report error */
	ereport(ERROR,
			(errcode(ERRCODE_SYSTEM_ERROR),
			 errmsg("mpd command failed: %s",
					message)));
}

Datum
pgmpc_current(PG_FUNCTION_ARGS)
{
	struct mpd_connection *conn = NULL;
	struct mpd_status *status = NULL;
	Datum		values[6];
	bool		nulls[6];
	TupleDesc	tupdesc;
	HeapTuple	tuple;
	Datum		result;

	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		elog(ERROR, "return type must be a row type");

	/* Initialize the values of return tuple */
	memset(values, 0, sizeof(values));
	memset(nulls, true, sizeof(nulls));

	/* Establish connection to mpd server */
	conn = mpd_connection_new("localhost", 6600, 0);

	/*
	 * Send all necessary commands at once to avoid unnecessary round
	 * trips. The following information is obtained in an aync way:
	 * - current status of server
	 * - current song run on server
	 */
	if (!mpd_command_list_begin(conn, true) ||
		!mpd_send_status(conn) ||
		!mpd_send_current_song(conn) ||
		!mpd_command_list_end(conn))
		pgmpc_print_error(conn, status);

	/* Obtain status from server and check it */
	status = mpd_recv_status(conn);
	if (status == NULL)
		pgmpc_print_error(conn, status);

	/* Show current song if any */
	if (mpd_status_get_state(status) == MPD_STATE_PLAY ||
		mpd_status_get_state(status) == MPD_STATE_PAUSE)
	{
		struct mpd_song *song;

		/* There should be a next response, in this case a song */
		if (!mpd_response_next(conn))
			pgmpc_print_error(conn, status);

		/* And now get it */
		song = mpd_recv_song(conn);
		if (song != NULL)
		{
			/* Get information about the current song */
			const char *title = mpd_song_get_tag(song, MPD_TAG_TITLE, 0);
			const char *artist = mpd_song_get_tag(song, MPD_TAG_ARTIST, 0);
			const char *album = mpd_song_get_tag(song, MPD_TAG_ALBUM, 0);
			unsigned int elapsed_time = mpd_status_get_elapsed_time(status);
			unsigned int total_time = mpd_status_get_total_time(status);
			int song_pos = mpd_status_get_song_pos(status) + 1;

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

		if (!mpd_response_finish(conn))
			pgmpc_print_error(conn, status);
	}

	mpd_status_free(status);
	mpd_connection_free(conn);

	/* Form result and return it */
	tuple = heap_form_tuple(tupdesc, values, nulls);
	result = HeapTupleGetDatum(tuple);

	PG_RETURN_DATUM(result);
}
