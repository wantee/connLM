#!/bin/sh

if [ $# -lt 1 ]
then
  echo "Usage: $0 <mdl,-o:a.clm>"
  exit 1
fi

EGS_DIR="$(cd `dirname $0`/..; pwd)"

i=$1
o=`echo $i | sed 's/.*://'`
connlm-draw --log-file=/dev/stderr $i - | dot -Tps:cairo | ps2pdf - $o.pdf

if [ `uname` == "Darwin" ]; then
  open $o.pdf
fi
