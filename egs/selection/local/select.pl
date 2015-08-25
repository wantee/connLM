#!/usr/bin/perl -w

use strict;

@ARGV != 5 && die "Usage: perl select.pl <corpus> <indomain_score> <general_score> <threshold> <out_selected>";

my ($fcorpus, $fin, $fgen, $thresh, $fout) = @ARGV;

open FCORPUS, $fcorpus or die "Failed to open file $fcorpus. $!";
open FIN, $fin or die "Failed to open file $fin. $!";
open FGEN, $fgen or die "Failed to open file $fgen. $!";
open FOUT, ">$fout" or die "Failed to open file $fout. $!";

my $total = 0;
my $selected = 0;
while (my $txt = <FCORPUS>) {
    if ($txt =~ /^$/) {
        next;
    }

    $total++;
    my $in_socre = <FIN>;
    if (! $in_socre) {
       die "$fin not enough lines. $.";
    }
    my $gen_socre = <FGEN>;
    if (! $gen_socre) {
       die "$fgen not enough lines. $.";
    }

    my @words = split /\s+/, $txt;
#    my $num = ($in_socre - $gen_socre) / @words;
#    print $num, "\n";
    if (($in_socre - $gen_socre) / (@words + 1) >= $thresh) {
        print FOUT $txt;
        $selected++;
    }
}

close FCORPUS;
close FIN;
close FGEN;
close FOUT;

print "Total sentences: $total\n";
print "Selected sentences: $selected\n";

