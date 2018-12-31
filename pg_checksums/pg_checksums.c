/*------------------------------------------------------------------------
 *
 * pg_checksums.c
 *		Handle page-level checksums in an offline cluster
 *
 *	Copyright (c) 2010-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *		pg_checksums/pg_checksums.c
 *
 *------------------------------------------------------------------------
 */

#include "postgres_fe.h"

#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

#include "catalog/pg_control.h"
#include "common/controldata_utils.h"
#include "common/file_perm.h"
#include "common/file_utils.h"
#include "getopt_long.h"
#include "storage/bufpage.h"
#include "storage/checksum.h"
#include "storage/checksum_impl.h"

static int64 files = 0;
static int64 blocks = 0;
static int64 badblocks = 0;
static ControlFileData *ControlFile;

static bool debug = false;
static bool do_sync = true;

typedef enum
{
	PG_ACTION_NONE,
	PG_ACTION_DISABLE,
	PG_ACTION_ENABLE,
	PG_ACTION_VERIFY
} ChecksumAction;

/* Filename components, taken from upstream's storage/fd.h */
#define PG_TEMP_FILES_DIR "pgsql_tmp/"
#define PG_TEMP_FILE_PREFIX "pgsql_tmp"

static ChecksumAction action = PG_ACTION_NONE;
static char	   *DataDir = NULL;

static const char *progname;

static void
usage()
{
	printf(_("%s verifies page level checksums in offline PostgreSQL database cluster.\n\n"), progname);
	printf(_("Usage:\n"));
	printf(_("  %s [OPTION] [DATADIR]\n"), progname);
	printf(_("\nOptions:\n"));
	printf(_(" [-D] DATADIR    data directory\n"));
	printf(_("  -A, --action   action to take on the cluster, can be set as\n"));
	printf(_("                 \"verify\", \"disable\" and \"enable\"\n"));
	printf(_("  -d, --debug    debug output, listing all checked blocks\n"));
	printf(_("      --no-sync  do not wait for changes to be written safely to disk\n"));
	printf(_("  -V, --version  output version information, then exit\n"));
	printf(_("  -?, --help     show this help, then exit\n"));
	printf(_("\nIf no data directory (DATADIR) is specified, "
			 "the environment variable PGDATA\nis used.\n\n"));
	printf(_("Report bugs to https://github.com/michaelpq/pg_plugins.\n"));
}

static const char *skip[] = {
	"pg_control",
	"pg_filenode.map",
	"pg_internal.init",
	"PG_VERSION",
	NULL,
};

static void
updateControlFile(void)
{
    char        buffer[PG_CONTROL_FILE_SIZE];
	char		path[MAXPGPATH];
	int			fd;

	Assert(action == PG_ACTION_ENABLE ||
		   action == PG_ACTION_DISABLE);

	/*
	 * Disable or enable checksums depending on action taken.
	 */
	if (action == PG_ACTION_ENABLE)
		ControlFile->data_checksum_version = PG_DATA_CHECKSUM_VERSION;
	else if (action == PG_ACTION_DISABLE)
		ControlFile->data_checksum_version = 0;

	/*
     * For good luck, apply the same static assertions as in backend's
     * WriteControlFile().
     */
	StaticAssertStmt(sizeof(ControlFileData) <= PG_CONTROL_MAX_SAFE_SIZE,
					 "pg_control is too large for atomic disk writes");
	StaticAssertStmt(sizeof(ControlFileData) <= PG_CONTROL_FILE_SIZE,
					 "sizeof(ControlFileData) exceeds PG_CONTROL_FILE_SIZE");

	/* Recalculate CRC of control file */
	INIT_CRC32C(ControlFile->crc);
	COMP_CRC32C(ControlFile->crc,
				(char *) ControlFile,
				offsetof(ControlFileData, crc));
	FIN_CRC32C(ControlFile->crc);

	/*
     * Write out PG_CONTROL_FILE_SIZE bytes into pg_control by zero-padding
     * the excess over sizeof(ControlFileData), to avoid premature EOF related
     * errors when reading it.
     */
	memset(buffer, 0, PG_CONTROL_FILE_SIZE);
	memcpy(buffer, ControlFile, sizeof(ControlFileData));

	snprintf(path, sizeof(path), "%s/global/pg_control", DataDir);

	/* open and write */
	fd = open(path, O_WRONLY | O_CREAT | PG_BINARY, pg_file_create_mode);
	if (fd < 0)
	{
		fprintf(stderr, "could not open control file \"%s\": %s\n",
				path, strerror(errno));
		exit(1);
	}

	if (write(fd, buffer, PG_CONTROL_FILE_SIZE) != PG_CONTROL_FILE_SIZE)
	{
		fprintf(stderr, "could not write to control file \"%s\": %s\n",
				path, strerror(errno));
		exit(1);
	}

	if (close(fd) < 0)
	{
		fprintf(stderr, "could not close control file \"%s\": %s\n",
				path, strerror(errno));
		exit(1);
	}
}

