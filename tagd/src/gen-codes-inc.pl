#!/usr/bin/perl
## generates input file of tagd_codes case statments that
## map to strings to be included by codes.h

use strict;
use warnings;

my $PRG = $0;

sub usage {
	my $msg = shift;
	print "$msg\n" if $msg;
	print "usage: $PRG <TAGD_CODES|PART_OF_SPEECH> <codes.h>\n";
	exit 1;
}

@ARGV == 2 or usage();

my %types = (
	TAGD_CODES => 1,
	PART_OF_SPEECH => 1
);

my $block_type = $ARGV[0] and $types{$ARGV[0]} or usage("no such block type: $ARGV[0]");
my $BLOCK_START = $block_type."_START";
my $BLOCK_END = $block_type."_END";

open FH, $ARGV[1] or usage("open $ARGV[1] failed: $!");

my $codes_block = 0;
while (<FH>) {
	if (/$BLOCK_START/) {
		$codes_block = 1;
		next;
	}

	last if (/$BLOCK_END/);

	next if /^\s*\/\//;  # ignore comments at start of line

	if ($codes_block) {
		if (/([A-Z_]+)/) {
			print "case tagd::$1:\treturn \"$1\";\n";
		}
	}
}

close(FH);
