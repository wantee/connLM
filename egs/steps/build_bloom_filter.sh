#!/bin/bash

# run a standalone model using standard layout config
# Begin configuration section.
num_thr=1
# end configuration sections

echo "$0 $@"  # Print the command line for logging
function print_help()
{
  echo "usage: $0 <conf-dir> <exp-dir> <text-file> <out-file>"
  echo "e.g.: $0 conf exp data/train exp/train.blmflt"
}

help_message=`print_help`

[ -f `dirname $0`/path.sh ] && . `dirname $0`/path.sh
[ -f ./path.sh ] && . ./path.sh

. ../utils/common_utils.sh || exit 1
. ../utils/parse_options.sh || exit 1

if [ $# -lt 4 ]; then
  print_help 1>&2
  exit 1;
fi

conf_dir=$1
exp_dir=$2
text_file=`rxfilename "$3"`
out_file=$4

begin_date=`date +"%Y-%m-%d %H:%M:%S"`
begin_ts=`date +%s`

mkdir -p "$exp_dir/log"

log_file=${log_file:-"$exp_dir/log/bf-build.log"}
shu-run bloom-filter-build --config="$conf_dir/bf-build.conf" \
                     --log-file="$log_file" \
                     "$exp_dir/vocab.clm" "'$text_file'" "$out_file" || exit 1

echo "================================="
shu-run bloom-filter-info --log-file="$exp_dir/log/bf-info.log" \
                          --verbose=true "$out_file" || exit 1
echo "================================="

../utils/check_log.sh -b "$begin_date" $log_file.wf

end_ts=`date +%s`
echo "$0: Elapse time: $(shu-diff-timestamp $begin_ts $end_ts)"

exit 0;
