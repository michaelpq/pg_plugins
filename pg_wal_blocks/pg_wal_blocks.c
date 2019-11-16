/*-------------------------------------------------------------------------
 *
 * pg_wal_blocks.c
 *		Tracker of relation blocks touched by WAL records
 *
 * Copyright (c) 1996-2019, PostgreSQL Global Development Group
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

/* Global parameters */
static bool verbose = false;
static char *full_path = NULL;
static uint32 WalSegSz = DEFAULT_XLOG_SEG_SIZE;	/* should be settable */

/* Data regarding input WAL to parse */
static XLogSegNo segno = 0;

/* Parsing status */
static int xlogreadfd = -1; /* File descriptor of opened WAL segment */

/* Structures for XLOG reader callback */
typedef struct XLogReadBlockPrivate
{
	const char	   *full_path;
} XLogReadBlockPrivate;
static int XLogReadPageBlock(XLogReaderState *xlogreader,
							 XLogRecPtr targetPagePtr,
							 int reqLen, XLogRecPtr targetRecPtr,
							 char *readBuf);

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
	char	   *sep;

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

/* XLogreader callback function, to read a WAL page */
static int
XLogReadPageBlock(XLogReaderState *xlogreader, XLogRecPtr targetPagePtr,
				  int reqLen, XLogRecPtr targetRecPtr, char *readBuf)
{
	XLogReadBlockPrivate *private =
		(XLogReadBlockPrivate *) xlogreader->private_data;
	uint32      targetPageOff;

	targetPageOff = targetPagePtr % WalSegSz;

	if (xlogreadfd < 0)
	{
		xlogreadfd = open(private->full_path, O_RDONLY | PG_BINARY, 0);

		if (xlogreadfd < 0)
		{
			fprintf(stderr, "could not open file \"%s\": %s\n",
					private->full_path,
					strerror(errno));
			return -1;
		}
	}

	/*
	 * At this point, we have the right segment open.
	 */
	Assert(xlogreadfd != -1);

	/* Read the requested page */
	if (lseek(xlogreadfd, (off_t) targetPageOff, SEEK_SET) < 0)
	{
		fprintf(stderr, "could not seek in file \"%s\": %s\n",
				private->full_path,
				strerror(errno));
		return -1;
	}

	if (read(xlogreadfd, readBuf, XLOG_BLCKSZ) != XLOG_BLCKSZ)
	{
		fprintf(stderr, "could not read from file \"%s\": %s\n",
				private->full_path,
				strerror(errno));
		return -1;
	}

	return XLOG_BLCKSZ;
}

/*
 * extract_block_info
 * Extract block information for given record.
 */
static void
extract_block_info(XLogReaderState *record)
{
	int block_id;

	for (block_id = 0; block_id <= record->max_block_id; block_id++)
	{
		RelFileNode rnode;
		ForkNumber forknum;
		BlockNumber blkno;

		if (!XLogRecGetBlockTag(record, block_id, &rnode, &forknum, &blkno))
			continue;

		/* We only care about the main fork */
		if (forknum != MAIN_FORKNUM)
			continue;

		/*
		 * Print information of block touched.
		 * TODO: more advanced logic (really needed?)
		 */
		fprintf(stderr, "Block touched: dboid = %u, relid = %u, block = %u\n",
				rnode.dbNode, rnode.relNode, blkno);
	}
}

/*
 * do_wal_parsing
 * Central part where the actual parsing work happens.
 */
static void
do_wal_parsing(void)
{
	XLogReadBlockPrivate private;
	XLogRecord *record;
	XLogReaderState *xlogreader;
	char *errormsg;
	XLogRecPtr first_record;

	private.full_path = full_path;

	/* Set the first record to look at */
	XLogSegNoOffsetToRecPtr(segno, 0, WalSegSz, first_record);
	xlogreader = XLogReaderAllocate(WalSegSz, NULL, XLogReadPageBlock,
									&private);
	first_record = XLogFindNextRecord(xlogreader, first_record);

	/* Loop through all the records */
	do
	{
		/* Move on to next record */
		record = XLogReadRecord(xlogreader, first_record, &errormsg);
		if (errormsg)
			fprintf(stderr, "error reading xlog record: %s\n", errormsg);

		/* after reading the first record, continue at next one */
		first_record = InvalidXLogRecPtr;

		/* extract block information for this record */
		extract_block_info(xlogreader);
	} while (record != NULL);

	XLogReaderFree(xlogreader);
	if (xlogreadfd != -1)
	{
		close(xlogreadfd);
		xlogreadfd = -1;
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
		char	   *directory = NULL;
		char	   *fname = NULL;
		int			fd;
		TimeLineID	timeline_id;

		full_path = pg_strdup(argv[optind]);

		split_path(full_path, &directory, &fname);

		fd = open(full_path, O_RDONLY | PG_BINARY, 0);
		if (fd < 0)
		{
			fprintf(stderr, "could not open file \"%s\"", fname);
			exit(1);
		}
		close(fd);

		/* parse timeline and segment number from file name */
		XLogFromFileName(fname, &timeline_id, &segno, WalSegSz);
	}

	if (full_path == NULL)
	{
		fprintf(stderr, "%s: no input file defined.\n", progname);
		exit(1);
	}

	/* File to parse is here, so begin */
	do_wal_parsing();
	exit(0);
}
