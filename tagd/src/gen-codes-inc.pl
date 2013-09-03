#!/usr/bin/perl
## generates input file of tagd_codes case statments that
## map to strings to be included by codes.h

use strict;
use warnings;

@ARGV > 0 or die "usage: $0 <codes.h>";

my $codes_block = 0;
while (<>) {
	if (/TAGD_CODES_START/) {
		$codes_block = 1;
		next;
	}

	last if (/TAGD_CODES_END/);

	next if /^\s*\/\//;  # ignore comments at start of line

	if ($codes_block) {
		if (/([A-Z_]+)/) {
			print "case tagd::$1:\treturn \"$1\";\n";
		}
	}
}