static bool
skipfile(char *fn)
{
	const char **f;

	if (strcmp(fn, ".") == 0 ||
		strcmp(fn, "..") == 0)
		return true;

	for (f = skip; *f; f++)
		if (strcmp(*f, fn) == 0)
			return true;

	/* Skip temporary files */
	if (strncmp(fn,
				PG_TEMP_FILE_PREFIX,
				strlen(PG_TEMP_FILE_PREFIX)) == 0)
		return true;

	/* Skip temporary folders */
	if (strncmp(fn,
				PG_TEMP_FILES_DIR,
				strlen(PG_TEMP_FILES_DIR)) == 0)
		return true;

	return false;
}

static void
operate_file(char *fn, int segmentno)
{
	char		buf[BLCKSZ];
	PageHeader	header = (PageHeader) buf;
	int			f;
	int			blockno;

	Assert(action == PG_ACTION_ENABLE ||
		   action == PG_ACTION_VERIFY);

	f = open(fn, O_RDWR | PG_BINARY);
	if (f < 0)
	{
		fprintf(stderr, _("%s: could not open file \"%s\": %m\n"), progname, fn);
		exit(1);
	}

	files++;

	for (blockno = 0;; blockno++)
	{
		uint16		csum;
		int			r = read(f, buf, BLCKSZ);

		if (r == 0)
			break;
		if (r != BLCKSZ)
		{
			fprintf(stderr, _("%s: short read of block %d in file \"%s\", got only %d bytes\n"),
					progname, blockno, fn, r);
			exit(1);
		}
		blocks++;

		/* New pages have no checksum yet */
		if (PageIsNew(buf))
			continue;

		/*
		 * Operate on the block.  If enabling checksums, calculate the
		 * checksum and update directly the page in-place.  If checking
		 * them, compare the stored value and the calculated ones.
		 */
		csum = pg_checksum_page(buf, blockno + segmentno * RELSEG_SIZE);

		if (action == PG_ACTION_VERIFY)
		{
			if (csum != header->pd_checksum)
			{
				if (ControlFile->data_checksum_version == PG_DATA_CHECKSUM_VERSION)
					fprintf(stderr, _("%s: checksum verification failed in file \"%s\", block %d: calculated checksum %X but expected %X\n"),
							progname, fn, blockno, csum, header->pd_checksum);
				badblocks++;
			}
			else if (debug)
				fprintf(stderr, _("%s: checksum verified in file \"%s\", block %d: %X\n"),
						progname, fn, blockno, csum);
		}
		else if (action == PG_ACTION_ENABLE)
		{
			/* Update the buffer and rewrite it in place */
			header->pd_checksum = csum;

			/* Moving back in place is necessary! */
			if (lseek(f, blockno * BLCKSZ, SEEK_SET) < 0)
			{
				fprintf(stderr, "%s: could not lseek for block %d in file \"%s\": %s\n",
						progname, blockno, fn, strerror(errno));
				exit(1);
			}
			if (write(f, buf, BLCKSZ) != BLCKSZ)
			{
				fprintf(stderr, "%s: could not update checksum of block %d in file \"%s\": %s\n",
						progname, blockno, fn, strerror(errno));
				exit(1);
			}
		}
	}

	close(f);
}

