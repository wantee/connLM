#!/bin/bash

# ensure ./run.sh 1,7,9 first

source ../steps/path.sh

. ../steps/path.sh || exit 1

if [ "`basename $PWD`" != "tiny" ]; then
  echo 'You must run "$0" from the tiny/ directory.'
  exit 1
fi

>&2 echo "$0 $@"

exp_dir=${1:-./exp/}
conf_dir=${2:-./conf/}
model=${3:-rnn+maxent}
wsm=${4:-baseline}
dir=${5:-./test/tmp/fst_conv}

rm -rf "$dir"
mkdir -p "$dir"
mkdir -p "$dir/num"
mkdir -p "$dir/sym"

if [ "$wsm" == "pick" ]; then
  ../steps/build_bloom_filter.sh "$conf_dir/$model" "$exp_dir" \
      "data/train" "$exp_dir/$model/train.blmflt" || exit 1
fi

function tofst()
{
  out_dir=$1
  print_syms=$2
  nt=${3:-1}

  opts=""
  if [ "$wsm" == "pick" ]; then
    opts+="--word-selection-method=pick "
    opts+="--bloom-filter-file=$exp_dir/$model/train.blmflt "
    opts+="--ws-arg=0.05 --max-gram=4 "
  else
    opts+="--word-selection-method=$wsm --ws-arg=0.025 --ws-arg-power=0.8 "
  fi

  connlm-tofst --log-file="$out_dir/log/fst.converter.log" \
               --num-thread=$nt --output-unk=true \
               --word-syms-file="$out_dir/words.txt" \
               --state-syms-file="$out_dir/g.ssyms" \
               --print-syms=$2 $opts\
               "$exp_dir/$model/final.clm" "$out_dir/g.txt"
  if [ $? -ne 0 ]; then
    shu-err "connlm-tofst generate fst to [$out_dir] failed."
    return 1
  fi
}

function mapfst()
{
  out_dir=$1

  ../utils/fst/renumber-fst.py "$out_dir/g.txt" "$out_dir/g.state.map"
  if [ $? -ne 0 ]; then
    shu-err "renumber-fst in [$out_dir] failed."
    return 1
  fi

  ./test/sym2int.pl -f 1-2 "$out_dir/g.state.map" "$out_dir/g.txt" \
      | sort -n -k 1,2 > "$out_dir/g.mapped.txt"
  if [ $? -ne 0 ]; then
    shu-err "sym2int to map g.txt failed"
    return 1
  fi

  ./test/sym2int.pl -f 1 "$out_dir/g.state.map" "$out_dir/g.ssyms" \
      | sort -n -k 1 > "$out_dir/g.mapped.ssyms"
  if [ $? -ne 0 ]; then
    shu-err "sym2int to map g.ssyms failed"
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
cat "$dir/num/g.ssyms" | awk 'NF > 1 { if ($2 != "<s>" && $2 != "</s>") {print $2} }' | \
     perl -e 'use List::Util qw/shuffle/; print shuffle <>;' | \
     head -n 100 | tr ':' ' ' | cut -d' ' -f2- > "$dir/num/sents.txt"
if [ $? -ne 0 ]; then
  shu-err " [ERROR]"
  exit 1
fi

connlm-eval --log-file="$dir/num/log/eval.log" \
            "$exp_dir/$model/final.clm" "$dir/num/sents.txt" "$dir/num/sents.prob"
if [ $? -ne 0 ]; then
  shu-err "connlm-eval failed "
  shu-err " [ERROR]"
  exit 1
fi

../utils/fst/check-fst.py --sent-prob="$dir/num/sents.prob" \
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

nt=4
echo "Genrate num FST with $nt threads"
mkdir -p "$dir/num-$nt"
tofst "$dir/num-$nt" "false" $nt
if [ $? -ne 0 ]; then
  shu-err " [ERROR]"
  exit 1
fi

echo "Differing num FST with 1 thread and $nt threads..."
mapfst "$dir/num" "false"
if [ $? -ne 0 ]; then
  shu-err "mapfst in [$dir/num] failed"
  shu-err " [ERROR]"
  exit 1
fi

mapfst "$dir/num-$nt" "false"
if [ $? -ne 0 ]; then
  shu-err "mapfst in [$dir/num-$nt] failed"
  shu-err " [ERROR]"
  exit 1
fi

if ! diff "$dir/num/words.txt" "$dir/num-$nt/words.txt" > /dev/null; then
  shu-err "words.txt differ"
  shu-err " [ERROR]"
  exit 1
fi

if ! diff "$dir/num/g.mapped.ssyms" "$dir/num-$nt/g.mapped.ssyms" > /dev/null; then
  shu-err "g.ssyms differ"
  shu-err " [ERROR]"
  exit 1
fi

if ! diff "$dir/num/g.mapped.txt" "$dir/num-$nt/g.mapped.txt" > /dev/null; then
  shu-err "g.txt differ"
  shu-err " [ERROR]"
  exit 1
fi

echo "[SUCCESS]"
