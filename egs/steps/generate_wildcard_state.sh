#!/bin/bash

# Begin configuration section.
num_thr=1
eval_text_file=""
# end configuration sections

echo "$0 $@"  # Print the command line for logging
function print_help()
{
  echo "usage: $0 <conf-dir> <exp-dir> <out-file>"
  echo "e.g.: $0 conf/rnn exp/rnn exp/rnn/ws.txt"
  echo "     --eval-text-file <file>   # default: \"\"."
}

help_message=`print_help`

[ -f `dirname $0`/path.sh ] && . `dirname $0`/path.sh
[ -f ./path.sh ] && . ./path.sh

. ../utils/common_utils.sh || exit 1
. ../utils/parse_options.sh || exit 1

if [ $# -lt 3 ]; then
  print_help 1>&2
  exit 1;
fi

conf_dir=$1
exp_dir=$2
out_file=$3

eval_text_file=`rxfilename "$eval_text_file"`

begin_date=`date +"%Y-%m-%d %H:%M:%S"`
begin_ts=`date +%s`

mkdir -p "$exp_dir/log"
log_file=${log_file:-"$exp_dir/log/wildcard-state.log"}

opts=""
if [ -n "$eval_text_file" ]; then
  opts+="--eval-text-file='$eval_text_file'"
fi
shu-run connlm-wildcard-state --config="$conf_dir/wildcard-state.conf" \
                     --log-file="$log_file" \
                     $opts "$exp_dir/final.clm" "$out_file" || exit 1

../utils/check_log.sh -b "$begin_date" mem $log_file
../utils/check_log.sh -b "$begin_date" warn $log_file.wf

end_ts=`date +%s`
echo "$0: Elapse time: $(shu-diff-timestamp $begin_ts $end_ts)"

exit 0;
