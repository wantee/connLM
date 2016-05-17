#!/bin/sh

if [ $# -lt 1 ]
then
  echo "Usage: $0 <mdl,-o:a.clm> [dot-file]"
  exit 1
fi

set -e

EGS_DIR="$(cd `dirname $0`/..; pwd)"

i=$1
o=`echo $i | sed 's/.*://'`
d=$2
if [ -n "$d" ]; then
  connlm-draw --log-file=/dev/stderr "$i" "$d"
  cat "$d" | dot -Tps:cairo | ps2pdf - "$o.pdf"
else
  connlm-draw --log-file=/dev/stderr "$i" - \
                       | dot -Tps:cairo | ps2pdf - "$o.pdf"
fi

if [ `uname` == "Darwin" ]; then
  open $o.pdf
fi
