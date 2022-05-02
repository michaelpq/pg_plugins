/*-------------------------------------------------------------------------
 *
 * pgmpc.c
 *		mpd client for PostgreSQL
 *
 * Copyright (c) 1996-2022, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		  pgmpc/pgmpc.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

/* mpd stuff */
#include "mpd/client.h"

#include "fmgr.h"
#include "funcapi.h"
#include "miscadmin.h"

#include "access/htup.h"
#include "access/htup_details.h"
#include "access/tupdesc.h"
#include "catalog/pg_type_d.h"
#include "utils/builtins.h"
#include "utils/guc.h"
#include "utils/memutils.h"

PG_MODULE_MAGIC;

/* MPD variables */
static struct mpd_connection *mpd_conn = NULL;
static struct mpd_status *mpd_status = NULL;

/* Connection parameters */
static char *mpd_host = "localhost";
static int mpd_port = 6600;
static char *mpd_password;
/* Timeout for connection obtention in seconds, 0 = infinite */
static int mpd_timeout = 10;

/* Entry point of library loading */
void _PG_init(void);

/* Utility functions */
static void pgmpc_init(void);
static void pgmpc_print_error(void);
static void pgmpc_reset(void);
static void pgmpc_load_params(void);
static void pgmpc_init_setof_single(FunctionCallInfo fcinfo,
									Oid argtype,
									char *argname,
									TupleDesc *tupdesc,
									Tuplestorestate **tupstore);
static void pgmpc_init_setof(FunctionCallInfo fcinfo,
							 int argnum,
							 Oid *argtypes,
							 char **argnames,
							 TupleDesc *tupdesc,
							 Tuplestorestate **tupstore);

/* List of interface functions */
PG_FUNCTION_INFO_V1(pgmpc_status);
PG_FUNCTION_INFO_V1(pgmpc_play);
PG_FUNCTION_INFO_V1(pgmpc_pause);
PG_FUNCTION_INFO_V1(pgmpc_next);
PG_FUNCTION_INFO_V1(pgmpc_prev);
PG_FUNCTION_INFO_V1(pgmpc_random);
PG_FUNCTION_INFO_V1(pgmpc_repeat);
PG_FUNCTION_INFO_V1(pgmpc_single);
PG_FUNCTION_INFO_V1(pgmpc_consume);
PG_FUNCTION_INFO_V1(pgmpc_set_volume);
PG_FUNCTION_INFO_V1(pgmpc_update);
PG_FUNCTION_INFO_V1(pgmpc_ls);
PG_FUNCTION_INFO_V1(pgmpc_playlist);
PG_FUNCTION_INFO_V1(pgmpc_lsplaylists);
PG_FUNCTION_INFO_V1(pgmpc_add);
PG_FUNCTION_INFO_V1(pgmpc_load);
PG_FUNCTION_INFO_V1(pgmpc_save);
PG_FUNCTION_INFO_V1(pgmpc_rm);
PG_FUNCTION_INFO_V1(pgmpc_clear);

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
	mpd_conn = mpd_connection_new(mpd_host, mpd_port, mpd_timeout * 1000);
	if (mpd_connection_get_error(mpd_conn) != MPD_ERROR_SUCCESS)
		pgmpc_print_error();

	/* Send password if any */
	if (mpd_password[0])
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
 * pgmpc_init_setof_single
 * Similar to pgmpc_init_setof, for one argument only.
 */
static void
pgmpc_init_setof_single(FunctionCallInfo fcinfo,
						Oid argtype,
						char *argname,
						TupleDesc *tupdesc,
						Tuplestorestate **tupstore)
{
	char **argnames;
	Oid	argtypes[1];

	/* Initialize content and do the work */
	argtypes[0] = argtype;
	argnames = (char **) palloc(sizeof(char *));
	argnames[0] = pstrdup(argname);
	pgmpc_init_setof(fcinfo, 1, argtypes, argnames, tupdesc, tupstore);
	pfree(argnames[0]);
	pfree(argnames);
}

/*
 * pgmpc_init_setof
 * Intilialize properly a function returning multiple tuples with a
 * tuplestore and a TupDesc.
 */
