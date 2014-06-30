#!/usr/bin/perl
## generates a gperf input file of hard tags
## The DATA (__END__) section of this file contains gperf declarations heading

use strict;
use warnings;
use open qw(:std :utf8);

@ARGV > 0 or die "usage: $0 <hard-tags.h>";

# gperf declarations heading
while (<DATA>) { print $_ }

my %tree;

my $row = 0;
while (<>) {
    #chomp;
    s/\s+$//g;

    if (/^#define\s*(\S*)\s*\"(\S*)\"\s*\/\/gperf\s*([^, ]+), ?(.*)/) {
		$row++;
		my $hard_tag_define = $1;
		my $hard_tag_value  = $2;
		my $super_hard_tag  = $3;
		my $pos  = $4;

		$tree{$hard_tag_define} = {
			hard_tag_define => $hard_tag_define,
			hard_tag_value => $hard_tag_value,
			super_hard_tag => $super_hard_tag,
		};

		if ($hard_tag_define eq 'HARD_TAG_ENTITY') {
			# <row_id>, <tag id>, <super_object>, <pos>, <rank_cstr>
			print "$hard_tag_value, $super_hard_tag, $pos, $row, 0\n";
		} else {
			if ( $tree{$super_hard_tag}->{childs} ) {
				push @{$tree{$super_hard_tag}->{childs}}, $hard_tag_define;
			} else {
				$tree{$super_hard_tag}->{childs} = [ $hard_tag_define ];
			}

			# last byte is its child index (starting from 1)
			my $last_byte = @{$tree{$super_hard_tag}->{childs}};
			my $super_rank = $tree{$super_hard_tag}->{rank};

			if (!$super_rank) {
				$tree{$hard_tag_define}->{rank} = [ $last_byte ];
			} else {
				my @s = @$super_rank;
				push @s, $last_byte;
				$tree{$hard_tag_define}->{rank} = \@s;
			}

			# <row_id>, <tag id>, <super_object>, <pos>, <rank_cstr>
			print "$hard_tag_value, $super_hard_tag, $pos, $row, ";

			# rank as a 64bit hex string
			my $b = 0;
			print "0x";
			foreach (@{$tree{$hard_tag_define}->{rank}}) {
				printf "%02x", $_;
				$b++;
			}
			while ($b++ < 8) { print "00"; }

			print "\n";
		}
    }
}

__END__
%{
#include "tagspace.h"
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
	tagspace::rowid_t row_id;
	uint64_t rank;
};
%%
