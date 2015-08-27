#!/bin/bash

# Begin configuration section.
config_file=""
# end configuration sections

echo "$0 $@"  # Print the command line for logging
function print_help()
{
  echo "usage: $0 <train-file> <exp-dir>"
  echo "e.g.: $0 data/train exp"
  echo "options: "
  echo "     --config-file <file>         # vocab config file."
}

help_message=`print_help`

[ -f `dirname $0`/path.sh ] && . `dirname $0`/path.sh
[ -f ./path.sh ] && . ./path.sh

. ../utils/parse_options.sh || exit 1

if [ $# -ne 2 ]; then 
  print_help 1>&2 
  exit 1;
fi

train_file=$1
dir=$2

log_file=$dir/log/vocab.log
model_file=$dir/vocab.clm

begin_date=`date +"%Y-%m-%d %H:%M:%S"`
begin_ts=`date +%s`
mkdir -p $dir

echo "$0: Learning vocab $model_file from $train_file"
if [ -z $config_file ]; then
cat $train_file | connlm-vocab --log-file=$log_file \
           - $model_file \
|| exit 1; 
else
cat $train_file | connlm-vocab --log-file=$log_file \
           --config=$config_file \
           - $model_file \
|| exit 1; 
fi

words=`../utils/get_value.sh "Words" $log_file`
size=`../utils/get_value.sh "Vocab Size" $log_file`
echo "================================="
echo "Words: $words"
echo "Vocab size: $size"
echo "================================="

../utils/check_log.sh -b "$begin_date" $log_file.wf

end_ts=`date +%s`
echo "$0: Elapse time: $(shu-diff-timestamp $begin_ts $end_ts)"

exit 0;

