#!/bin/bash

train_file=./data/train
valid_file=./data/valid
test_file=./data/test

conf_dir=./conf/
exp_dir=./exp/

tr_thr=1
test_thr=1

stepnames[1]="Learn Vocab"
stepnames[2]="Train MaxEnt model"
stepnames[3]="Train RNN model"
stepnames[4]="Train RNN+MaxEnt model"

steps_len=${#stepnames[*]}

. ../steps/path.sh || exit 1

. ../utils/parse_options.sh || exit 1

if [ $# -gt 1 ] || ! shu-valid-range $1 || [ "$1" == "--help" ]; then 
  echo "usage: $0 [steps]"
  echo "e.g.: $0 -3,5,7-9,10-"
  echo "  stpes could be a number range within 1-$steps_len:"
  st=1
  while [ $st -le $steps_len ]
  do
  echo "   step$st:	${stepnames[$st]}"
  ((st++))
  done
  echo ""
  echo "options: "
  echo "     --conf-dir <conf-dir>         # config directory."
  echo "     --exp-dir <exp-dir>           # exp directory."
  exit 1
fi

steps=$1

st=1
if shu-in-range $st $steps; then
echo
echo "Step $st: ${stepnames[$st]} ..."
../steps/learn_vocab.sh $train_file $valid_file $exp_dir || exit 1;
fi
((st++))

if shu-in-range $st $steps; then
echo
echo "Step $st: ${stepnames[$st]} ..."
conf=$conf_dir/maxent
dir=$exp_dir/maxent
../steps/init_model.sh --config-file $conf/init.conf\
        $exp_dir/vocab.clm $dir || exit 1;

../steps/train_model.sh --train-config $conf/train.conf \
        --test-config $conf_dir/test.conf \
        --train-threads $tr_thr \
        --test-threads $test_thr \
        $train_file $valid_file $dir || exit 1;

../steps/test_model.sh --config-file $conf_dir/test.conf \
        --test-threads $test_thr \
        $dir $test_file || exit 1;
fi
((st++))

if shu-in-range $st $steps; then
echo
echo "Step $st: ${stepnames[$st]} ..."
conf=$conf_dir/rnn
dir=$exp_dir/rnn
../steps/init_model.sh --config-file $conf/init.conf\
        $exp_dir/vocab.clm $dir || exit 1;

../steps/train_model.sh --train-config $conf/train.conf \
        --test-config $conf_dir/test.conf \
        --train-threads $tr_thr \
        --test-threads $test_thr \
        $train_file $valid_file $dir || exit 1;

../steps/test_model.sh --config-file $conf_dir/test.conf \
        --test-threads $test_thr \
        $dir $test_file || exit 1;
fi
((st++))

if shu-in-range $st $steps; then
echo
echo "Step $st: ${stepnames[$st]} ..."
conf=$conf_dir/rnn+maxent
dir=$exp_dir/rnn+maxent
../steps/init_model.sh --config-file $conf/init.conf\
        $exp_dir/vocab.clm $dir || exit 1;

../steps/train_model.sh --train-config $conf/train.conf \
        --test-config $conf_dir/test.conf \
        --train-threads $tr_thr \
        --test-threads $test_thr \
        $train_file $valid_file $dir || exit 1;

../steps/test_model.sh --config-file $conf_dir/test.conf \
        --test-threads $test_thr \
        $dir $test_file || exit 1;
fi
((st++))

