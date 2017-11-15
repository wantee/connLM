#!/bin/bash

# ensure ./run.sh 1-2 first

source ../steps/path.sh

if [ "`basename $PWD`" != "tiny" ]; then
  echo 'You must run "$0" from the tiny/ directory.'
  exit 1
fi

exp_dir=${1:-./exp/maxent/}
dir=${2:-./test/tmp/}
mkdir -p $dir
tr_file="$dir/train.sort"
out_file="$dir/out.sort"
cat "data/train" | awk 'NF' | LC_ALL=C sort > $tr_file

echo -n "Testing input..."
log_file="$dir/eval.log"
rm -f $log_file 2>/dev/null
connlm-eval --log-file=$log_file --num-thread=1 --reader.epoch-size=1k \
            $exp_dir/01.clm data/train || exit 1
connlm-extract-syms --log-file="$dir/extract-syms.log" $exp_dir/01.clm \
                    $dir/syms.txt || exit 1
cat $log_file | grep -o -E "Step: words\[.*\]" \
    | awk -F "[][]" '{if($3 == 0) {\
                        printf("\n");
                      } else {
                        printf("%d ", $3);
                      }}' \
    | python -c '
from __future__ import print_function
import sys
vocab = {}
with open(sys.argv[1], "r") as f:
    for line in f:
        fields = line.split()
        vocab[fields[1]] = fields[0]
for line in sys.stdin:
    for wid in line.split():
        print(vocab[wid], end=" ")
    print("")
    ' $dir/syms.txt | sed 's/ $//' | LC_ALL=C sort > $out_file

if ! diff $tr_file $out_file > /dev/null; then
  echo "[ERROR]"
  exit 1
fi
echo "[SUCCESS]"
