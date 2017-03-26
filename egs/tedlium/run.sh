#!/bin/bash

# data source from 'cantab-tedlium' or 'TedliumRelease2'
data_source=TedliumRelease2

corpus=./corpus
data=./data/
train_file=$data/train
valid_file=$data/valid
test_file=$data/test
wordlist=$data/wordlist

conf_dir=./conf/
exp_dir=./exp/

tr_thr=16
eval_thr=16
tofst_thr=16

realtype="float"

stepnames[1]="Prepare data"
stepnames+=("Learn Vocab")
stepnames+=("Train MaxEnt model:maxent")
stepnames+=("Train RNN model:rnn")
stepnames+=("Train RNN+MaxEnt model:rnn+maxent")
stepnames+=("Convert RNN to WFST:rnn#")

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
  if [ "$data_source" == "cantab-tedlium" ]; then
    num_valid=15000
    local/cantab-tedlium-download_data.sh $corpus || exit 1
    local/cantab-tedlium-prepare_data.sh $corpus $num_valid $data || exit 1
  else
    num_valid=10000
    local/tedlium-release2-download_data.sh $corpus || exit 1
    local/tedlium-release2-prepare_data.sh $corpus $num_valid $data || exit 1
  fi
fi
((st++))

if shu-in-range $st $steps; then
echo
echo "Step $st: ${stepnames[$st]} ..."
../steps/learn_vocab.sh --config-file $conf_dir/vocab.conf \
    --wordlist $wordlist $train_file $exp_dir || exit 1;
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
    elif [[ "$model" == *"#" ]]; then
      model=${model%?}
      ../steps/convert_to_fst.sh --num-thr $tofst_thr \
          --bloom-filter-text-file $train_file \
          ${model} $conf_dir $exp_dir || exit 1;
    else
      ../steps/run_standalone.sh --train-thr $tr_thr --eval-thr $eval_thr \
          ${model} $conf_dir $exp_dir \
          $train_file $valid_file $test_file || exit 1;
    fi
  fi
  ((st++))
done
