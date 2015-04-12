#!/bin/bash

# Begin configuration section.
config_file=""
# end configuration sections

echo "$0 $@"  # Print the command line for logging
[ -f path.sh ] && . ./path.sh

. ../utils/parse_options.sh || exit 1

if [ $# -ne 2 ]; then 
  echo "usage: $0 <exp-dir> <test-file>"
  echo "e.g.: $0 exp/rnn data/test"
  echo "options: "
  echo "     --config-file <config-file>         # config file."
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
echo "================================="

exit 0;

