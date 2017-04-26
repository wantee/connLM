#!/bin/bash

# Begin configuration section.
num_thr=1
output_ssyms=false
bloom_filter_file=""
wildcard_state_file=""
# end configuration sections

echo "$0 $@"  # Print the command line for logging
function print_help()
{
  echo "usage: $0 <conf-dir> <exp-dir>"
  echo "e.g.: $0 conf/rnn exp/rnn"
  echo "options: "
  echo "     --num-thr <threads>    # default: 1."
  echo "     --output-ssyms <true|false>   # default: false."
  echo "     --bloom-filter-file <file>   # default: \"\"."
  echo "     --wildcard-state-file <file>   # default: \"\"."
}

help_message=`print_help`

[ -f `dirname $0`/path.sh ] && . `dirname $0`/path.sh
[ -f ./path.sh ] && . ./path.sh

. ../utils/parse_options.sh || exit 1

if [ $# -lt 2 ]; then
  print_help 1>&2
  exit 1;
fi

conf_dir=$1
exp_dir=$2

begin_date=`date +"%Y-%m-%d %H:%M:%S"`
begin_ts=`date +%s`

dir="$exp_dir/fst"
mkdir -p "$dir/log"

log_file=${log_file:-"$dir/log/tofst.log"}
opts="--num-thread=$num_thr"
if $output_ssyms; then
  opts+=" --state-syms-file=$dir/g.ssyms"
fi

if [ -n "$bloom_filter_file" ]; then
  opts+=" --bloom-filter-file=$bloom_filter_file"
fi

if [ -n "$wildcard_state_file" ]; then
  opts+=" --wildcard-state-file=$wildcard_state_file"
fi

shu-run connlm-tofst --config="$conf_dir/tofst.conf" --log-file="$log_file" \
                     $opts \
                     --word-syms-file="$dir/words.txt" \
                     "$dir/../final.clm" "'| gzip -c > $dir/g.txt.gz'" || exit 1

echo "================================="
num_states=`../utils/get_value.sh "Total states" $log_file`
echo "#states: $num_states"

num_arcs=`../utils/get_value.sh "Total arcs" $log_file`
echo "#arcs: $num_arcs"

max_gram=`../utils/get_value.sh "max_gram" $log_file`
echo "max-gram: $max_gram"


for i in `seq $max_gram`; do
  num_gram=`../utils/get_value.sh "${i}-grams" $log_file`
  num_gram=${num_gram:-0}
  echo "#${i}-gram: $num_gram"
done

echo "================================="

../utils/check_log.sh -b "$begin_date" mem $log_file
../utils/check_log.sh -b "$begin_date" warn $log_file.wf

end_ts=`date +%s`
echo "$0: Elapse time: $(shu-diff-timestamp $begin_ts $end_ts)"

exit 0;
