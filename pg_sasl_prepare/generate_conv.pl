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

# Script-specific and post composition that need to be excluded from the tables
# generated per http://www.unicode.org/reports/tr15/.
my @no_recomp_codes = (
	'0958',  # DEVANAGARI LETTER QA
	'0959',  # DEVANAGARI LETTER KHHA
	'095A',  # DEVANAGARI LETTER GHHA
	'095B',  # DEVANAGARI LETTER ZA
	'095C',  # DEVANAGARI LETTER DDDHA
	'095D',  # DEVANAGARI LETTER RHA
	'095E',  # DEVANAGARI LETTER FA
	'095F',  # DEVANAGARI LETTER YYA
	'09DC',  # BENGALI LETTER RRA
	'09DD',  # BENGALI LETTER RHA
	'09DF',  # BENGALI LETTER YYA
	'0A33',  # GURMUKHI LETTER LLA
	'0A36',  # GURMUKHI LETTER SHA
	'0A59',  # GURMUKHI LETTER KHHA
	'0A5A',  # GURMUKHI LETTER GHHA
	'0A5B',  # GURMUKHI LETTER ZA
	'0A5E',  # GURMUKHI LETTER FA
	'0B5C',  # ORIYA LETTER RRA
	'0B5D',  # ORIYA LETTER RHA
	'0F43',  # TIBETAN LETTER GHA
	'0F4D',  # TIBETAN LETTER DDHA
	'0F52',  # TIBETAN LETTER DHA
	'0F57',  # TIBETAN LETTER BHA
	'0F5C',  # TIBETAN LETTER DZHA
	'0F69',  # TIBETAN LETTER KSSA
	'0F76',  # TIBETAN VOWEL SIGN VOCALIC R
	'0F78',  # TIBETAN VOWEL SIGN VOCALIC L
	'0F93',  # TIBETAN SUBJOINED LETTER GHA
	'0F9D',  # TIBETAN SUBJOINED LETTER DDHA
	'0FA2',  # TIBETAN SUBJOINED LETTER DHA
	'0FA7',  # TIBETAN SUBJOINED LETTER BHA
	'0FAC',  # TIBETAN SUBJOINED LETTER DZHA
	'0FB9',  # TIBETAN SUBJOINED LETTER KSSA
	'FB1D',  # HEBREW LETTER YOD WITH HIRIQ:
	'FB1F',  # HEBREW LIGATURE YIDDISH YOD YOD PATAH
	'FB2A',  # HEBREW LETTER SHIN WITH SHIN DOT
	'FB2B',  # HEBREW LETTER SHIN WITH SIN DOT
	'FB2C',  # HEBREW LETTER SHIN WITH DAGESH AND SHIN DOT
	'FB2D',  # HEBREW LETTER SHIN WITH DAGESH AND SIN DOT
	'FB2E',  # HEBREW LETTER ALEF WITH PATAH
	'FB2F',  # HEBREW LETTER ALEF WITH QAMATS
	'FB30',  # HEBREW LETTER ALEF WITH MAPIQ
	'FB31',  # HEBREW LETTER BET WITH DAGESH
	'FB32',  # HEBREW LETTER GIMEL WITH DAGESH
	'FB33',  # HEBREW LETTER DALET WITH DAGESH
	'FB34',  # HEBREW LETTER HE WITH MAPIQ
	'FB35',  # HEBREW LETTER VAV WITH DAGESH
	'FB36',  # HEBREW LETTER ZAYIN WITH DAGESH
	'FB38',  # HEBREW LETTER TET WITH DAGESH
	'FB39',  # HEBREW LETTER YOD WITH DAGESH
	'FB3A',  # HEBREW LETTER FINAL KAF WITH DAGESH
	'FB3B',  # HEBREW LETTER KAF WITH DAGESH
	'FB3C',  # HEBREW LETTER LAMED WITH DAGESH
	'FB3E',  # HEBREW LETTER MEM WITH DAGESH
	'FB40',  # HEBREW LETTER NUN WITH DAGESH
	'FB41',  # HEBREW LETTER SAMEKH WITH DAGESH
	'FB43',  # HEBREW LETTER FINAL PE WITH DAGESH
	'FB44',  # HEBREW LETTER PE WITH DAGESH
	'FB46',  # HEBREW LETTER TSADI WITH DAGESH
	'FB47',  # HEBREW LETTER QOF WITH DAGESH
	'FB48',  # HEBREW LETTER RESH WITH DAGESH
	'FB49',  # HEBREW LETTER SHIN WITH DAGESH
	'FB4A',  # HEBREW LETTER TAV WITH DAGESH
	'FB4B',  # HEBREW LETTER VAV WITH HOLAM
	'FB4C',  # HEBREW LETTER BET WITH RAFE
	'FB4D',  # HEBREW LETTER KAF WITH RAFE
	'FB4E',  # HEBREW LETTER PE WITH RAFE
	# post composition exclusion
	'2ADC',  #  FORKING
	'1D15E', # MUSICAL SYMBOL HALF NOTE
	'1D15F', # MUSICAL SYMBOL QUARTER NOTE
	'1D160', # MUSICAL SYMBOL EIGHTH NOTE
	'1D161', # MUSICAL SYMBOL SIXTEENTH NOTE
	'1D162', # MUSICAL SYMBOL THIRTY-SECOND NOTE
	'1D163', # MUSICAL SYMBOL SIXTY-FOURTH NOTE
	'1D164', # MUSICAL SYMBOL ONE HUNDRED TWENTY-EIGHTH NOTE
	'1D1BB', # MUSICAL SYMBOL MINIMA
	'1D1BC', # MUSICAL SYMBOL MINIMA BLACK
	'1D1BD', # MUSICAL SYMBOL SEMIMINIMA WHITE
	'1D1BE', # MUSICAL SYMBOL SEMIMINIMA BLACK
	'1D1BF', # MUSICAL SYMBOL FUSA WHITE
	'1D1C0'  # MUSICAL SYMBOL FUSA BLACK
    );

# Count number of lines in input file to get size of table.
my $input_lines = 0;
open(my $FH, $input_file) or die "Could not open input file $input_file: $!.";
while (my $line = <$FH>)
{
	my @elts = split(';', $line);
	my $code = get_hexa_code($elts[0]);

	# Skip codes longer than 4 bytes, or 8 characters.
	next if length($code) > 8;

	# Skip codes that cannot be composed
	my $found_no_recomp = 0;
	foreach my $lcode  (@no_recomp_codes)
	{
		if ($lcode eq $elts[0])
		{
			$found_no_recomp = 1;
			last;
		}
	}
	next if $found_no_recomp;

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

	# Skip codes that cannot be composed
	my $found_no_recomp = 0;
	foreach my $lcode  (@no_recomp_codes)
	{
		if ($lcode eq $elts[0])
		{
			$found_no_recomp = 1;
			last;
		}
	}
	next if $found_no_recomp;

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
