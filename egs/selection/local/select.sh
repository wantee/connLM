#!/bin/bash

# Begin configuration section.
parallel=true
merge_out=true
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

log_dir=`dirname $out`/log
mkdir -p $log_dir

for f in "$in_dir"/score.*.prob "$in_dir"/.jobs \
         "$gen_dir"/score.*.prob "$gen_dir"/.jobs; do
  [ ! -f $f ] && echo "$0: no such file $f" && exit 1;
done

in_jobs=`cat "$in_dir/.jobs"`
if [ `ls "$in_dir"/score.*.prob | wc -l` -ne $in_jobs ]; then
  echo "$0: prob files number not equal to job number. dir[$in_dir]"
  exit 1
fi

gen_jobs=`cat "$gen_dir/.jobs"`
if [ `ls "$gen_dir"/score.*.prob | wc -l` -ne "$gen_jobs" ]; then
  echo "$0: prob files number not equal to job number. dir[$gen_dir]"
  exit 1
fi

if $parallel; then
  if [ "$in_jobs" -ne "$gen_jobs" ]; then
    echo "$0: jobs not match. can not parallelise."
    exit 1
  fi

  if [ ! -f "$in_dir/.lines" ]; then
    echo "$0: $in_dir/.lines not exist."
    exit 1
  fi
  if [ ! -f "$gen_dir/.lines" ]; then
    echo "$0: $gen_dir/.lines not exist."
    exit 1
  fi

  in_lines=`cat "$in_dir/.lines"`
  gen_lines=`cat "$gen_dir/.lines"`
  
  if [ "$in_lines" -le 0 ] || [ "$in_lines" -ne "$gen_lines" ]; then
    echo "$0: lines not match."
    exit 1
  fi
  lines=$in_lines
  score_job=$in_jobs

  for i in `seq -w $score_job`; do
    s=`expr $lines / $score_job \* \( $i - 1 \) + 1`
    if [ $i -eq $score_job ]; then
      e=$lines
    else
      e=`expr $lines / $score_job \* $i`
    fi

    paste <(sed -n "${s},${e}p" "$corpus" | awk 'NF' ) \
      <(cat "$in_dir"/score.$i.prob) <(cat "$gen_dir"/score.$i.prob) \
      | perl "local/select.pl" $thresh > "$out-$i" \
             2> $log_dir/select.$i.log || exit 1

    pids[$(expr $i + 0)]=$!
  done
  
  failed=0
  for i in `seq $score_job`; do
    wait ${pids[$i]}
    ret=$?
    if [ $ret -ne 0 ]; then
      echo "$0: Select job $i failed."
      ((failed += 1))
    fi
  done
  
  if [ $failed -ne 0 ]; then
    exit 1
  fi

  cat "$log_dir"/select.*.log | awk -F: ' \
           {if(NR%2){sp = $1; s += $2}else{tp = $1; t += $2}}\
           END{print sp":", s; print tp":", t;}' 

  if $merge_out; then
    cat $out-* > $out
    rm $out-*
  fi
else
  paste <(awk 'NF' "$corpus") <(cat "$in_dir"/score.*.prob) \
        <(cat "$gen_dir"/score.*.prob) \
    | perl "local/select.pl" $thresh > "$out" \
           2> $log_dir/select.log || exit 1
  cat "$log_dir/select.log"
fi

end_ts=`date +%s`
echo "$0: Elapse time: $(shu-diff-timestamp $begin_ts $end_ts)"

exit 0;

