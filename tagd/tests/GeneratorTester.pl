#!/usr/bin/env perl
use strict;
use warnings;
use Test::More;
use File::Temp qw(tempdir);
use IPC::Open3;
use Symbol qw(gensym);
my $dir = tempdir(CLEANUP => 1);
sub generate {
	my ($csv, $corrections) = @_;
	for my $pair (['input', $csv], ['fixes', $corrections]) {
		open my $fh, '>', "$dir/$pair->[0]" or die $!;
		print $fh $pair->[1];
		close $fh;
	}
	my $err = gensym;
	my $pid = open3(undef, my $out, $err, $^X, '../src/magic/gen-gperf-media-types.pl',
		"$dir/input", "$dir/fixes");
	local $/;
	my $text = <$out> // '';
	my $error = <$err> // '';
	waitpid($pid, 0);
	return ($?, $text, $error);
}
my $header = "Media Type,Type,Subtype,Template,Extensions\n";
my $fixes = "mime,presence,expected,extension,rationale\n";
my $data = $header . "IMAGE/JPEG,image,jpeg,\"a,b\",jpe\napplication/test,application,test,,\n";
my $override = $fixes . "image/jpeg,present,jpe,jpg,\"canonical, suffix\"\n";
my ($rc, $out) = generate($data, $override);
is($rc, 0, 'quoted CSV and normalized MIME');
like($out, qr/image\/jpeg, "jpg"/, 'override applied');
unlike($out, qr/application\/test,/, 'no empty extension entry');
my (undef, $again) = generate($data, $override);
is($again, $out, 'deterministic output');
($rc) = generate($data, $override =~ s/,jpe,/,jpeg,/r);
ok($rc != 0, 'upstream drift rejected');
($rc) = generate($data . "image/jpeg,image,jpeg,,jpg\n", $fixes);
ok($rc != 0, 'normalized duplicate rejected');
($rc) = generate($header . "text/plain,text,plain,,../evil\n", $fixes);
ok($rc != 0, 'unsafe suffix rejected');
($rc) = generate($header . "text/plain,text,plain,\"unfinished\n", $fixes);
ok($rc != 0, 'malformed CSV rejected');
($rc, $out) = generate($header, $fixes . "text/plain,absent,,txt,Supplement\n");
is($rc, 0, 'absent-row supplement');
like($out, qr/text\/plain, "txt"/, 'supplement emitted');
done_testing();
