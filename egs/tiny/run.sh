#!/bin/bash

train_file=./data/train
valid_file=./data/valid
test_file=./data/test

. ../utils/parse_range.sh || exit 1

if [ $# -eq 1 -a "$1" == "--help" ]; then 
  echo "usage: $0 [steps]"
  echo "e.g.: $0 -3,5,7-9,10-"
  exit 1;
fi

steps=$1

if in_range 1 $steps; then
echo "Learning vocab..."
../steps/learn_vocab.sh $train_file $valid_file exp || exit 1;
fi

if in_range 2 $steps; then
echo
echo "Training MaxEnt ..."
conf_dir=./conf/maxent
dir=exp/maxent
../steps/init_model.sh --config-file $conf_dir/init.conf\
        exp/vocab.clm $dir || exit 1;

../steps/train_model.sh --train-config $conf_dir/train.conf \
        --test-config ./conf/test.conf \
        $train_file $valid_file $dir || exit 1;

../steps/test_model.sh --config-file ./conf/test.conf \
        $dir $test_file || exit 1;
fi

if in_range 3 $steps; then
echo
echo "Training RNN ..."
conf_dir=./conf/rnn
dir=exp/rnn
../steps/init_model.sh --config-file $conf_dir/init.conf\
        exp/vocab.clm $dir || exit 1;

../steps/train_model.sh --train-config $conf_dir/train.conf \
        --test-config ./conf/test.conf \
        $train_file $valid_file $dir || exit 1;

../steps/test_model.sh --config-file ./conf/test.conf \
        $dir $test_file || exit 1;
fi

if in_range 4 $steps; then
echo
echo "Training RNN+MaxEnt ..."
conf_dir=./conf/rnn+maxent
dir=exp/rnn+maxent
../steps/init_model.sh --config-file $conf_dir/init.conf\
        exp/vocab.clm $dir || exit 1;

../steps/train_model.sh --train-config $conf_dir/train.conf \
        --test-config ./conf/test.conf \
        $train_file $valid_file $dir || exit 1;

../steps/test_model.sh --config-file ./conf/test.conf \
        $dir $test_file || exit 1;
fi
