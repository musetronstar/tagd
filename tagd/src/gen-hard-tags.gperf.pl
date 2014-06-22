#!/usr/bin/perl
## generates a gperf input file of hard tags
## The DATA (__END__) section of this file contains gperf declarations heading

use strict;
use warnings;
use open qw(:std :utf8);

@ARGV > 0 or die "usage: $0 <hard-tags.h>";

# gperf declarations heading
while (<DATA>) { print $_ }

my $rank = 0;
while (<>) {
    #chomp;
    s/\s+$//g;

    if (/^#define\s*(\S*)\s*\"(\S*)\"\s*\/\/gperf(.*)/) {
		# <tag id>, <super_object>, <rank>
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
