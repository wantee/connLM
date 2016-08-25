#!/bin/bash

set -e

if [ "`basename $PWD`" != "tiny" ]; then
  echo 'You must run "$0" from the tiny/ directory.'
  exit 1
fi

if [ -z "$SHU_CASE"  ]; then
  echo '$SHU_CASE not defined'
  exit 1
fi

if [ $# -lt 1 ]; then
  echo "Usage: $0 <step>"
  exit 1
fi

step=$1
./run.sh --conf-dir "test/tmp/case-$SHU_CASE/conf" \
         --exp-dir "test/tmp/case-$SHU_CASE/exp" 1,$step \
      > "test/output/case-$SHU_CASE.out"
