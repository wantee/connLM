#!/bin/bash

# Begin configuration section.
config_file=""
# end configuration sections

echo "$0 $@"  # Print the command line for logging
[ -f `dirname $0`/path.sh ] && . `dirname $0`/path.sh
[ -f ./path.sh ] && . ./path.sh

. ../utils/parse_options.sh || exit 1

if [ $# -ne 3 ]; then 
  echo "usage: $0 <train-file> <valid-file> <exp-dir>"
  echo "e.g.: $0 data/train data/valid exp"
  echo "options: "
  echo "     --config-file <file>         # vocab config file."
  exit 1;
fi

train_file=$1
valid_file=$2
dir=$3

log_file=$dir/log/vocab.log
model_file=$dir/vocab.clm

mkdir -p $dir

echo "**Learning vocab $model_file from $train_file"
if [ -z $config_file ]; then
cat $valid_file $train_file | connlm-vocab --log-file=$log_file \
           - $model_file \
|| exit 1; 
else
cat $valid_file $train_file | connlm-vocab --log-file=$log_file \
           --config=$config_file \
           - $model_file \
|| exit 1; 
fi

words=`../utils/get_value.sh "Words" $log_file`
size=`../utils/get_value.sh "Vocab Size" $log_file`
echo "Succeeded learning vocab: "
echo "  Words: $words"
echo "  Vocab size: $size"

exit 0;

