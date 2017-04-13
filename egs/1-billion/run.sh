#!/bin/bash

data=$PWD/corpus

# base url for downloads.
data_url=http://statmt.org/wmt11/training-monolingual.tgz

train_file=./data/train.gz
valid_file=./data/valid
test_file=./data/test
vocab_file=./data/vocab

conf_dir=./conf/
exp_dir=./exp/

tr_thr=16
eval_thr=16

realtype="float"

stepnames[1]="Prepare data"
stepnames+=("Learn Vocab")
stepnames+=("Train MaxEnt model:maxent")
stepnames+=("Train RNN model:rnn")
stepnames+=("Train RNN+MaxEnt model:rnn+maxent")

steps_len=${#stepnames[*]}

function print_help()
{
  echo "usage: $0 [steps]"
  echo "e.g.: $0 -3,5,7-9,10-"
  echo "  stpes could be a number range within 1-$steps_len:"
  st=1
  while [ $st -le $steps_len ]
  do
  echo "   step$st:	${stepnames[$st]%%:*}"
  ((st++))
  done
  echo ""
  echo "options: "
  echo "     --conf-dir <conf-dir>         # config directory."
  echo "     --exp-dir <exp-dir>           # exp directory."
}

help_message=`print_help`

if [ ! -z "$realtype" ]; then
export CONNLM_REALTYPE="$realtype"
fi

. ../steps/path.sh || exit 1

. ../utils/parse_options.sh || exit 1

if [ $# -gt 1 ] || ! shu-valid-range $1; then
  print_help 1>&2
  exit 1
fi

steps=$1

mkdir -p "$exp_dir"
cp -r "${conf_dir%/}" "$exp_dir"
conf_dir="$exp_dir/`basename $conf_dir`"

st=1
if shu-in-range $st $steps; then
echo
echo "Step $st: ${stepnames[$st]} ..."
local/download_data.sh $data_url $data || exit 1
local/prep_data.sh $data || exit 1

if [ ! -f "$train_file" ]; then
  mkdir -p `dirname "$train_file"`
  cat $data/1-billion-word-language-modeling-benchmark/training-monolingual.tokenized.shuffled/news.en-*-of-00100 | gzip -c > $train_file || exit 1
fi
if [ ! -f "$valid_file" ]; then
  mkdir -p `dirname "$valid_file"`
  cp $data/1-billion-word-language-modeling-benchmark/heldout-monolingual.tokenized.shuffled/news.en.heldout-00000-of-00050 $valid_file || exit 1
fi
if [ ! -f "$test_file" ]; then
  mkdir -p `dirname "$test_file"`
  cp $data/1-billion-word-language-modeling-benchmark/heldout-monolingual.tokenized.shuffled/news.en.heldout-00001-of-00050 $test_file || exit 1
fi

fi
((st++))

if shu-in-range $st $steps; then
echo
echo "Step $st: ${stepnames[$st]} ..."
../steps/learn_vocab.sh --config-file $conf_dir/vocab.conf \
    $train_file $exp_dir || exit 1;
fi
((st++))

while [ $st -le $steps_len ]
do
  if shu-in-range $st $steps; then
  echo
  echo "Step $st: ${stepnames[$st]%%:*} ..."
  model=${stepnames[$st]#*:}
  if [[ "$model" == *"~"* ]]; then
    ../steps/run_cascade.sh --train-thr $tr_thr --eval-thr $eval_thr \
        ${model} $conf_dir $exp_dir \
        $train_file $valid_file $test_file || exit 1;
  else
    ../steps/run_standalone.sh --train-thr $tr_thr --eval-thr $eval_thr \
        ${model} $conf_dir $exp_dir \
        $train_file $valid_file $test_file || exit 1;
  fi
  fi
  ((st++))
done
