#!/usr/bin/perl

use strict;
use warnings;
use Getopt::Long qw(GetOptions);

my $help;
GetOptions(
  'help' => \$help,
) or die "Usage: $0 wt.names < log > lr\n";

if ($help) {
  die "Usage: $0 wt.names < log > lr\n";
}

my ($fwt_names) = @ARGV;

open(my $FWT_NAMES, '<', $fwt_names)
  or die "Could not open file '$fwt_names' $!";

my %wt_names;
while(my $line = <$FWT_NAMES>) {
  chomp $line;

  $wt_names{$line} = 1;
}

my $sec = undef;
my %lrs;
while(my $line = <STDIN>) {
  chomp $line;

  if ($line =~ m/\[(.*)\]/) {
    $sec = lc($1);
  }

  if ($line =~ m/LEARN_RATE\s*:([^}]+)/i) {
    $lrs{$sec} = $1 + 0.0;
  }
}

for my $sec (sort keys(%lrs)) {
  if ($wt_names{$sec}) {
    my $lr = $lrs{$sec};
    $sec =~ s!/!.!g;
    print "--$sec.learn-rate=$lr\n";
  }
}
