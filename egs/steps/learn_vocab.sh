#!/bin/bash

# Begin configuration section.
config_file=""
wordlist=""
# end configuration sections

echo "$0 $@"  # Print the command line for logging
function print_help()
{
  echo "usage: $0 <train-file> <exp-dir>"
  echo "e.g.: $0 data/train exp"
  echo "options: "
  echo "     --config-file <file>         # vocab config file."
  echo "     --wordlist <file>            # wordlist file."
}

help_message=`print_help`

[ -f `dirname $0`/path.sh ] && . `dirname $0`/path.sh
[ -f ./path.sh ] && . ./path.sh

. ../utils/common_utils.sh || exit 1
. ../utils/parse_options.sh || exit 1

if [ $# -ne 2 ]; then
  print_help 1>&2
  exit 1;
fi

train_file=`rxfilename $1`
dir=$2

log_file=$dir/log/vocab.log
model_file=$dir/vocab.clm

begin_date=`date +"%Y-%m-%d %H:%M:%S"`
begin_ts=`date +%s`
mkdir -p $dir

echo "$0: Learning vocab $model_file from $1"
opts="--log-file=$log_file"
if [ -n "$config_file" ]; then
  opts+=" --config=$config_file"
fi
if [ -n "$wordlist" ]; then
  opts+=" --wordlist=$wordlist"
fi
shu-run connlm-vocab $opts "'$train_file'" $model_file || exit 1;

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