static void
operate_directory(char *basedir, char *subdir)
{
	char		path[MAXPGPATH];
	DIR		   *dir;
	struct dirent *de;

	Assert(action == PG_ACTION_ENABLE ||
		   action == PG_ACTION_VERIFY);

	snprintf(path, sizeof(path), "%s/%s", basedir, subdir);
	dir = opendir(path);
	if (!dir)
	{
		fprintf(stderr, _("%s: could not open directory \"%s\": %m\n"),
				progname, path);
		exit(1);
	}
	while ((de = readdir(dir)) != NULL)
	{
		char		fn[MAXPGPATH + 1];
		struct stat st;

		if (skipfile(de->d_name))
			continue;

		snprintf(fn, sizeof(fn), "%s/%s", path, de->d_name);
		if (lstat(fn, &st) < 0)
		{
			fprintf(stderr, _("%s: could not stat file \"%s\": %m\n"),
					progname, fn);
			exit(1);
		}
		if (S_ISREG(st.st_mode))
		{
			char	   *forkpath,
					   *segmentpath;
			int			segmentno = 0;

			/*
			 * Cut off at the segment boundary (".") to get the segment number
			 * in order to mix it into the checksum. Then also cut off at the
			 * fork boundary, to get the relfilenode the file belongs to for
			 * filtering.
			 */
			segmentpath = strchr(de->d_name, '.');
			if (segmentpath != NULL)
			{
				*segmentpath++ = '\0';
				segmentno = atoi(segmentpath);
				if (segmentno == 0)
				{
					fprintf(stderr, _("%s: invalid segment number %d in filename \"%s\"\n"),
							progname, segmentno, fn);
					exit(1);
				}
			}

			forkpath = strchr(de->d_name, '_');
			if (forkpath != NULL)
				*forkpath++ = '\0';

			operate_file(fn, segmentno);
		}
#ifndef WIN32
		else if (S_ISDIR(st.st_mode) || S_ISLNK(st.st_mode))
#else
		else if (S_ISDIR(st.st_mode) || pgwin32_is_junction(fn))
#endif
			operate_directory(path, de->d_name);
	}
	closedir(dir);
}

