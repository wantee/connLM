#!/bin/bash

# Begin configuration section.
config_file=""
eval_opts=""
out_prob=""
eval_threads=1
log_file=""
pipe_input=false
# end configuration sections

echo "$0 $@"  # Print the command line for logging
function print_help()
{
  echo "usage: $0 <exp-dir> <test-file>"
  echo "e.g.: $0 exp/rnn data/test"
  echo "options: "
  echo "     --config-file <config-file>         # config file."
  echo "     --eval-threads <number>             # default: 1, number of evaluating threads."
  echo "     --eval-opts <opts>                  # default: \"\", options to be passed to connlm-eval."
  echo "     --out-prob <prob-file>              # default: \"\", output probs to specific file."
  echo "     --log-file <log-file>               # default: \"\", specify log file."
  echo "     --pipe-input <true|false>           # default: false, whether input test file is through a pipe."
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
log_file=${log_file:-"$dir/log/eval.log"}

echo "$0: Evaluating model $dir/final.clm ..."
if [ ! -z $config_file ]; then
eval_opts="--config=$config_file $eval_opts"
fi

if $pipe_input; then
shu-run eval "$test_file" | connlm-eval $eval_opts \
       --log-file=$log_file --num-thread=$eval_threads \
       $dir/final.clm - $out_prob || exit 1;

else
shu-run connlm-eval $eval_opts --log-file=$log_file \
           --num-thread=$eval_threads \
           $dir/final.clm $test_file $out_prob || exit 1;
fi

echo "================================="
ent=`../utils/get_value.sh "Entropy" $log_file`
echo "Eval Entropy: $(printf "%.6f" $ent)"

ppl=`../utils/get_value.sh "PPL" $log_file`
echo "Eval PPL: $(printf "%.6f" $ppl)"

wpc=`../utils/get_value.sh "words/sec" $log_file`
echo "Eval Speed: $(bc <<< "scale=1; $wpc / 1000")k words/sec"
echo "================================="

../utils/check_log.sh -b "$begin_date" $log_file.wf

end_ts=`date +%s`
echo "$0: Elapse time: $(shu-diff-timestamp $begin_ts $end_ts)"

exit 0;
