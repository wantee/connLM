#!/bin/bash

# Begin configuration section.
config_file=""
# end configuration sections

echo "$0 $@"  # Print the command line for logging
[ -f path.sh ] && . ./path.sh

. ../utils/parse_options.sh || exit 1

if [ $# -ne 2 ]; then 
  echo "usage: $0 <vocab-clm> <exp-dir>"
  echo "e.g.: $0 exp/vocab.clm exp/rnn"
  echo "options: "
  echo "     --config-file <config-file>         # config file."
  exit 1;
fi


vocab=$1
dir=$2

log_file=$dir/log/init.log
model_out=$dir/init.clm

mkdir -p $dir

echo "**Initializing model $vocab to $model_out ..."
if [ -z $config_file ]; then
connlm-init --log-file=$log_file \
           $vocab $model_out \
|| exit 1; 
else
connlm-init --log-file=$log_file \
           --config=$config_file \
           $vocab $model_out \
|| exit 1; 
fi

exit 0;

