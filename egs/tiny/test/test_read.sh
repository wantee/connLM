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

for thr in 1 2 3 4; do
  for shuffle in true false; do
    for sz in 1 2 5 10; do
      echo -n "Testing train: thread=$thr, shuffle=$shuffle, sz=$sz..."
      connlm-train --log-file=/dev/null --debug-file=- \
                   --num-thread=$thr --shuffle=$shuffle --epoch-size=$sz \
                   data/train $exp_dir/init.clm $exp_dir/01.clm \
        | grep "<EGS>:" | cut -d' ' -f2- | sort > $out_file
  
      if ! diff $tr_file $out_file > /dev/null; then
        echo "[ERROR]"
        exit 1
      fi
      echo "[SUCCESS]"
    done
  done
done

for thr in 1 2 3 4; do
  for sz in 1 2 5 10; do
    echo -n "Testing test: thread=$thr, sz=$sz..."
    connlm-test --log-file=/dev/null --debug-file=- \
                --num-thread=$thr --epoch-size=$sz \
                $exp_dir/01.clm data/train \
      | grep "<EGS>:" | cut -d' ' -f2- | sort > $out_file
  
    if ! diff $tr_file $out_file > /dev/null; then
      echo "[ERROR]"
      exit 1
    fi
    echo "[SUCCESS]"
  done
done