static void
pgmpc_init_setof(FunctionCallInfo fcinfo,
				 int argnum,
				 Oid *argtypes,
				 char **argnames,
				 TupleDesc *tupdesc,
				 Tuplestorestate **tupstore)
{
	ReturnSetInfo *rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;
	MemoryContext per_query_ctx;
	MemoryContext oldcontext;
	int i;

	/* check to see if caller supports us returning a tuplestore */
	if (rsinfo == NULL || !IsA(rsinfo, ReturnSetInfo))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("set-valued function called in context that cannot accept a set")));
	if (!(rsinfo->allowedModes & SFRM_Materialize))
		ereport(ERROR,
				(errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
				 errmsg("materialize mode required, but it is not " \
						"allowed in this context")));

	per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
	oldcontext = MemoryContextSwitchTo(per_query_ctx);

	/* Build tuple descriptor */
	*tupdesc = CreateTemplateTupleDesc(argnum);
	for (i = 0; i < argnum; i++)
		TupleDescInitEntry(*tupdesc, (AttrNumber) i + 1, argnames[i],
						   argtypes[i], -1, 0);

	*tupstore = tuplestore_begin_heap(true, false, work_mem);
	rsinfo->returnMode = SFRM_Materialize;
	rsinfo->setResult = *tupstore;
	rsinfo->setDesc = *tupdesc;

	MemoryContextSwitchTo(oldcontext);
}

/*
 * pgmpc_status
 *
 * Show current song and status.
 */
Datum
pgmpc_status(PG_FUNCTION_ARGS)
{
#define PGMPC_STATUS_COLUMNS 7
	Datum		values[PGMPC_STATUS_COLUMNS];
	bool		nulls[PGMPC_STATUS_COLUMNS];
	TupleDesc	tupdesc;
	HeapTuple	tuple;
	Datum		result;

	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		elog(ERROR, "return type must be a row type");

	pgmpc_init();

	/* Initialize the values of return tuple */
	MemSet(values, 0, sizeof(values));
	MemSet(nulls, true, sizeof(nulls));

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
			int volume = mpd_status_get_volume(mpd_status);

			/* Build tuple using this information */
			if (title)
			{
				nulls[0] = false;
				values[0] =  CStringGetTextDatum(title);
			}
			else
				nulls[0] = true;
			if (artist)
			{
				nulls[1] = false;
				values[1] =  CStringGetTextDatum(artist);
			}
			else
				nulls[1] = true;
			if (album)
			{
				nulls[2] = false;
				values[2] =  CStringGetTextDatum(album);
			}
			else
				nulls[2] = true;
			nulls[3] = false;
			values[3] = UInt32GetDatum(elapsed_time);
			nulls[4] = false;
			values[4] = UInt32GetDatum(total_time);
			nulls[5] = false;
			values[5] = Int32GetDatum(song_pos);
			nulls[6] = false;
			values[6] = Int32GetDatum(volume);

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
	PG_RETURN_VOID();
}

/*
 * pgmpc_pause
 * Pause current song
 */
Datum
pgmpc_pause(PG_FUNCTION_ARGS)
{
	pgmpc_init();
	/*
	 * Enforce pause to be set in all cases. Subsequent calls
	 * to this function result still in a pause state.
	 */
	if (!mpd_run_pause(mpd_conn, true))
		pgmpc_print_error();
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
	PG_RETURN_VOID();
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
	PG_RETURN_VOID();
}

/*
 * pgmpc_update
 * Update remote database.
 */
Datum
pgmpc_update(PG_FUNCTION_ARGS)
{
	char *path = NULL;

	/* Get optional path if defined */
	if (PG_NARGS() == 1 && !PG_ARGISNULL(0))
		path = text_to_cstring(PG_GETARG_TEXT_PP(0));

	/* Run the command */
	pgmpc_init();
	if (!mpd_run_update(mpd_conn, path))
		pgmpc_print_error();
	pgmpc_reset();
	PG_RETURN_VOID();
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

/*
 * pgmpc_set_volume
 * Set volume on server.
 */
Datum
pgmpc_set_volume(PG_FUNCTION_ARGS)
{
	unsigned int volume = PG_GETARG_UINT32(0);

	/* Check for incorrect values */
	if (volume > 100)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("Volume value needs to be between 0 and 100")));

	/* Run the command */
	pgmpc_init();
	if (!mpd_run_set_volume(mpd_conn, volume))
		pgmpc_print_error();
	pgmpc_reset();
	PG_RETURN_VOID();
}

/*
 * pgmpc_ls
 * List all songs of remote server.
 */
