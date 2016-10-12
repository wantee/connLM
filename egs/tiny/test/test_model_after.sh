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
  echo "Usage: $0 <model>"
  exit 1
fi

case_dir="test/tmp/case-$SHU_CASE"
./test/test_io.sh $1 "$case_dir/exp/$1" data/train \
        >${case_dir}/exp/log/test_io.log 2>&1
