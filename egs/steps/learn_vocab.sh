#!/bin/bash

# Begin configuration section.
max_word_num=0
max_vocab_size=1000000
class_size=100
hs=true
# end configuration sections

echo "$0 $@"  # Print the command line for logging
[ -f path.sh ] && . ./path.sh

. ../utils/parse_options.sh || exit 1

if [ $# -ne 2 ]; then 
  echo "usage: $0 <train-file> <model-out>"
  echo "e.g.: $0 data/train exp/0.clm"
  echo "options: "
  echo "     --max-word-num <number>         # default: 0, #words used to learn vocab."
  echo "     --max-vocab-size <number>       # default: 1000000, vocab capacity."
  echo "     --class-size <number>           # default: 100; output layer class size"
  echo "     --hs (true|false)               # default: true; whether to use hierarchical softmax."
  exit 1;
fi

log_file=log/vocab.log

train_file=$1
model_file=$2

mkdir -p `dirname $model_file`

echo "**Learning vocab $model_file from $train_file"
connlm-vocab --log-file=$log_file \
           --max-word-num=$max_word_num \
           --max-vocab-size=$max_vocab_size \
           --class-size=$class_size \
           --hs=$hs \
           $train_file $model_file \
|| exit 1; 

exit 0;

