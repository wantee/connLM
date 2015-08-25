#!/bin/bash

# Begin configuration section.
# end configuration sections

echo "$0 $@"  # Print the command line for logging
function print_help()
{
  echo "usage: $0 <corpus> <indomain-score> <general-score> <thresh> <out-selected>"
  echo "e.g.: $0 data/general.corpus exp/indomain/rnn+maxent/indomain.prob exp/general/rnn+maxent/general.prob 0.0 exp/selected"
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
in_score=$2
gen_score=$3
thresh=$4
out=$5

begin_date=`date +"%Y-%m-%d %H:%M:%S"`
begin_ts=`date +%s`

perl "local/select.pl" "$corpus" "$in_score" "$gen_score" $thresh \
     "$out" || exit 1

end_ts=`date +%s`
echo "Elapse time: $(shu-diff-timestamp $begin_ts $end_ts)"

exit 0;

