/*-------------------------------------------------------------------------
 *
 * pg_wal_blocks.c
 *		Tracker of relation blocks touched by WAL records
 *
 * Copyright (c) 1996-2015, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		pg_wal_blocks/pg_wal_blocks.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres_fe.h"
#include "getopt_long.h"

#include "access/xlogdefs.h"
#include "access/xlog_internal.h"

#define PG_WAL_BLOCKS_VERSION "0.1"

const char *progname;

/* Mode parameters */
static bool verbose = false;

/* Data regarding input WAL to parse */
static XLogSegNo segno = 0;
static XLogRecPtr startptr = InvalidXLogRecPtr;
static TimeLineID timeline_id = 1;

static void
usage(const char *progname)
{
	printf("%s tracks relation blocks touched by WAL records.\n\n", progname);
	printf("Usage:\n %s [OPTION] [WAL_SEGMENT]...\n\n", progname);
	printf("Options:\n");
	printf("  -v             write some progress messages as well\n");
	printf("  -V, --version  output version information, then exit\n");
	printf("  -?, --help     show this help, then exit\n");
	printf("\n");
	printf("Report bugs to https://github.com/michaelpq/pg_plugins.\n");
}


/*
 * Split a pathname as dirname(1) and basename(1) would.
 *
 * XXX this probably doesn't do very well on Windows.  We probably need to
 * apply canonicalize_path(), at the very least.
 */
static void
split_path(const char *path, char **dir, char **fname)
{
	char       *sep;

	/* split filepath into directory & filename */
	sep = strrchr(path, '/');

	/* directory path */
	if (sep != NULL)
	{
		*dir = pg_strdup(path);
		(*dir)[(sep - path) + 1] = '\0';        /* no strndup */
		*fname = pg_strdup(sep + 1);
	}
	/* local directory */
	else
	{
		*dir = NULL;
		*fname = pg_strdup(path);
	}
}

int
main(int argc, char **argv)
{
	static struct option long_options[] = {
		{"help", no_argument, NULL, '?'},
		{"version", no_argument, NULL, 'V'},
		{"verbose", no_argument, NULL, 'v'},
		{NULL, 0, NULL, 0}
	};
	int		c;
	int		option_index;

	progname = get_progname(argv[0]);

	if (argc <= 1)
	{
		fprintf(stderr, "%s: no arguments specified\n", progname);
		exit(1);
	}

	/* Process command-line arguments */
	if (argc > 1)
	{
		if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0)
		{
			usage(progname);
			exit(0);
		}
		if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0)
		{
			puts("pg_wal_blocks " PG_WAL_BLOCKS_VERSION);
			exit(0);
		}
	}

	while ((c = getopt_long(argc, argv, "v", long_options, &option_index)) != -1)
	{
		switch (c)
		{
			case '?':
				fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
				exit(1);
			case 'v':
				verbose = true;
				break;
		}
	}

	if ((optind + 1) < argc)
	{
		fprintf(stderr,
				"%s: too many command-line arguments (first is \"%s\")\n",
				progname, argv[optind + 2]);
		exit(1);
	}

	/* parse files as start/end boundaries, extract path if not specified */
	if (optind < argc)
	{
        char       *directory = NULL;
		char       *fname = NULL;
		char	   *full_path = pg_strdup(argv[optind]);
		int         fd;

		split_path(full_path, &directory, &fname);

		fd = open(full_path, O_RDONLY | PG_BINARY, 0);
		if (fd < 0)
		{
			fprintf(stderr, "could not open file \"%s\"", fname);
			exit(1);
		}
		close(fd);
		pg_free(full_path);

		/* parse timeline and segment number from file name */
		XLogFromFileName(fname, &timeline_id, &segno);

		/* Set start position to begin from */
		XLogSegNoOffsetToRecPtr(segno, 0, startptr);
	}

	/* we don't know what to print */
	if (XLogRecPtrIsInvalid(startptr))
	{
		fprintf(stderr, "%s: no start log position given.\n", progname);
		exit(1);
	}

	/* File and start position are here, begin the parsing */
	//xlogreader_state = XLogReaderAllocate(XLogDumpReadPage, &private);

	for (;;)
	{
		//XLogReadRecord stuff
	}

	exit(0);
}
