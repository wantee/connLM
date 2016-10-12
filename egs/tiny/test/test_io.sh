#!/bin/bash

source ../steps/path.sh

if [ "`basename $PWD`" != "tiny" ]; then
  echo 'You must run "$0" from the tiny/ directory.'
  exit 1
fi

if [ $# -lt 2 ]; then
  echo "Usage: $0 <model> <exp_dir> [train_file]"
  exit 1
fi

model=$1
exp_dir=$2
train_file=${3:-./data/train}

shu-run connlm-info $exp_dir/init.clm || exit 1

shu-run connlm-copy --binary=true $exp_dir/init.clm $exp_dir/bin.clm || exit 1

shu-run connlm-copy --binary=false $exp_dir/bin.clm $exp_dir/txt.clm || exit 1

shu-run connlm-copy --binary=false $exp_dir/txt.clm $exp_dir/txt1.clm || exit 1

shu-run connlm-copy --binary=true $exp_dir/txt.clm $exp_dir/bin1.clm || exit 1

diff $exp_dir/txt.clm $exp_dir/txt1.clm || exit

echo "All Passed."
exit 0
