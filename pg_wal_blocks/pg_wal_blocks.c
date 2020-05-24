/*-------------------------------------------------------------------------
 *
 * pg_wal_blocks.c
 *		Tracker of relation blocks touched by WAL records
 *
 * Copyright (c) 1996-2020, PostgreSQL Global Development Group
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

/* Structures for XLOG reader callback */
typedef struct XLogReadBlockPrivate
{
	const char	   *full_path;
	TimeLineID		timeline;
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

static void
XLogOpenSegment(XLogReaderState *state, XLogSegNo nextSegNo,
				TimeLineID *tli_p)
{
	XLogReadBlockPrivate *private =
		(XLogReadBlockPrivate *) state->private_data;

	state->seg.ws_file = open(private->full_path, O_RDONLY | PG_BINARY, 0);
	if (state->seg.ws_file < 0)
	{
		fprintf(stderr, "could not open file \"%s\": %m\n",
				private->full_path);
		exit(EXIT_FAILURE);
	}
}

static void
XLogCloseSegment(XLogReaderState *state)
{
	if (state->seg.ws_file != -1)
	{
		close(state->seg.ws_file);
		state->seg.ws_file = -1;
	}
}

/* XLogreader callback function, to read a WAL page */
static int
XLogReadPageBlock(XLogReaderState *state, XLogRecPtr targetPagePtr,
				  int reqLen, XLogRecPtr targetRecPtr, char *readBuf)
{
	XLogReadBlockPrivate *private =
		(XLogReadBlockPrivate *) state->private_data;
	WALReadError errinfo;

	/*
	 * Note that WalRead() is in charge of opening the segment to read and
	 * it will trigger the callback to open a segment.
	 */
	if (!WALRead(state, readBuf, targetPagePtr, XLOG_BLCKSZ,
				 private->timeline, &errinfo))
	{
		char        fname[MAXPGPATH];
		WALOpenSegment *seg = &errinfo.wre_seg;

		XLogFileName(fname, seg->ws_tli, seg->ws_segno,
					 state->segcxt.ws_segsize);

		if (errinfo.wre_errno != 0)
		{
			errno = errinfo.wre_errno;
			fprintf(stderr, "could not read from file %s, offset %u: %s\n",
					fname, errinfo.wre_off, strerror(errno));
		}
		else
			fprintf(stderr, "could not read from file %s, offset %u: read %d of %zu\n",
					fname, errinfo.wre_off, errinfo.wre_read,
					(Size) errinfo.wre_req);

		exit(EXIT_FAILURE);
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
do_wal_parsing(TimeLineID timeline)
{
	XLogReadBlockPrivate private;
	XLogRecord *record;
	XLogReaderState *state;
	char *errormsg;
	XLogRecPtr first_record;
	XLogRecPtr next_record;

	private.full_path = full_path;
	private.timeline = timeline;

	/* Set the first record to look at */
	XLogSegNoOffsetToRecPtr(segno, 0, WalSegSz, first_record);
	state = XLogReaderAllocate(WalSegSz, NULL,
							   XL_ROUTINE(.page_read = &XLogReadPageBlock,
										  .segment_open = &XLogOpenSegment,
										  .segment_close = &XLogCloseSegment),
							   &private);

	/* first find a valid recptr to start from */
	next_record = XLogFindNextRecord(state, first_record);

	if (XLogRecPtrIsInvalid(next_record))
	{
		fprintf(stderr, "could not find valid first record after %X/%X\n",
				(uint32) (first_record >> 32), (uint32) first_record);
		exit(EXIT_FAILURE);
	}

	XLogBeginRead(state, next_record);

	/* Loop through all the records */
	do
	{
		/* Move on to next record */
		record = XLogReadRecord(state, &errormsg);
		if (errormsg)
			fprintf(stderr, "error reading xlog record: %s\n", errormsg);

		/* extract block information for this record */
		extract_block_info(state);
	} while (record != NULL);

	XLogReaderFree(state);
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
	TimeLineID	timeline;

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
		XLogFromFileName(fname, &timeline, &segno, WalSegSz);
	}

	if (full_path == NULL)
	{
		fprintf(stderr, "%s: no input file defined.\n", progname);
		exit(1);
	}

	/* File to parse is here, so begin */
	do_wal_parsing(timeline);
	exit(0);
}
