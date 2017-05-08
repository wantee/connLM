#!/bin/bash

# Begin configuration section.
# end configuration sections

echo "$0 $@"  # Print the command line for logging
function print_help()
{
  echo "usage: $0 <hdfs_corpus> <hdfs_score> <exp-dir> <indomain-dir> <general-dir>"
  echo "e.g.: $0 hdfs:///corpus/ hdfs:///tmp/score exp exp/indomain/rnn+maxent/ exp/general/rnn+maxent/"
  echo "options: "
}

help_message=`print_help`

. `dirname $0`/../../steps/path.sh || exit 1
[ -f `dirname $0`/../path.sh ] && . `dirname $0`/../path.sh

. ../utils/parse_options.sh || exit 1

if [ $# -ne 5 ]; then
  print_help 1>&2
  exit 1;
fi

corpus=$1
score_dir=$2
dir=$3
inmdl="$4/final.clm"
genmdl="$5/final.clm"

ln -sf "`pwd`/$inmdl" "$inmdl.in"
inmdl="$inmdl.in"
ln -sf "`pwd`/$genmdl" "$genmdl.gen"
genmdl="$genmdl.gen"

log_dir="$dir/log/"
mkdir -p "$log_dir"

begin_date=`date +"%Y-%m-%d %H:%M:%S"`
begin_ts=`date +%s`

libs="libconnlm.so libstutils.so libmkl*"
opts=""
for lib in $libs; do
  f=$(ldd `which connlm-eval `| awk -F'>' '{print $NF}' | awk '{print $1}' \
      | grep $lib)
  opts+="-file $f "
done

HADOOP_HOME="$(dirname `which hadoop`)/.."
hadoop jar $HADOOP_HOME/contrib/streaming/hadoop-streaming*.jar \
           -D mapred.job.name="Score Text ($1 -> $2)" \
           -D mapred.reduce.tasks=0 \
           -input "$corpus" \
           -output "$score_dir" \
           -mapper "./mr_score_mapper.sh `basename $inmdl` `basename $genmdl`" \
           -file "$(cd `dirname $0`; pwd)/mr_score_mapper.sh" \
           -file `which connlm-eval` \
           -file $inmdl -file $genmdl $opts > "$log_dir/mr_score.log" 2>&1 \
           || exit 1

end_ts=`date +%s`
echo "$0: Elapse time: $(shu-diff-timestamp $begin_ts $end_ts)"

exit 0;
