use strict;
use warnings;
use TestLib;
use Test::More tests => 13;

program_help_ok('pg_checksums');
program_version_ok('pg_checksums');
program_options_handling_ok('pg_checksums');

command_fails([ 'pg_checksums', '-A', 'incorrect_action' ],
	      '--action with incorrect value');
command_fails([ 'pg_checksums', '-A', 'enable', '-A', 'disable' ],
	      '--action already specified');
command_fails([ 'pg_checksums', '-D' ], 'no data directory specified');
my $tempdir = TestLib::tempdir;
command_fails([ 'pg_checksums', '-D', "$tempdir" ], 'no action specified');
command_fails([ 'pg_checksums', 'foo', 'bar' ], 'too many arguments');
