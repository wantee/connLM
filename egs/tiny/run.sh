#!/bin/bash

train_file="./data/train"
valid_file="./data/valid"
test_file="./data/test"

conf_dir="./conf/"
exp_dir="./exp/"

tr_thr=1
eval_thr=1

realtype="float"

stepnames[1]="Learn Vocab"
stepnames+=("Train MaxEnt model:maxent")
stepnames+=("Train CBOW model:cbow")
stepnames+=("Train FFNN model:ffnn")
stepnames+=("Train RNN model:rnn")
stepnames+=("Train RNN+MaxEnt model:rnn+maxent")
stepnames+=("Train RNN~MaxEnt merge model:rnn~maxent")

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
../steps/learn_vocab.sh --config-file $conf_dir/vocab.conf \
    $train_file $exp_dir || exit 1;
fi
((st++))

while [ $st -le $steps_len ]
do
  if shu-in-range $st $steps; then
  echo
  echo "Step $st: ${stepnames[$st]%%:*} ..."
  ../steps/run_standalone.sh --train-thr $tr_thr --eval-thr $eval_thr \
      ${stepnames[$st]#*:} $conf_dir $exp_dir \
      $train_file $valid_file $test_file || exit 1;
  fi
  ((st++))
done
