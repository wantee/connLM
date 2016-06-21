#!/usr/bin/perl

use strict;
use warnings;
use Getopt::Long qw(GetOptions);

my $seed = 1;
my $help;
GetOptions(
  'help' => \$help,
) or die "Usage: $0 < log > lr\n";

if ($help) {
  die "Usage: $0 < log > lr\n";
}

my $sec;
my %lrs;
while(my $line = <STDIN>) {
  chomp $line;

  if ($line =~ m/\[(.*)\]/) {
      $sec = $1;
  }

  if ($line =~ m/LEARN_RATE : ([0-9\.]+)/) {
      $lrs{$sec} = $1;
  }
}

for my $sec (sort keys(%lrs)) {
  print "$sec:$lrs{$sec}\n";
}
