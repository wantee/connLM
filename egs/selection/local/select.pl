#!/usr/bin/perl -w

use strict;

@ARGV != 1 && die "Usage: perl select.pl <threshold> < (corpus:indomain_score:general_score) > out_selected";

my ($thresh) = @ARGV;

my $total = 0;
my $selected = 0;
while (my $line = <STDIN>) {
    if ($line =~ /^$/) {
        next;
    }

    $total++;
    my @words = split /\s+/, $line;
    my $gen_score = pop @words;
    my $in_score = pop @words;

    if (($in_score - $gen_score) / (@words + 1) >= $thresh) { # +1 for </s>
        print join(" ", @words)."\n";
        $selected++;
    }
}

print STDERR "Total sentences: $total\n";
print STDERR "Selected sentences: $selected\n";
