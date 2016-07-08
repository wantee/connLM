#!/usr/bin/perl

use strict;
use warnings;
use Getopt::Long qw(GetOptions);

my $help;
GetOptions(
  'help' => \$help,
) or die "Usage: connlm-info 1.clm | $0 > wt.names\n";

if ($help) {
  die "Usage: connlm-info 1.clm | $0 > wt.names\n";
}

my $comp = undef;
my $glue = undef;
my %wts;
while(my $line = <STDIN>) {
  chomp $line;

  if ($line =~ m/<COMPONENT>:(.+)/i) {
    $comp = lc($1);
    $glue = undef;
    next;
  }

  if (!$comp) {
    next;
  }

  if ($line =~ m/<GLUE>:(.+)/i) {
    $glue = lc($1);
    next;
  }

  if (!$glue) {
    next;
  }

  if ($line =~ m/Type:(.+wt)/) {
    $wts{$comp."/".$glue} = 1;
  }
}

for my $name (keys(%wts)) {
  print "$name\n";
}
