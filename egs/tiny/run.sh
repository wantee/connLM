#!/bin/bash

train_file=./data/train
valid_file=./data/valid
test_file=./data/test

tr_thr=1
test_thr=1

stepnames[1]="Learn Vocab"
stepnames[2]="Train MaxEnt model"
stepnames[3]="Train RNN model"
stepnames[4]="Train RNN+MaxEnt model"

steps_len=${#stepnames[*]}

. ../utils/parse_range.sh || exit 1

if [ $# -gt 1 -o "$1" == "--help" ]; then 
  echo "usage: $0 [steps]"
  echo "e.g.: $0 -3,5,7-9,10-"
  echo "  stpes could be 1-$steps_len:"
  st=1
  while [ $st -le $steps_len ]
  do
  echo "   step$st:	${stepnames[$st]}"
  ((st++))
  done
  exit 1
fi

steps=$1

st=1
if in_range $st $steps; then
echo
echo "Step $st: ${stepnames[$st]} ..."
../steps/learn_vocab.sh $train_file $valid_file exp || exit 1;
fi
((st++))

if in_range $st $steps; then
echo
echo "Step $st: ${stepnames[$st]} ..."
conf_dir=./conf/maxent
dir=exp/maxent
../steps/init_model.sh --config-file $conf_dir/init.conf\
        exp/vocab.clm $dir || exit 1;

../steps/train_model.sh --train-config $conf_dir/train.conf \
        --test-config ./conf/test.conf \
        --train-threads $tr_thr \
        --test-threads $test_thr \
        $train_file $valid_file $dir || exit 1;

../steps/test_model.sh --config-file ./conf/test.conf \
        --test-threads $test_thr \
        $dir $test_file || exit 1;
fi
((st++))

if in_range $st $steps; then
echo
echo "Step $st: ${stepnames[$st]} ..."
conf_dir=./conf/rnn
dir=exp/rnn
../steps/init_model.sh --config-file $conf_dir/init.conf\
        exp/vocab.clm $dir || exit 1;

../steps/train_model.sh --train-config $conf_dir/train.conf \
        --test-config ./conf/test.conf \
        --train-threads $tr_thr \
        --test-threads $test_thr \
        $train_file $valid_file $dir || exit 1;

../steps/test_model.sh --config-file ./conf/test.conf \
        --test-threads $test_thr \
        $dir $test_file || exit 1;
fi
((st++))

if in_range $st $steps; then
echo
echo "Step $st: ${stepnames[$st]} ..."
conf_dir=./conf/rnn+maxent
dir=exp/rnn+maxent
../steps/init_model.sh --config-file $conf_dir/init.conf\
        exp/vocab.clm $dir || exit 1;

../steps/train_model.sh --train-config $conf_dir/train.conf \
        --test-config ./conf/test.conf \
        --train-threads $tr_thr \
        --test-threads $test_thr \
        $train_file $valid_file $dir || exit 1;

../steps/test_model.sh --config-file ./conf/test.conf \
        --test-threads $test_thr \
        $dir $test_file || exit 1;
fi
((st++))

