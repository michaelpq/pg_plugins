# Do basic sanity checks of all actions supported by pg_checksums
# using an initialized cluster.

use strict;
use warnings;
use PostgresNode;
use TestLib;
use Test::More tests => 17;

# Run a simple command and grab its stdout output into a result given back
# to caller.
sub run_simple_command
{
	my ($cmd, $test_name) = @_;
	my $stdoutfile = File::Temp->new();
	my $stderrfile = File::Temp->new();
	my $result = IPC::Run::run $cmd, '>', $stdoutfile, '2>', $stderrfile;
	my $stdout = slurp_file($stdoutfile);

	ok($result, $test_name);
	chomp($stdout);
	return $stdout;
}

# Look at the binary position of pg_config and enforce the position of
# pg_regress to what is installed. This is hacky, but prove_installcheck
# is not really smart here.
my $stdout = run_simple_command(['pg_config', '--libdir'],
		"fetch library directory using pg_config");
print "LIBDIR path found as $stdout\n";
$ENV{PG_REGRESS} = "$stdout/pgxs/src/test/regress/pg_regress";

# Initialize node with checksums disabled and some data.
my $node = get_new_node('node');
$node->init;
$node->start;
my $pgdata = $node->data_dir;

# There are temporary files and folders with dummy contents, which
# should be ignored by the scan.
append_to_file "$pgdata/global/pgsql_tmp_123", "foo";
mkdir "$pgdata/global/pgsql_tmp";
append_to_file "$pgdata/global/pgsql_tmp/1.1", "foo";

# Error, server must be shut down cleanly.
command_fails(['pg_checksums', '--action', 'verify', '-d', '-D', $pgdata],
	      "pg_checksums requires server to be cleanly stopped");
$node->stop;

# Run verification, which should fail as checksums are disabled.
command_fails(['pg_checksums', '--action', 'verify', '-d', '-D', $pgdata],
	      "checksums disabled so no verification");

# Checksums already disabled
command_fails(['pg_checksums', '--action', 'disable', '-d', '-D', $pgdata],
	      "checksums already disabled");

# Enable checksums on cluster.
command_ok(['pg_checksums', '--action', 'enable', '-d', '-D', $pgdata],
	   "checksums enabled");

# Checksums already enabled
command_fails(['pg_checksums', '--action', 'enable', '-d', '-D', $pgdata],
	      "checksums already enabled");

# Control file should know that checksums are enabled.
command_like(['pg_controldata', $pgdata],
	     qr/Data page checksum version:.*1/,
	     'checksums are enabled in control file');

# This time verification is able to work.
command_ok(['pg_checksums', '--action', 'verify', '-d', '-D', $pgdata],
	   "checksums enabled so verification happens");

# Disable checksums.
command_ok(['pg_checksums', '--action', 'disable', '-d', '-D', $pgdata],
	   "checksums disabled");

# Control file should know that checksums are disabled.
command_like(['pg_controldata', $pgdata],
	     qr/Data page checksum version:.*0/,
	     'checksums are enabled in control file');

# Enable checksums once again before stressing corruption checks.
run_log(['pg_checksums', '--action', 'enable', '-d', '-D', $pgdata]);
$node->start;

# Create table to corrupt and get its relfilenode
$node->safe_psql('postgres',
	"SELECT a INTO corrupt1 FROM generate_series(1,10000) AS a;
	ALTER TABLE corrupt1 SET (autovacuum_enabled=false);");
my $file_corrupted = $node->safe_psql('postgres',
	"SELECT pg_relation_filepath('corrupt1')");
my $relfilenode_corrupted =  $node->safe_psql('postgres',
	"SELECT relfilenode FROM pg_class WHERE relname = 'corrupt1';");

# Set page header and block size
my $pageheader_size = 24;
my $block_size = $node->safe_psql('postgres', 'SHOW block_size;');
$node->stop;

# Time to create some corruption
open my $file, '+<', "$pgdata/$file_corrupted";
seek($file, $pageheader_size, 0);
syswrite($file, '\0\0\0\0\0\0\0\0\0');
close $file;

$node->command_checks_all([ 'pg_checksums', '-A', 'verify', '-D', $pgdata],
			  1,
			  [qr/Bad checksums:.*1/],
			  [qr/checksum verification failed/],
			  'fails with corrupted data');
