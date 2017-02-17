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

	# Skip characters with no decompositions and a class of 0.
	next if $elts[3] eq '0' && $elts[5] eq '';

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
	uint32	utf;		/* UTF-8 */
	uint8	class;		/* combining class of character */
	uint8	dec_size;	/* size of decomposition code list */
} pg_utf_decomposition;

/* conversion table */
HEADER
print $OUTPUT "static const pg_utf_decomposition SASLPrepConv[ $input_lines ] =\n{\n";

# Hash for decomposition tables made of string arrays (one for each
# character decomposition, classified by size).
my %decomp_tabs = ();

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

	# Skip characters with no decompositions and a class of 0.
	# to reduce the table size.
	next if $elts[3] eq '0' && $elts[5] eq '';

	# Print a comma for all items except the first one.
	if ($first_item)
	{
	    $first_item = 0;
	}
	else
	{
	    print $OUTPUT ",\n";
	}

	# Remove decomposition type if any, keep only character codes and
	# then print them.
	$decom =~ s/\<[^][]*\>//g;
	my @decom_elts = split(" ", $decom);

	# Now print a single entry in the conversion table.
	print $OUTPUT "\t{";
	# Code number
	print $OUTPUT "0x$code, ";
	# Combining class
	print $OUTPUT "$class, ";
	# Decomposition size
	# Print size of decomposition
	my $decom_size = scalar(@decom_elts);

	print $OUTPUT "$decom_size}";

	# If the character has no decomposition we are done.
	next if $decom_size == 0;

	# Now save the decompositions into a dedicated area that will
	# be written afterwards.  First build the entry dedicated to
	# a sub-table with the code and decomposition.
	my $first_decom = 1;
	my $decomp_string = "{";
	# Code number
	$decomp_string .= "0x$code, {";
	foreach(@decom_elts)
	{
		if ($first_decom)
		{
		    $first_decom = 0;
		}
		else
		{
		    $decomp_string .= ", ";
		}
		my $decom_data = get_hexa_code($_);
		$decomp_string .= "0x$decom_data";
	}
	$decomp_string .= "}}";
	# Store it in its dedicated list.
	push(@{ $decomp_tabs{$decom_size} }, $decomp_string);
}

print $OUTPUT "\n};\n\n\n";

# Print the decomposition tables by size.
foreach my $decomp_size (sort keys %decomp_tabs )
{
	my @decomp_entries = @{ $decomp_tabs{$decomp_size}};
	my $decomp_length = scalar(@decomp_entries);

	# First print the header.
	print $OUTPUT <<HEADER;
\n\n/* Decomposition table with entries of list length of $decomp_size */
typedef struct
{
	uint32	utf;		/* UTF-8 */
	uint32	decomp[$decomp_size];	/* size of decomposition code list */
} pg_utf_decomposition_size_$decomp_size;

static const pg_utf_decomposition_size_$decomp_size UtfDecomp_$decomp_size [ $decomp_length ] =
{
HEADER

	$first_item = 1;
	# Print each entry.
	foreach(@decomp_entries)
	{
		if ($first_item)
		{
		    $first_item = 0;
		}
		else
		{
		    print $OUTPUT ",\n";
		}
		print $OUTPUT "\t$_";
	}
	print $OUTPUT "\n};\n";
}

close $OUTPUT;
close $INPUT;
