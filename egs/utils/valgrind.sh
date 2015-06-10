#!/bin/bash

VAL_OUT=${VAL_OUT:-"valgrind.out"}


VAL_FD=10
VAL_RUN=1

if [ -n "$VAL_EXES" ]; then
  rm -f $VAL_OUT
  eval "exec $VAL_FD>> $VAL_OUT"

  shopt -s expand_aliases
  for x in $VAL_EXES; do
    alias $x="echo \"Run \$VAL_RUN\" >> $VAL_OUT; VAL_RUN=\$((VAL_RUN+1));valgrind --log-fd=$VAL_FD $x"
  done
fi

function val_exit()
{
  if [ -z "$VAL_EXES" ]; then
      return
  fi

  eval "exec $VAL_FD<&-"

  echo "Checking valgrind log from $VAL_OUT"
  perl -e '$pr = -1;
           while(<>) { 
             if (m/^Run (\d+)$/) {
               $run = $1; 
             } elsif (m/ERROR SUMMARY: (\d+) errors /) {
               if ($1 != 0) {
                   if ($pr != $run) {
                     print "Run $run:\n";
                     $pr = $run;
                   }
                   print "  $_";
               }
             } elsif (m/definitely lost: (\d+) bytes /) {
               if ($1 != 0) {
                   if ($pr != $run) {
                     print "Run $run:\n";
                     $pr = $run;
                   }
                   print "  $_";
               }
             }
           }
           ' < $VAL_OUT
}

trap val_exit EXIT

