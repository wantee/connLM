#!/usr/bin/perl

use strict;
use warnings;
use Getopt::Long qw(GetOptions);

my $help;
GetOptions(
  'help' => \$help,
) or die "Usage: $0 0.5 < lr > lr.new\n";

if ($help) {
  die "Usage: $0 0.5 < lr > lr.new\n";
}

my ($scale) = @ARGV;

while(my $line = <STDIN>) {
  chomp $line;

  if ($line =~ m/--(.*?)=(.+)/) {
    print "--$1=".($2 * $scale)."\n";
  }
}
