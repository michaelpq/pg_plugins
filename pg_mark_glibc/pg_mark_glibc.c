/*-------------------------------------------------------------------------
 *
 * pg_mark_glibc.c
 *		Register the system's version of glibc into a PostgreSQL data
 *		folder.  To be honest this is just a lazy wrapper on top of
 *		gnu_get_libc_version():
 *		http://man7.org/linux/man-pages/man3/gnu_get_libc_version.3.html
 *
 * Copyright (c) 1996-2022, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		pg_mark_glibc/pg_mark_glibc.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres_fe.h"

#include <gnu/libc-version.h>

#include "getopt_long.h"

const char *progname;

/* Global parameters */
static char *DataDir = NULL;

/* Garker file holding glibc version */
const char *marker_file_name = "GLIBC_VERSION";

static void
usage(const char *progname)
{
	printf("%s marks the current version of glibc used into the given data folder.\n\n",
		   progname);
	printf("Usage:\n %s [OPTION] ... [DATADIR]\n\n", progname);
	printf("Options:\n");
	printf("  [-D, --pgdata=]DATADIR  data directory\n");
	printf("  -V, --version           output version information, then exit\n");
	printf("  -?, --help              show this help, then exit\n");
	printf("\n");
	printf("Report bugs to https://github.com/michaelpq/pg_plugins.\n");
}

static void
write_marker_file(void)
{
	FILE       *marker_file;
	char       *path;

	path = psprintf("%s/%s", DataDir, marker_file_name);

	if ((marker_file = fopen(path, PG_BINARY_W)) == NULL)
	{
		fprintf(stderr, _("could not open file \"%s\": %m"), path);
		exit(1);
	}
	if (fprintf(marker_file, "%s\n", gnu_get_libc_version()) < 0 ||
		fclose(marker_file))
	{
		fprintf(stderr, _("could not write file \"%s\": %m"), path);
		exit(1);
	}
	free(path);
}

int
main(int argc, char **argv)
{
	static struct option long_options[] = {
		{"pgdata", required_argument, NULL, 'D'},
		{"version", no_argument, NULL, 'V'},
		{"verbose", no_argument, NULL, 'v'},
		{NULL, 0, NULL, 0}
	};
	int		c;
	int		option_index;

	progname = get_progname(argv[0]);

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
			puts("pg_mark_glibc (PostgreSQL)" PG_VERSION);
			exit(0);
		}
	}

	while ((c = getopt_long(argc, argv, "D:v", long_options, &option_index)) != -1)
	{
		switch (c)
		{
			case 'D':
				DataDir = optarg;
				break;
			case '?':
			default:
				fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
				exit(1);
		}
	}

	if (DataDir == NULL)
	{
		if (optind < argc)
			DataDir = argv[optind++];
		else
			DataDir = getenv("PGDATA");

		/* If no DataDir was specified, and none could be found, error out */
		if (DataDir == NULL)
		{
			fprintf(stderr, _("%s: no data directory specified\n"), progname);
			fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
			exit(1);
		}
	}

	/* Complain if any arguments remain */
	if (optind < argc)
	{
		fprintf(stderr, _("%s: too many command-line arguments (first is \"%s\")\n"),
				progname, argv[optind]);
		fprintf(stderr, _("Try \"%s --help\" for more information.\n"),
				progname);
		exit(1);
	}

	/* do the actual work */
	write_marker_file();

	exit(0);
}
