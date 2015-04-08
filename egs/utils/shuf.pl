#!/usr/bin/perl

use strict;
use warnings;
use Getopt::Long qw(GetOptions);
use List::Util 'shuffle';
 
my $seed = 1;
my $help;
GetOptions(
    'seed=s' => \$seed,
    'help' => \$help,
) or die "Usage: $0 --seed s < in > out\n";
 
if ($help) {
    die "Usage: $0 --seed s < in > out\n";
}

srand($seed);
print shuffle(<STDIN>);

