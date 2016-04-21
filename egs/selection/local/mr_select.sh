#!/bin/bash

# Begin configuration section.
# end configuration sections

echo "$0 $@"  # Print the command line for logging
function print_help()
{
  echo "usage: $0 <hdfs_score> <thresh> <exp-dir> <out-selected>"
  echo "e.g.: $0 hdfs:///tmp/score 0.0 exp hdfs:///tmp/selected"
  echo "options: "
}

help_message=`print_help`

. `dirname $0`/../../steps/path.sh || exit 1
[ -f `dirname $0`/../path.sh ] && . `dirname $0`/../path.sh

. ../utils/parse_options.sh || exit 1

if [ $# -ne 4 ]; then
  print_help 1>&2
  exit 1;
fi

score_dir=$1
thresh=$2
dir=$3
out=$4

log_dir="$dir/log/"
mkdir -p "$log_dir"

begin_date=`date +"%Y-%m-%d %H:%M:%S"`
begin_ts=`date +%s`

HADOOP_HOME="$(dirname `which hadoop`)/.."
hadoop jar $HADOOP_HOME/contrib/streaming/hadoop-streaming*.jar \
           -D mapred.job.name="Select Text [$thresh] ($1 -> $2)" \
           -D mapred.reduce.tasks=0 \
           -input "$score_dir" \
           -output "$out" \
           -mapper "perl select.pl $thresh" \
           -file "$(cd `dirname $0`; pwd)/select.pl" \
           > "$log_dir/mr_select.log" 2>&1 || exit 1
jobid=`grep "Running job" "$log_dir/mr_select.log" | awk -F: '{print $NF}'`
total=`hadoop job -counter $jobid connLM "Total Sentences"`
selected=`hadoop job -counter $jobid connLM "Selected Sentences"`
echo "Total Sentences: $total" | tee -a "$log_dir/mr_select.log"
echo "Selected Sentences: $selected" | tee -a "$log_dir/mr_select.log"

end_ts=`date +%s`
echo "$0: Elapse time: $(shu-diff-timestamp $begin_ts $end_ts)"

exit 0;
