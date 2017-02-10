# Generate a conversion table using a Unicode data file as input, saving
# in the output as a header file in the location specified by the caller
# of this script.

use strict;
use warnings;

use utf8;
use open ':std', ':encoding(UTF-8)';

# Convert a single unicode character using code given by caller to
# hexadecimal and return it to caller. This is useful to treat the
# first or sixth columns of UnicodeData.txt and print it in
# hexadecimal format.
sub get_hexa_code
{
	my $code = shift;

	# First generate a unicode string, and then convert it.
	my $s = sprintf("\\u%s", $code);
	$s =~ s/\\u(....)/chr(hex($1))/eg;
	# Encode it to get the set of bytes wanted.
	utf8::encode($s);

	# Compute result
	my $result = "";
	for (my $key = 0; $key < length($s); $key++)
	{
		my $char = substr($s, $key, 1);
		$char = sprintf("%x", ord($char));
		$result = $result . $char;
	}

	return $result;
}

die "Usage: $0 INPUT_FILE OUTPUT_PUT\n" if @ARGV != 2;
my $input_file = $ARGV[0];
my $output_file = $ARGV[1];

# Count number of lines in input file to get size of table.
my $input_lines = 0;
open(my $FH, $input_file) or die "Could not open input file $input_file: $!.";
while (my $line = <$FH>)
{
	my @elts = split(';', $line);
	my $code = get_hexa_code($elts[0]);

	# Skip codes longer than 4 bytes, or 8 characters.
	next if length($code) > 8;
	$input_lines++;
}
close $FH;

# Open the input file and treat it line by line, one for each Unicode
# character.
open my $INPUT, $input_file or die "Could not open input file $input_file: $!";
open my $OUTPUT, "> $output_file" or die "Could not open output file $output_file: $!\n";

# Print header of output file.
print $OUTPUT <<HEADER;
/*
 * File auto-generated from generate_conv.pl, do not edit. There is
 * deliberately not an #ifndef PG_UTF8_TABLE_H here.
 */
typedef struct
{
    uint32      utf;        /* UTF-8 */
    uint8       class;      /* combining class of character */
    uint32      codes[18];   /* decomposition codes */
} pg_utf_decomposition;

/* conversion table */
HEADER
print $OUTPUT "static const pg_utf_decomposition SASLPrepConv[ $input_lines ] = {\n";

my $first_item = 1;
while ( my $line = <$INPUT> )
{
	# Split the line wanted and get the fields needed:
	# - Unicode number
	# - Combining class
	# - Decomposition table
	my @elts = split(';', $line);
	my $code = get_hexa_code($elts[0]);
	my $class = sprintf("0x%02x", $elts[3]);
	my $decom = $elts[5];

	# Skip codes longer than 4 bytes, or 8 characters.
	next if length($code) > 8;

	# Print a comma for all items except the first one.
	if ($first_item)
	{
	    $first_item = 0;
	}
	else
	{
	    print $OUTPUT ",\n";
	}

	# Now print a single entry in the conversion table.
	print $OUTPUT "\t{";
	# Code number
	print $OUTPUT "0x$code, ";
	# Combining class
	print $OUTPUT "$class, {";

	# Remove decomposition type if any, keep only character codes and
	# then print them.
	$decom =~ s/\<[^][]*\>//g;
	my @decom_elts = split(" ", $decom);
	my $first_decom = 1;
	foreach(@decom_elts)
	{
		if ($first_decom)
		{
		    $first_decom = 0;
		}
		else
		{
		    print $OUTPUT ", ";
		}
		my $decom_data = get_hexa_code($_);
		print $OUTPUT "0x$decom_data";
	}
	print $OUTPUT "}}";
}

print $OUTPUT "\n};\n";
close $OUTPUT;
close $INPUT;
