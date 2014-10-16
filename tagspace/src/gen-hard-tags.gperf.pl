#!/usr/bin/perl
## generates a gperf input file of hard tags
## The DATA (__END__) section of this file contains gperf declarations heading

use strict;
use warnings;
use open qw(:std :utf8);

@ARGV > 0 or die "usage: $0 <hard-tags.h>";

my %tree;
my %defines;
my @rows = ("");  # first row == 0 unused
my $row = 0;

while (<>) {
    #chomp;
    s/\s+$//g;

# hard-tags.h form:
#define HARD_TAG_TAGNAME "<_tagname>" //gperf <SUPER_RELATION>, <tagd::part_of_speech>
# example:
#define HARD_TAG_RELATOR	"_relator"	//gperf HARD_TAG_ENTITY, tagd::POS_RELATOR

    if (/^#define\s*(\S*)\s*\"(\S*)\"\s*\/\/gperf\s*([^, ]+), ?(.*)/) {
		$row++;
		my $hard_tag_define = $1;
		my $hard_tag_value  = $2;
		my $super_hard_tag_define  = $3;
		my $pos  = $4;

		$defines{$hard_tag_define} = $hard_tag_value;

		$tree{$hard_tag_value} = {
			hard_tag_define => $hard_tag_define,
			hard_tag_value => $hard_tag_value,
			super_hard_tag_define => $super_hard_tag_define,
			pos => $pos,
			row => $row
		};

		my $super_hard_tag = $defines{$super_hard_tag_define};

		die "undef super hard tag!" if !$super_hard_tag;

		push @rows, $hard_tag_value;

		if ($hard_tag_define eq 'HARD_TAG_ENTITY') {
			# <row_id>, <tag id>, <super_object>, <pos>, <rank_cstr>
			$tree{$hard_tag_value}->{rank} = undef;
			next;
		} 
		
		if ( $tree{$super_hard_tag}->{childs} ) {
			push @{$tree{$super_hard_tag}->{childs}}, $hard_tag_value;
		} else {
			$tree{$super_hard_tag}->{childs} = [ $hard_tag_value ];
		}

		# last byte is its child index (starting from 1)
		my $last_byte = @{$tree{$super_hard_tag}->{childs}};
		my $super_rank = $tree{$super_hard_tag}->{rank};

		if (!$super_rank) {
			$tree{$hard_tag_value}->{rank} = [ $last_byte ];
		} else {
			my @s = @$super_rank;
			push @s, $last_byte;
			$tree{$hard_tag_value}->{rank} = \@s;
		}
    }
}

# gperf declarations heading
while (<DATA>) { print $_ }

print "const char * hard_tag_rows[".scalar(@rows)."] = { \"" . join("\", \"", @rows) . "\" };\n";
print "const size_t hard_tag_rows_end = " . scalar(@rows) . ";\n";
print "\n";  # close declarations
print "%%\n";  # close declarations

shift @rows;  # first row unused
foreach ( @rows ) {
	my $hard_tag_value = $_;
	my $val = $tree{$hard_tag_value};

	# <row_id>, <tag id>, <super_object>, <pos>, <rank_cstr>
	print "$hard_tag_value, $val->{super_hard_tag_define}, $val->{pos}, $val->{row}, ";

	# rank as a 64bit hex string
	my $b = 0;
	print "0x";
	foreach (@{$val->{rank}}) {
		printf "%02x", $_;
		$b++;
	}
	while ($b++ < 8) { print "00"; }

	print "\n";
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

