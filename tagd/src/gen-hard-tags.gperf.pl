#!/usr/bin/perl
## generates a gperf input file of hard tags
## DATA section of this file contains gperf declarations

use strict;
use warnings;
use open qw(:std :utf8);

use Net::IDN::Encode 'domain_to_ascii';
my $print_private = 1;

@ARGV > 0 or die "usage: $0 <hard-tags.h>";

# gperf declarations header
while (<DATA>) {
        print $_ 
}

my %tlds;
my $managed_by = undef;
my $idn;
my $tld;
my $rank = 0;
while (<>) {

    # strip comments, leading ws, blank lines...
    #chomp;
    s/\s+$//g;
    if (/^\s*$/) {
        # print "\n";
        next;
    }

    if (/^#define\s*(\S*)\s*\"(\S*)\"\s*\/\/gperf(.*)/) {
        print "$2, $3, $rank\n";
		$rank++;
        next;
    }
}

__END__
%{
#include "tagd.h"
%}
// declarations
%struct-type
%7bit
%language=C++
%define class-name hard_tag_hash
%define slot-name key
%define lookup-function-name lookup

struct hard_tag_hash_value {
    const char *key;
    const char *super;
    tagd::part_of_speech pos;
	uint32_t rank;
};
%%
