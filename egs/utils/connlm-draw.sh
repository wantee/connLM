#!/bin/sh

function usage()
{
  echo >&2 "Usage: $0 [-v] <mdl,-o:a.clm> [dot-file]"
}

opts=
while getopts v o
do
  case "$o" in
  v)   opts+="--verbose=true ";;
  [?]) usage
    exit 1;;
  esac
done
shift $(($OPTIND-1))

if [ $# -lt 1 ]
then
  usage
  exit 1
fi

set -e

EGS_DIR="$(cd `dirname $0`/..; pwd)"

i=$1
o=`echo $i | sed 's/.*://'`
d=$2
if [ -n "$d" ]; then
  connlm-draw $opts --log-file=/dev/stderr "$i" "$d"
  cat "$d" | dot -Tps:cairo | ps2pdf - "$o.pdf"
else
  connlm-draw $opts --log-file=/dev/stderr "$i" - \
                       | dot -Tps:cairo | ps2pdf - "$o.pdf"
fi

if [ `uname` == "Darwin" ]; then
  open $o.pdf
fi
