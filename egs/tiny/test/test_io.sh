#!/bin/bash

source ../steps/path.sh

if [ "`basename $PWD`" != "tiny" ]; then
  echo 'You must run "$0" from the tiny/ directory.'
  exit 1
fi

train_file=${1:-./data/train}
dir=${2:-./test/tmp/}
mkdir -p $dir

shu-run connlm-vocab $train_file $dir/vocab.clm || exit 1

shu-run connlm-output $dir/vocab.clm $dir/output.clm || exit 1

shu-run connlm-init $dir/output.clm conf/rnn+maxent/topo $dir/init.clm || exit 1

shu-run connlm-info $dir/init.clm || exit 1

shu-run connlm-copy --binary=true $dir/init.clm $dir/bin.clm || exit 1

shu-run connlm-copy --binary=false $dir/bin.clm $dir/txt.clm || exit 1

shu-run connlm-copy --binary=false $dir/txt.clm $dir/txt1.clm || exit 1

shu-run connlm-copy --binary=true $dir/txt.clm $dir/bin1.clm || exit 1

diff $dir/txt.clm $dir/txt1.clm || exit

echo "All Passed."
exit 0
