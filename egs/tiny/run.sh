#!/bin/bash

train_file=./data/train
valid_file=./data/valid
test_file=./data/test

../steps/learn_vocab.sh $train_file $valid_file exp/vocab.clm || exit 1;

../steps/init_model.sh --config-file ./conf/init.conf\
        exp/vocab.clm exp/init.clm $test_file || exit 1;

../steps/train_model.sh --train-config ./conf/train.conf \
        --test-config ./conf/test.conf \
        $train_file $valid_file exp init.clm || exit 1;

