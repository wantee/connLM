#!/bin/bash

# Begin configuration section.
config_file=./conf/init.conf
# end configuration sections

echo "$0 $@"  # Print the command line for logging
[ -f path.sh ] && . ./path.sh

. ../utils/parse_options.sh || exit 1

if [ $# -lt 2 ]; then 
  echo "usage: $0 <model-in> <model-out> [test-file]"
  echo "e.g.: $0 exp/0.clm exp/1.clm data/test"
  echo "options: "
  echo "     --config-file <config-file>         # default: ./conf/init.conf, config file."
  exit 1;
fi

log_file=log/init.log
test_log_file=log/init.test.log

model_in=$1
model_out=$2
test_file=$3

mkdir -p `dirname $model_in`
mkdir -p `dirname $model_out`

echo "**Initializing model $model_in to $model_out ..."
connlm-init --log-file=$log_file \
           --config=$config_file \
           $model_in $model_out \
|| exit 1; 

exit 0;

