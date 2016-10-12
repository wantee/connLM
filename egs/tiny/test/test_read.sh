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

if [ `uname` == 'Darwin' ]; then
  num_cores=`sysctl hw.ncpu | awk '{print $NF}'`
else
  num_cores=`grep -c '^processor' /proc/cpuinfo`
fi

max_thr=$(printf %.f `bc <<< "$num_cores * 1.5"`)
inc=`expr $max_thr / 4`
if [ $inc -le 0 ]; then inc=1; fi
thrs=`seq 1 $inc $max_thr`
if [ `echo $thrs | awk '{print $NF}'` -ne $max_thr ]; then
  thrs+=" $max_thr";
fi

for thr in $thrs; do
  for shuffle in true false; do
    for sz in 1 2 5 10; do
      echo -n "Testing train: thread=$thr, shuffle=$shuffle, sz=$sz..."
      connlm-train --log-file=/dev/null --num-thread=$thr \
                   --reader^debug-file=- --reader^shuffle=$shuffle \
                   --reader^epoch-size=$sz \
                   $exp_dir/init.clm data/train $exp_dir/01.clm \
        | grep "<EGS>:" | cut -d' ' -f2- | sort > $out_file

      if ! diff $tr_file $out_file > /dev/null; then
        echo "[ERROR]"
        exit 1
      fi
      echo "[SUCCESS]"
    done
  done
done

for thr in $thrs; do
  for sz in 1 2 5 10; do
    echo -n "Testing eval: thread=$thr, sz=$sz..."
    connlm-eval --log-file=/dev/null --num-thread=$thr \
                --reader^debug-file=- --reader^epoch-size=$sz \
                $exp_dir/01.clm data/train \
      | grep "<EGS>:" | cut -d' ' -f2- | sort > $out_file

    if ! diff $tr_file $out_file > /dev/null; then
      echo "[ERROR]"
      exit 1
    fi
    echo "[SUCCESS]"
  done
done
