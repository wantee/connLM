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
cat "data/train" | awk 'NF' | sort > $tr_file

echo -n "Testing input..."
log_file="$dir/eval.log"
connlm-eval --log-file=$log_file --num-thread=1 --reader^epoch-size=1k \
            $exp_dir/01.clm data/train || exit 1
cat $log_file | grep -o -E "Step: word\[.*\]" \
    | awk -F "[][]" '{if($2 == "</s>") {\
                        printf("\n");
                      } else {
                        printf("%s ", $2);
                      }}' \
    | sed 's/ $//' | sort > $out_file

if ! diff $tr_file $out_file > /dev/null; then
  echo "[ERROR]"
  exit 1
fi
echo "[SUCCESS]"
