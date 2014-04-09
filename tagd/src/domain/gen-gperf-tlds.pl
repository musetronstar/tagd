#!/usr/bin/perl
## generates a gperf input file of tlds defined in effective_tld_names.dat
## DATA section of this file contains gperf declarations

use strict;
use warnings;
use open qw(:std :utf8);

use Net::IDN::Encode 'domain_to_ascii';
my $print_private = 1;

@ARGV > 0 or die "usage: $0 <effective_tld_names.dat>";

# gperf declarations header
while (<DATA>) {
    if (/^USE_PRIVATE_PLACEHOLDER/) {
        print "%{\n";
        print "const bool HASH_CONTAINS_PRIVATE_TLDS = "
            .($print_private ? "true" : "false").";\n";
        print "%}\n";
    } else {
        print $_ 
    }
}

my %tlds;
my $managed_by = undef;
my $idn;
my $tld;
while (<>) {
    # strip comments, leading ws, blank lines...
    #chomp;
    s/\s+$//g;
    if (/^\s*$/) {
        # print "\n";
        next;
    }

    $managed_by = 'ICANN' if /BEGIN ICANN/;
    $managed_by = 'PRIVATE' if /BEGIN PRIVATE/;

    next if !$managed_by;
    next if $managed_by eq 'PRIVATE' and !$print_private;

    ($idn) = /(xn--\S+)/ if !$idn;

    if (/^\/\/(.*)/) {
        print "# $1\n";
        next;
    }

    my $tld_constant;
    if (/^\*\.(.*)/) {
        $tld = $1;
        $tld_constant = "TLD_WILDCARD";
    } elsif (/^!(.*)/) {
        $tld = $1;
        $tld_constant = "TLD_EXCEPTION_REG";
    } else {
        $tld = $_;
        $tld_constant = "TLD_".$managed_by;
    }

    if ($idn) {
        print "# $tld\n";
        print_uniq("$idn, $tld_constant\n");
        $idn = undef;
    } else {
        my $a = domain_to_ascii($tld);
       
        print "# $tld\n" if $a ne $tld;  # utf
        print_uniq ("$a, $tld_constant\n");
    }
}

my %uniq;
sub print_uniq {
	my $v = shift;
	if (!$uniq{$v}) {
		$uniq{$v} = 1;
		print $v;
	}
}

__END__
// declarations
%struct-type
%7bit
%language=C++
%define class-name tld_hash
%define slot-name key
%define lookup-function-name lookup

USE_PRIVATE_PLACEHOLDER

// tld_code defined by including file 
struct hash_value {
    const char *key;
    tld_code code;
};
%%
