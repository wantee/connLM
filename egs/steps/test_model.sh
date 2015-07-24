#!/bin/bash

# Begin configuration section.
config_file=""
test_threads=1
# end configuration sections

echo "$0 $@"  # Print the command line for logging
function print_help()
{
  echo "usage: $0 <exp-dir> <test-file>"
  echo "e.g.: $0 exp/rnn data/test"
  echo "options: "
  echo "     --config-file <config-file>         # config file."
  echo "     --test-threads <number>             # default: 1, number of testing threads"
}

help_message=`print_help`

[ -f `dirname $0`/path.sh ] && . `dirname $0`/path.sh
[ -f ./path.sh ] && . ./path.sh

. ../utils/parse_options.sh || exit 1

if [ $# -ne 2 ]; then 
  print_help 1>&2 
  exit 1;
fi


dir=$1
test_file=$2

begin_date=`date +"%Y-%m-%d %H:%M:%S"`
begin_ts=`date +%s`
log_file=$dir/log/test.log

echo "**Testing model $dir/final.clm ..."
if [ -z $config_file ]; then
connlm-test --log-file=$log_file \
           --num-thread=$test_threads \
           $dir/final.clm $test_file \
|| exit 1; 
else
connlm-test --log-file=$log_file \
           --config=$config_file \
           --num-thread=$test_threads \
           $dir/final.clm $test_file \
|| exit 1; 
fi

echo "================================="
ent=`../utils/get_value.sh "Entropy" $log_file`
echo "Test Entropy: $(printf "%.6f" $ent)"

ppl=`../utils/get_value.sh "PPL" $log_file`
echo "Test PPL: $(printf "%.6f" $ppl)"

wpc=`../utils/get_value.sh "words/sec" $log_file`
echo "Test Speed: $(bc <<< "scale=1; $wpc / 1000")k words/sec"
echo "================================="

../utils/check_log.sh -b "$begin_date" $log_file.wf

end_ts=`date +%s`
echo "Elapse time: $(shu-diff-timestamp $begin_ts $end_ts)"

exit 0;

