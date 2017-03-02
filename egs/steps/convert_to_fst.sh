#!/bin/bash

# run a standalone model using standard layout config
# Begin configuration section.
num_thr=1
output_ssyms=false
# end configuration sections

echo "$0 $@"  # Print the command line for logging
function print_help()
{
  echo "usage: $0 <model-type> <conf-dir> <exp-dir>"
  echo "e.g.: $0 rnn conf exp"
  echo "options: "
  echo "     --num-thr <threads>    # default: 1."
  echo "     --output-ssyms <true|false>   # default: false."
}

help_message=`print_help`

[ -f `dirname $0`/path.sh ] && . `dirname $0`/path.sh
[ -f ./path.sh ] && . ./path.sh

. ../utils/parse_options.sh || exit 1

if [ $# -lt 3 ]; then
  print_help 1>&2
  exit 1;
fi

model=$1
conf_dir=$2
exp_dir=$3

begin_date=`date +"%Y-%m-%d %H:%M:%S"`
begin_ts=`date +%s`

conf="$conf_dir/$model"
dir="$exp_dir/$model/fst"
mkdir -p $dir
mkdir -p "$dir/log"

log_file=${log_file:-"$dir/log/tofst.log"}
opts="--num-thread=$num_thr"
if $output_ssyms; then
  opts+=" --state-syms-file=$dir/g.ssyms"
fi
connlm-tofst --config="$conf/tofst.conf" --log-file="$log_file" \
             $opts \
             --word-syms-file="$dir/words.txt" \
             "$dir/../final.clm" "$dir/g.txt" || exit 1

echo "================================="
num_states=`../utils/get_value.sh "Total states in FST" $log_file`
echo "#states: $num_states"

num_arcs=`../utils/get_value.sh "Total arcs" $log_file`
echo "#arcs: $num_arcs"

max_gram=`../utils/get_value.sh "max_gram" $log_file`
echo "max-gram: $max_gram"


for i in `seq $max_gram`; do
  num_gram_wildcard=`../utils/get_value.sh "${i}-grams in wildcard subFST" $log_file`
  num_gram=`../utils/get_value.sh "${i}-grams in normal subFST" $log_file`
  num_gram_wildcard=${num_gram_wildcard:-0}
  num_gram=${num_gram:-0}
  echo "#${i}-gram: $num_gram_wildcard + $num_gram = `expr $num_gram_wildcard + $num_gram`"
done

echo "================================="

../utils/check_log.sh -b "$begin_date" $log_file.wf

end_ts=`date +%s`
echo "$0: Elapse time: $(shu-diff-timestamp $begin_ts $end_ts)"

exit 0;