int
main(int argc, char *argv[])
{
	static struct option long_options[] = {
		{"help", no_argument, NULL, '?'},
		{"action", required_argument, NULL, 'A'},
		{"debug", no_argument, NULL, 'd'},
		{"pgdata", no_argument, NULL, 'D'},
		{"version", no_argument, NULL, 'V'},
		{"no-sync", no_argument, NULL, 1},
		{NULL, 0, NULL, 0}
	};
	int			c;
	int			option_index;
	bool		crc_ok;

	set_pglocale_pgservice(argv[0], PG_TEXTDOMAIN("pg_checksums"));

	progname = get_progname(argv[0]);

	if (argc > 1)
	{
		if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-?") == 0)
		{
			usage();
			exit(0);
		}
		if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-V") == 0)
		{
			puts("pg_checksums (PostgreSQL) " PG_VERSION);
			exit(0);
		}
	}

	while ((c = getopt_long(argc, argv, "A:D:d", long_options,
							&option_index)) != -1)
	{
		switch (c)
		{
			case '?':
				fprintf(stderr, _("Try \"%s --help\" for more information.\n"), progname);
				exit(1);
			case 'd':
				debug = true;
				break;
			case 'D':
				DataDir = optarg;
				break;
			case 'A':
				/* Check for redundant options */
				if (action != PG_ACTION_NONE)
				{
					fprintf(stderr, _("%s: action already specified.\n"), progname);
					exit(1);
				}

				if (strcmp(optarg, "verify") == 0)
					action = PG_ACTION_VERIFY;
				else if (strcmp(optarg, "disable") == 0)
					action = PG_ACTION_DISABLE;
				else if (strcmp(optarg, "enable") == 0)
					action = PG_ACTION_ENABLE;
				else
				{
					fprintf(stderr, _("%s: incorrect action \"%s\" specified.\n"),
							progname, optarg);
					exit(1);
				}
				break;
			case 1:
				do_sync = false;
				break;
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

	/* Set mask based on PGDATA permissions */
	if (!GetDataDirectoryCreatePerm(DataDir))
	{
		fprintf(stderr, _("%s: unable to read permissions from \"%s\"\n"),
				progname, DataDir);
		exit(1);
	}

	umask(pg_mode_mask);

	/*
	 * Don't allow pg_checksums to be run as root, to avoid overwriting the
	 * ownership of files in the data directory. We need only check for root
	 * -- any other user won't have sufficient permissions to modify files in
	 * the data directory.  This does not matter for the "verify" mode, but
	 * let's be consistent.
	 */
#ifndef WIN32
	if (geteuid() == 0)
	{
		fprintf(stderr, _("cannot be executed by \"root\"\n"));
		fprintf(stderr, _("You must run %s as the PostgreSQL superuser.\n"),
				progname);
		exit(1);
	}
#endif

	/* Check if an action has been set */
	if (action == PG_ACTION_NONE)
	{
		fprintf(stderr, _("%s: no action specified"), progname);
		fprintf(stderr, _("Try \"%s --help\" for more information.\n"),
				progname);
		exit(1);
	}

	/* Check if cluster is running */
	ControlFile = get_controlfile(DataDir, progname, &crc_ok);
	if (!crc_ok)
	{
		fprintf(stderr, _("%s: pg_control CRC value is incorrect.\n"), progname);
		exit(1);
	}

	if (ControlFile->state != DB_SHUTDOWNED &&
		ControlFile->state != DB_SHUTDOWNED_IN_RECOVERY)
	{
		fprintf(stderr, _("%s: cluster must be shut down to verify checksums.\n"), progname);
		exit(1);
	}

	if (ControlFile->data_checksum_version == 0 &&
		action == PG_ACTION_VERIFY)
	{
		fprintf(stderr, _("%s: data checksums are disabled in cluster.\n"), progname);
		exit(1);
	}
	if (ControlFile->data_checksum_version == 0 &&
		action == PG_ACTION_DISABLE)
	{
		fprintf(stderr, _("%s: data checksums are already disabled in cluster.\n"), progname);
		exit(1);
	}
	if (ControlFile->data_checksum_version == PG_DATA_CHECKSUM_VERSION &&
		action == PG_ACTION_ENABLE)
	{
		fprintf(stderr, _("%s: data checksums are already enabled in cluster.\n"), progname);
		exit(1);
	}

	/*
	 * When disabling the data checksums, only update the control file and
	 * call it a day.
	 */
	if (action == PG_ACTION_DISABLE)
	{
		printf(_("Disabling checksums in cluster\n"));
		updateControlFile();
		if (do_sync)
			fsync_pgdata(DataDir, progname, PG_VERSION_NUM);
		return 0;
	}

	/* Operate all files */
	operate_directory(DataDir, "global");
	operate_directory(DataDir, "base");
	operate_directory(DataDir, "pg_tblspc");

	/*
	 * Print operation report, bad blocks are only tracked when verifying
	 * the checksum state.
	 */
	printf(_("Checksum operation completed\n"));
	printf(_("Data checksum version: %d\n"), ControlFile->data_checksum_version);
	printf(_("Files operated:  %" INT64_MODIFIER "d\n"), files);
	printf(_("Blocks operated: %" INT64_MODIFIER "d\n"), blocks);
	if (action == PG_ACTION_VERIFY)
	{
		printf(_("Bad checksums:  %" INT64_MODIFIER "d\n"), badblocks);

		if (badblocks > 0)
			return 1;
	}

	/*
	 * When enabling checksums, wait until the end the operation has
	 * completed to do the switch.
	 */
	if (action == PG_ACTION_ENABLE)
	{
		printf(_("Enabling checksums in cluster\n"));
		updateControlFile();
		if (do_sync)
			fsync_pgdata(DataDir, progname, PG_VERSION_NUM);
	}

	return 0;
}