Datum
pgmpc_ls(PG_FUNCTION_ARGS)
{
	TupleDesc   tupdesc;
	Tuplestorestate *tupstore;
	char *path = NULL;

	if (PG_NARGS() == 1 && !PG_ARGISNULL(0))
		path = text_to_cstring(PG_GETARG_TEXT_PP(0));

	/* Initialize function context */
	pgmpc_init_setof_single(fcinfo, TEXTOID, "uri", &tupdesc, &tupstore);

	/*
	 * Run the command to get all the songs.
	 */
	pgmpc_init();
	if (!mpd_send_list_all(mpd_conn, path))
		pgmpc_print_error();

	/* Now get all the songs and send them back to caller */
	while (true)
	{
		Datum       values[1];
		bool		nulls[1];
		struct mpd_song *song = mpd_recv_song(mpd_conn);

		/* Leave if done */
		if (song == NULL)
			break;

		/* Assign song name */
		nulls[0] = false;
		values[0] = CStringGetTextDatum(mpd_song_get_uri(song));

		/* Save values */
		tuplestore_putvalues(tupstore, tupdesc, values, nulls);

		/* Clean up for the next one */
		mpd_song_free(song);
	}

	/* We may be in error state, so check for it */
	if (mpd_connection_get_error(mpd_conn) != MPD_ERROR_SUCCESS)
	{
		const char *message = mpd_connection_get_error_message(mpd_conn);
		pgmpc_reset();
		ereport(ERROR,
				(errcode(ERRCODE_SYSTEM_ERROR),
				 errmsg("mpd command failed: %s",
						message)));
	}

	/* Clean up */
	pgmpc_reset();

	/* clean up and return the tuplestore */
	tuplestore_donestoring(tupstore);

	return (Datum) 0;
}

/*
 * pgmpc_playlist
 * List all songs in given playlist. If not playlist is specified list
 * songs of current playlist.
 */
Datum
pgmpc_playlist(PG_FUNCTION_ARGS)
{
	TupleDesc   tupdesc;
	Tuplestorestate *tupstore;
	char *playlist = NULL;
	bool ret;

	if (PG_NARGS() == 1 && !PG_ARGISNULL(0))
		playlist = text_to_cstring(PG_GETARG_TEXT_PP(0));

	/* Initialize function context */
	pgmpc_init_setof_single(fcinfo, TEXTOID, "playlist", &tupdesc, &tupstore);

	/*
	 * Run the command to get all the songs.
	 */
	pgmpc_init();
	ret = playlist ?
		mpd_send_list_playlist_meta(mpd_conn, playlist) :
		mpd_send_list_queue_meta(mpd_conn);
	if (!ret)
		pgmpc_print_error();

	/* Now get all the songs and send them back to caller */
	while (true)
	{
		Datum       values[1];
		bool		nulls[1];
		struct mpd_song *song = mpd_recv_song(mpd_conn);

		/* Leave if done */
		if (song == NULL)
			break;

		/* Assign song name */
		nulls[0] = false;
		values[0] = CStringGetTextDatum(mpd_song_get_uri(song));

		/* Save values */
		tuplestore_putvalues(tupstore, tupdesc, values, nulls);

		/* Clean up for the next one */
		mpd_song_free(song);
	}

	/* We may be in error state, so check for it */
	if (mpd_connection_get_error(mpd_conn) != MPD_ERROR_SUCCESS)
	{
		const char *message = mpd_connection_get_error_message(mpd_conn);
		pgmpc_reset();
		ereport(ERROR,
				(errcode(ERRCODE_SYSTEM_ERROR),
				 errmsg("mpd command failed: %s",
						message)));
	}

	/* Clean up */
	pgmpc_reset();

	/* clean up and return the tuplestore */
	tuplestore_donestoring(tupstore);

	return (Datum) 0;
}

/*
 * pgmpc_lsplaylists
 * List all playlists of remote server.
 */
Datum
pgmpc_lsplaylists(PG_FUNCTION_ARGS)
{
	TupleDesc   tupdesc;
	Tuplestorestate *tupstore;

	/* Initialize function context */
	pgmpc_init_setof_single(fcinfo, TEXTOID, "playlist", &tupdesc, &tupstore);

	/*
	 * Run the command to get all the songs.
	 */
	pgmpc_init();
	if (!mpd_send_list_playlists(mpd_conn))
		pgmpc_print_error();

	/* Now get all the songs and send them back to caller */
	while (true)
	{
		Datum       values[1];
		bool		nulls[1];
		struct mpd_playlist *playlist = mpd_recv_playlist(mpd_conn);

		/* Leave if done */
		if (playlist == NULL)
			break;

		/* Assign song name */
		nulls[0] = false;
		values[0] = CStringGetTextDatum(mpd_playlist_get_path(playlist));

		/* Save values */
		tuplestore_putvalues(tupstore, tupdesc, values, nulls);

		/* Clean up for the next one */
		mpd_playlist_free(playlist);
	}

	/* We may be in error state, so check for it */
	if (mpd_connection_get_error(mpd_conn) != MPD_ERROR_SUCCESS)
	{
		const char *message = mpd_connection_get_error_message(mpd_conn);
		pgmpc_reset();
		ereport(ERROR,
				(errcode(ERRCODE_SYSTEM_ERROR),
				 errmsg("mpd command failed: %s",
						message)));
	}

	/* Clean up */
	pgmpc_reset();

	/* clean up and return the tuplestore */
	tuplestore_donestoring(tupstore);

	return (Datum) 0;
}

