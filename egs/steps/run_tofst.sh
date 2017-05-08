#!/bin/bash

# Begin configuration section.
tofst_thr=1
stage=""
bloom_filter_text_file=""
wildcard_state_text_file=""
# end configuration sections

echo "$0 $@"  # Print the command line for logging
function print_help()
{
  echo "usage: $0 <conf-dir> <exp-dir>"
  echo "e.g.: $0 conf/rnn exp/rnn"
  echo "options: "
  echo "     --tofst-thr <threads>    # default: 1."
  echo "     --stage <stages>         # default: \"\"."
  echo "     --bloom-filter-text-file <file>   # default: \"\"."
  echo "     --wildcard-state-text-file <file>   # default: \"\"."
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

mkdir -p $exp_dir

st=1
blmflt="$exp_dir/`basename $bloom_filter_text_file`.blmflt"
if shu-in-range $st $stage; then
  echo "$0: Stage $st --- Building bloom filter..."
  if [ -z "$bloom_filter_text_file" ]; then
    echo "bloom_filter_text_file not set, skipping..."
    blmflt=""
  else
    ../steps/build_bloom_filter.sh "$conf_dir" "$exp_dir" \
      "$bloom_filter_text_file" "$blmflt" || exit 1
  fi
fi
((st++))

if shu-in-range $st $stage; then
  echo "$0: Stage $st --- Generating wildcard state..."
  ws="$exp_dir/ws.txt"
  opts=""
  if [ -n "$wildcard_state_text_file" ]; then
    opts+="--eval-text-file $wildcard_state_text_file"
  fi

  ../steps/generate_wildcard_state.sh $opts "$conf_dir" "$exp_dir" "$ws" || exit 1
fi
((st++))

if shu-in-range $st $stage; then
  echo "$0: Stage $st --- Converting to FST..."
    opts=""
    if [ -n "$blmflt" ]; then
      opts+="--bloom-filter-file $blmflt "
    fi
    if [ -n "$ws" ]; then
      opts+="--wildcard-state-file $ws "
    fi
    ../steps/convert_to_fst.sh --num-thr $tofst_thr \
        $opts "$conf_dir" "$exp_dir" || exit 1;
fi
((st++))

end_ts=`date +%s`
echo "$0: Elapse time: $(shu-diff-timestamp $begin_ts $end_ts)"

exit 0;
