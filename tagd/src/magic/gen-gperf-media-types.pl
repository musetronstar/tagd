#!/usr/bin/env perl
use strict;
use warnings;

# CSV is parsed before validation so quoted commas/newlines cannot change columns.
sub csv_rows {
	my ($path) = @_;
	open my $fh, '<:raw', $path or die "$path: $!\n";
	local $/;
	my $text = <$fh>;
	my (@rows, @row);
	my $field = '';
	my $state = 'start';
	for (my $i = 0; $i < length($text); ++$i) {
		my $c = substr($text, $i, 1);
		if ($state eq 'quoted') {
			if ($c eq '"') {
				if (substr($text, $i + 1, 1) eq '"') {
					$field .= '"';
					++$i;
				} else { $state = 'closed'; }
			} else { $field .= $c; }
			next;
		}
		if ($c eq '"' && $state eq 'start') { $state = 'quoted'; next; }
		if ($c eq ',' || $c eq "\n" || $c eq "\r") {
			push @row, $field;
			$field = '';
			$state = 'start';
			if ($c ne ',') {
				++$i if $c eq "\r" && substr($text, $i + 1, 1) eq "\n";
				push @rows, [@row];
				@row = ();
			}
			next;
		}
		die "$path: malformed CSV\n" if $state eq 'closed' || $c eq '"';
		$field .= $c;
		$state = 'plain';
	}
	die "$path: unterminated CSV quote\n" if $state eq 'quoted';
	push @rows, [@row, $field] if @row || $state ne 'start';
	return \@rows;
}

@ARGV == 2 or die "usage: $0 media-types.csv canonical-extensions.csv\n";
my $rows = csv_rows($ARGV[0]);
@$rows && join(',', @{shift @$rows}) eq 'Media Type,Type,Subtype,Template,Extensions'
	or die "unexpected upstream columns\n";
my %mapping;
for my $row (@$rows) {
	@$row == 5 or die "invalid upstream field count\n";
	my $mime = lc $row->[0];
	$mime =~ m{^[a-z0-9!#\$%&'*+.^_`|~-]+/[a-z0-9!#\$%&'*+.^_`|~-]+$}
		or die "invalid MIME: $mime\n";
	exists $mapping{$mime} and die "duplicate MIME: $mime\n";
	$mapping{$mime} = $row->[4];
}
my $corrections = csv_rows($ARGV[1]);
@$corrections && join(',', @{shift @$corrections}) eq 'mime,presence,expected,extension,rationale'
	or die "unexpected correction columns\n";
my %seen;
for my $row (@$corrections) {
	@$row == 5 or die "invalid correction field count\n";
	my ($mime, $presence, $expected, $extension, $why) = @$row;
	$mime = lc $mime;
	$seen{$mime}++ and die "duplicate correction: $mime\n";
	length($why) or die "missing correction rationale\n";
	if ($presence eq 'present') {
		exists $mapping{$mime} && $mapping{$mime} eq $expected
			or die "upstream drift: $mime\n";
	} elsif ($presence eq 'absent') {
		!exists $mapping{$mime} && $expected eq '' or die "upstream drift: $mime\n";
	} else { die "invalid presence: $mime\n"; }
	$mapping{$mime} = $extension;
}
print "%language=C++\n%readonly-tables\n%struct-type\n%define class-name media_type_lookup\n";
print "%define lookup-function-name find\n%define slot-name name\n";
print "%{\n#include <cstring>\n%}\nstruct media_type_entry { const char* name; const char* extension; };\n%%\n";
for my $mime (sort keys %mapping) {
	my $ext = $mapping{$mime};
	next if $ext eq '';
	$mime =~ m{^[a-z0-9!#\$%&'*+.^_`|~-]+/[a-z0-9!#\$%&'*+.^_`|~-]+$}
		or die "invalid correction MIME: $mime\n";
	$ext =~ /^[a-z0-9][a-z0-9_-]*$/ or die "unsafe extension for $mime: $ext\n";
	print "$mime, \"$ext\"\n";
}
print "%%\n";