/*
 * pgmpc_add
 * Add given song path to current playlist.
 */
Datum
pgmpc_add(PG_FUNCTION_ARGS)
{
	char *path;

	if (PG_ARGISNULL(0))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("Song path needs to be specified")));

	/* Get path value */
	path = text_to_cstring(PG_GETARG_TEXT_PP(0));

	/* User needs to specify a path */
	if (path == NULL)

	/* Now run the command */
	pgmpc_init();
	if (!mpd_run_add(mpd_conn, path))
		pgmpc_print_error();
	pgmpc_reset();
	PG_RETURN_VOID();
}

/*
 * pgmpc_load
 * Load given playlist.
 */
Datum
pgmpc_load(PG_FUNCTION_ARGS)
{
	char *playlist;

	/* User needs to specify a playlist */
	if (PG_ARGISNULL(0))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("Playlist needs to be specified")));

	playlist = text_to_cstring(PG_GETARG_TEXT_PP(0));

	/* Now run the command */
	pgmpc_init();
	if (!mpd_run_load(mpd_conn, playlist))
		pgmpc_print_error();
	pgmpc_reset();
	PG_RETURN_VOID();
}

/*
 * pgmpc_save
 * Save current playlist to file with given name.
 */
Datum
pgmpc_save(PG_FUNCTION_ARGS)
{
	char *playlist;

	/* User needs to specify a playlist */
	if (PG_ARGISNULL(0))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("Playlist needs to be specified")));

	/* Get playlist value */
	playlist = text_to_cstring(PG_GETARG_TEXT_PP(0));

	/* Now run the command */
	pgmpc_init();
	if (!mpd_run_save(mpd_conn, playlist))
		pgmpc_print_error();
	pgmpc_reset();
	PG_RETURN_VOID();
}

/*
 * pgmpc_rm
 * Delete given playlist.
 */
Datum
pgmpc_rm(PG_FUNCTION_ARGS)
{
	char *playlist;

	/* User needs to specify a playlist */
	if (PG_ARGISNULL(0))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("Playlist needs to be specified")));

	/* get playlist value */
	playlist = text_to_cstring(PG_GETARG_TEXT_PP(0));

	/* Now run the command */
	pgmpc_init();
	if (!mpd_run_rm(mpd_conn, playlist))
		pgmpc_print_error();
	pgmpc_reset();
	PG_RETURN_VOID();
}

/*
 * pgmpc_clear
 * Clear current playlist.
 */
Datum
pgmpc_clear(PG_FUNCTION_ARGS)
{
	/* Now run the command */
	pgmpc_init();
	if (!mpd_run_clear(mpd_conn))
		pgmpc_print_error();
	pgmpc_reset();
	PG_RETURN_VOID();
}

/*
 * pgmpc_load_params
 * Load GUC parameters
 */
static void
pgmpc_load_params(void)
{
	/* Connection host */
	DefineCustomStringVariable("pgmpc.mpd_host",
							   "Sets the IP to connect to mpd server.",
							   "Default value is \"localhost\".",
							   &mpd_host,
							   "localhost",
							   PGC_USERSET,
							   0, NULL, NULL, NULL);

	/* Connection password */
	DefineCustomStringVariable("pgmpc.mpd_password",
							   "Sets the password to connect to mpd server.",
							   "Default value is \"\".",
							   &mpd_password,
							   "",
							   PGC_USERSET,
							   0, NULL, NULL, NULL);

	/* Connection port */
	DefineCustomIntVariable("pgmpc.mpd_port",
							"Sets the port to connect to mpd server.",
							"Default value set to 6600.",
							&mpd_port,
							6600, 1, 65536,
							PGC_USERSET,
							0, NULL, NULL, NULL);

	/* Connection port */
	DefineCustomIntVariable("pgmpc.mpd_timeout",
							"Sets timeout for connection obtention in s.",
							"Default value set to 10s. Max is 300s. 0 means inifinite wait.",
							&mpd_timeout,
							10, 0, 300,
							PGC_USERSET,
							0, NULL, NULL, NULL);

	MarkGUCPrefixReserved("pgmpc");
}

/*
 * _PG_init
 * Entry point for library loading
 */
void
_PG_init(void)
{
	/* Load dedicated GUC parameters */
	pgmpc_load_params();
}
