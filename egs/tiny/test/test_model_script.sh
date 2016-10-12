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
  echo "Usage: $0 <step> [model]"
  exit 1
fi

step=$1
model=$2

case_dir="test/tmp/case-$SHU_CASE"

./run.sh --conf-dir "$case_dir/conf" --exp-dir "$case_dir/exp" 1,$step \
      > "test/output/case-$SHU_CASE.out"

if [ -n "$model" ]; then
  prefix="$case_dir/prefix.txt"
  echo -e 'I AM\nIT IS THAT\nYEAH' > "$prefix"

  source ../steps/path.sh

  model_file="$case_dir/exp/$model/final.clm"
  gen="test/output/case-$SHU_CASE.gen.txt"
  opt="--log-file=$case_dir/exp/$model/log/gen.log --random-seed=1"

  echo "# Generate 1" > $gen
  connlm-gen $opt $model_file >> $gen
  echo "# Generate 10" >> $gen
  connlm-gen $opt $model_file 10 >> $gen
  echo "# Generate with prefix" >> $gen
  connlm-gen $opt --prefix-file=$prefix $model_file >> $gen
fi
