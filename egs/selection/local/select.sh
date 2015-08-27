#!/bin/bash

# Begin configuration section.
# end configuration sections

echo "$0 $@"  # Print the command line for logging
function print_help()
{
  echo "usage: $0 <corpus> <indomain-score-dir> <general-score-dir> <thresh> <out-selected>"
  echo "e.g.: $0 data/general.corpus exp/indomain/rnn+maxent/score exp/general/rnn+maxent/score 0.0 exp/selected"
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
in_dir=$2
gen_dir=$3
thresh=$4
out=$5

begin_date=`date +"%Y-%m-%d %H:%M:%S"`
begin_ts=`date +%s`

for f in "$in_dir"/score.*.prob "$in_dir"/.jobs \
         "$gen_dir"/score.*.prob "$gen_dir"/.jobs; do
  [ ! -f $f ] && echo "$0: no such file $f" && exit 1;
done

if [ `ls "$in_dir"/score.*.prob | wc -l` -ne `cat "$in_dir/.jobs"` ]; then
  echo "prob files number not equal to job number. dir[$in_dir]"
  exit 1
fi

if [ `ls "$gen_dir"/score.*.prob | wc -l` -ne `cat "$gen_dir/.jobs"` ]; then
  echo "prob files number not equal to job number. dir[$gen_dir]"
  exit 1
fi

paste <(awk 'NF' "$corpus") <(cat "$in_dir"/score.*.prob) \
      <(cat "$gen_dir"/score.*.prob) \
  | perl "local/select.pl" $thresh > "$out" || exit 1

end_ts=`date +%s`
echo "$0: Elapse time: $(shu-diff-timestamp $begin_ts $end_ts)"

exit 0;

