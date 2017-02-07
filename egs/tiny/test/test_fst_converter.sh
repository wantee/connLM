#!/bin/bash

# ensure ./run.sh 1,5 first

source ../steps/path.sh

. ../steps/path.sh || exit 1

if [ "`basename $PWD`" != "tiny" ]; then
  echo 'You must run "$0" from the tiny/ directory.'
  exit 1
fi

exp_dir=${1:-./exp/rnn/}
dir=${2:-./test/tmp/fst_conv}

rm -rf "$dir"
mkdir -p "$dir"
mkdir -p "$dir/num"
mkdir -p "$dir/sym"

function tofst()
{
  out_dir=$1
  print_syms=$2
  connlm-tofst --log-file="$out_dir/log/fst.converter.log" \
               --num-thread=1 --output-unk=true \
               --wildcard-boost=0.005 --wildcard-boost-power=0.75 \
               --boost=0.007 --boost-power=0.8 \
               --word-syms-file="$out_dir/words.txt" \
               --state-syms-file="$out_dir/g.ssyms" \
               --print-syms=$2 \
               "$exp_dir/final.clm" "$out_dir/g.txt"
  if [ $? -ne 0 ]; then
    shu-err "connlm-tofst generate fst to [$out_dir] failed."
    return 1
  fi
}

echo "Testing fst converter..."
echo "Generating num FST..."
tofst "$dir/num" "false"
if [ $? -ne 0 ]; then
  shu-err " [ERROR]"
  exit 1
fi

echo "Checking FST..."
grep -v '<any>' "$dir/num/g.ssyms" | awk '{print $2}' | \
     perl -e 'use List::Util qw/shuffle/; print shuffle <>;' | \
     head -n 100 | tr ':' ' ' | cut -d' ' -f2- > "$dir/num/sents.txt"
if [ $? -ne 0 ]; then
  shu-err " [ERROR]"
  exit 1
fi

connlm-eval --log-file="$dir/num/log/eval.log" \
            "$exp_dir/final.clm" "$dir/num/sents.txt" "$dir/num/sents.prob"
if [ $? -ne 0 ]; then
  shu-err "connlm-eval failed "
  shu-err " [ERROR]"
  exit 1
fi

../utils/check-fst.py --sent-prob="$dir/num/sents.prob" \
                      "$dir/num/words.txt" "$dir/num/g.ssyms" "$dir/num/g.txt"
if [ $? -ne 0 ]; then
  shu-err "check-fst.py failed"
  shu-err " [ERROR]"
  exit 1
fi

echo "Generating sym FST..."
tofst "$dir/sym" "true"
if [ $? -ne 0 ]; then
  shu-err " [ERROR]"
  exit 1
fi

echo "Differing num FST with sym FST..."
if ! diff "$dir/num/words.txt" "$dir/sym/words.txt" > /dev/null; then
  shu-err "words.txt differ"
  shu-err " [ERROR]"
  exit 1
fi

if ! diff "$dir/num/g.ssyms" "$dir/sym/g.ssyms" > /dev/null; then
  shu-err "g.ssyms differ"
  shu-err " [ERROR]"
  exit 1
fi

./test/sym2int.pl -f 3-4 "$dir/sym/words.txt" "$dir/sym/g.txt" \
    | tr ' ' '\t' > "$dir/sym/g.num.txt"
if [ $? -ne 0 ]; then
  shu-err "sym2int g.txt failed"
  shu-err " [ERROR]"
  exit 1
fi

if ! diff "$dir/num/g.txt" "$dir/sym/g.num.txt" > /dev/null; then
  shu-err "g.txt differ"
  shu-err " [ERROR]"
  exit 1
fi

echo "[SUCCESS]"
