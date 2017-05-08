#!/bin/bash

# Begin configuration section.
eval_conf=""
# end configuration sections

echo "$0 $@"  # Print the command line for logging
function print_help()
{
  echo "usage: $0 <corpus> <indomain-dir> <general-dir> <score_job>"
  echo "e.g.: $0 data/general.corpus exp/indomain/rnn+maxent/ exp/general/rnn+maxent/"
  echo "options: "
  echo "     --eval-conf <config-file>    # config file for connlm-eval."
}

help_message=`print_help`

. `dirname $0`/../../steps/path.sh || exit 1
[ -f `dirname $0`/../path.sh ] && . `dirname $0`/../path.sh

. ../utils/parse_options.sh || exit 1

if [ $# -ne 4 ]; then
  print_help 1>&2
  exit 1;
fi

corpus=$1
in_dir=$2
gen_dir=$3
score_job=$4

function score_corpus() {
  dir=$1

  score_dir="$dir/score/"
  mkdir -p "$score_dir"
  mkdir -p "$score_dir/log"

  for i in `seq -w $score_job`; do
    s=`expr $lines / $score_job \* \( $i - 1 \) + 1`
    if [ $i -eq $score_job ]; then
      e=$lines
    else
      e=`expr $lines / $score_job \* $i`
    fi
    ../steps/eval_model.sh $opts --eval-threads 1 \
          --out-prob "$score_dir/score.$i.prob" \
          --eval-opts "--print-sent-prob=true --out-log-base=10" \
          --log-file "$score_dir/log/score.$i.log" \
          "$dir" "sed -n '${s},${e}p' $corpus |" \
        > "$score_dir/log/score_sh.$i.log" 2>&1 &
    pids[$(expr $i + 0)]=$!
  done

  failed=0
  for i in `seq $score_job`; do
    wait ${pids[$i]}
    ret=$?
    if [ $ret -ne 0 ]; then
      echo "$0: Score corpus job $i failed."
      ((failed += 1))
    fi
  done

  for i in `seq -w $score_job`; do
    echo "$0: Score job $i log:"
    cat "$score_dir/log/score_sh.$i.log"
  done

  if [ $failed -ne 0 ]; then
    exit 1
  fi

  echo $score_job > "$score_dir/.jobs"
}

begin_date=`date +"%Y-%m-%d %H:%M:%S"`
begin_ts=`date +%s`

if [ -z "$eval_conf" ]; then
opts="--config-file $eval_conf"
else
opts=
fi

mkdir -p "$in_dir/score"
mkdir -p "$gen_dir/score"

in_lines=0
if [ -f "$in_dir/score/.lines" ]; then
in_lines=`cat "$in_dir/score/.lines"`
fi
gen_lines=0
if [ -f "$gen_dir/score/.lines" ]; then
gen_lines=`cat "$gen_dir/score/.lines"`
fi

if [ "$in_lines" -le 0 ] || [ "$in_lines" -ne "$gen_lines" ]; then
  echo "$0: (Re)Computing lines..."
  lines=`cat $corpus | wc -l | sed 's/ //g'`
  echo $lines > "$in_dir/score/.lines"
  echo $lines > "$gen_dir/score/.lines"
else
  lines=$in_lines
fi

st=1
if shu-in-range $st $stage; then
echo "$0: Stage $st --- Scoring indomain corpus..."
score_corpus $in_dir
fi
((st++))

if shu-in-range $st $stage; then
echo "$0: Stage $st --- Scoring general corpus..."
score_corpus $gen_dir
fi
((st++))

end_ts=`date +%s`
echo "$0: Elapse time: $(shu-diff-timestamp $begin_ts $end_ts)"

exit 0;
