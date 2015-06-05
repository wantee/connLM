#!/bin/bash

# Begin configuration section.
config_file=""
test_threads=1
# end configuration sections

echo "$0 $@"  # Print the command line for logging
[ -f ../utils/path.sh ] && . ../utils/path.sh

. ../utils/parse_options.sh || exit 1

if [ $# -ne 2 ]; then 
  echo "usage: $0 <exp-dir> <test-file>"
  echo "e.g.: $0 exp/rnn data/test"
  echo "options: "
  echo "     --config-file <config-file>         # config file."
  echo "     --test-threads <number>             # default: 1, number of testing threads"
  exit 1;
fi


dir=$1
test_file=$2

log_file=$dir/log/test.log

echo "**Testing model $dir/final.clm ..."
if [ -z $config_file ]; then
connlm-test --log-file=$log_file \
           $dir/final.clm $test_file \
|| exit 1; 
else
connlm-test --log-file=$log_file \
           --config=$config_file \
           $dir/final.clm $test_file \
|| exit 1; 
fi

echo "================================="
ent=`../utils/get_value.sh "Entropy" $log_file`
echo "Test Entropy: $(printf "%.4f" $ent)"

ppl=`../utils/get_value.sh "PPL" $log_file`
echo "Test PPL: $(printf "%.4f" $ppl)"

wpc=`../utils/get_value.sh "words/sec" $log_file`
echo "Test Speed: $(bc <<< "scale=1; $wpc / 1000")k words/sec"
echo "================================="

exit 0;

