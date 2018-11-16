# Do basic sanity checks of all actions supported by pg_checksums
# using an initialized cluster.

use strict;
use warnings;
use PostgresNode;
use TestLib;
use Test::More tests => 12;

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
command_fails(['pg_checksums', '--action', 'verify', '-d', $pgdata],
	      "pg_checksums requires server to be cleanly stopped");
$node->stop;

# Run verification, which should fail as checksums are disabled.
command_fails(['pg_checksums', '--action', 'verify', '-d', $pgdata],
	      "checksums disabled so no verification");

# Enable checksums on cluster.
command_ok(['pg_checksums', '--action', 'enable', '-d', $pgdata],
	   "checksums enabled");

# Control file should know that checksums are enabled.
command_like(['pg_controldata', $pgdata],
	     qr/Data page checksum version:.*1/,
	     'checksums are enabled in control file');

# This time verification is able to work.
command_ok(['pg_checksums', '--action', 'verify', '-d', $pgdata],
	   "checksums enabled so verification happens");

# Disable checksums.
command_ok(['pg_checksums', '--action', 'disable', '-d', $pgdata],
	   "checksums disabled");

# Control file should know that checksums are disabled.
command_like(['pg_controldata', $pgdata],
	     qr/Data page checksum version:.*0/,
	     'checksums are enabled in control file');
