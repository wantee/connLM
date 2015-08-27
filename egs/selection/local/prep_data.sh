#!/bin/bash

# Begin configuration section.
valid_percentage="0.1"
test_percentage="0"
corpus_line="-1"
shuf_text=true
# end configuration sections

echo "$0 $@"  # Print the command line for logging
function print_help()
{
  echo "usage: $0 <data-dir>"
  echo "e.g.: $0 data"
  echo "options: "
  echo "     --valid-percentage <percentage>  # percentage text used as valid set."
  echo "     --test-percentage <percentage>  # percentage text used as test set."
  echo "     --corpus-line <lines>  # number of lines used for train model, negtive value denotes using all indomain text."
  echo "     --shuf-text <true|false>  # whether shuffle the input text, do not need to be true if the input already shuffled"
}

help_message=`print_help`

. `dirname $0`/../../steps/path.sh || exit 1
[ -f `dirname $0`/../path.sh ] && . `dirname $0`/../path.sh

. ../utils/parse_options.sh || exit 1

if [ $# -ne 1 ]; then 
  print_help 1>&2 
  exit 1;
fi

dir=$1

for f in "$dir/general.corpus" "$dir/indomain.corpus"; do
  [ ! -f $f ] && echo "$0: no such file $f" && exit 1;
done

function std_layout {
  corpus=$1
  lines=$2
  out_dir=$3

  mkdir -p "$out_dir"

  if $shuf_text; then
    perl ../utils/shuf.pl < $corpus \
      | head -n $lines > $out_dir/all || exit 1
  else
    head -n $lines $corpus > $out_dir/all || exit 1
  fi

  valid_size=$(bc <<< "scale=0;$lines * $valid_percentage / 1")
  head -n $valid_size $out_dir/all > $out_dir/valid || exit 1

  test_size=$(bc <<< "scale=0;$lines * $test_percentage / 1")
  tail -n $test_size $out_dir/all > $out_dir/test || exit 1

  sed -n "$((valid_size + 1)),$((lines - $test_size))p" $out_dir/all > $out_dir/train || exit 1
  echo "***Train size: $((lines - $test_size - valid_size))"
  echo "***Valid size: $valid_size"
  echo "***Test size: $test_size"
}

begin_date=`date +"%Y-%m-%d %H:%M:%S"`
begin_ts=`date +%s`

if [ "$corpus_line" -lt 0 ]; then
  corpus_line=`cat "$dir/indomain.corpus" | wc -l | sed 's/ //g'`
  corpus_line=`echo $corpus_line | sed 's/ //g'`
fi

echo "**Layouting indomain corpus with $corpus_line lines..."
std_layout $dir/indomain.corpus $corpus_line $dir/indomain || exit 1

echo "**Layouting general corpus with $corpus_line lines..."
std_layout $dir/general.corpus $corpus_line $dir/general || exit 1

end_ts=`date +%s`
echo "$0: Elapse time: $(shu-diff-timestamp $begin_ts $end_ts)"

exit 0;

