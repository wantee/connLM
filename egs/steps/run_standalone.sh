#!/bin/bash

# run a standalone model using standard layout config
# Begin configuration section.
train_thr=1
eval_thr=1
stage=""
start_halving_impr=0.003
end_halving_impr=0.0003
# end configuration sections

echo "$0 $@"  # Print the command line for logging
function print_help()
{
  echo "usage: $0 <model-type> <conf-dir> <exp-dir> <train-file> <valid-file> [test-file]"
  echo "e.g.: $0 rnn conf exp data/train data/valid data/test"
  echo "options: "
  echo "     --train-thr <threads>    # default: 1."
  echo "     --eval-thr <threads>     # default: 1."
  echo "     --stage <stages>         # default: \"\"."
  echo "     --start-halving-impr <value>         # default: 0.003, improvement starting halving"
  echo "     --end-halving-impr <value>           # default: 0.0003, improvement ending halving"
}

help_message=`print_help`

[ -f `dirname $0`/path.sh ] && . `dirname $0`/path.sh
[ -f ./path.sh ] && . ./path.sh

. ../utils/parse_options.sh || exit 1

if [ $# -lt 5 ]; then
  print_help 1>&2
  exit 1;
fi

model=$1
conf_dir=$2
exp_dir=$3
train_file=$4
valid_file=$5
test_file=$6

begin_date=`date +"%Y-%m-%d %H:%M:%S"`
begin_ts=`date +%s`

conf="$conf_dir/$model"
dir="$exp_dir/$model"

mkdir -p $dir

st=1
if shu-in-range $st $stage; then
echo "$0: Stage $st --- Initialising model..."
../steps/init_model.sh --init-config-file "$conf/init.conf" \
        --output-config-file "$conf/output.conf" \
        "$exp_dir/vocab.clm" "$conf/topo" "$dir" || exit 1;
fi
((st++))

if shu-in-range $st $stage; then
echo "$0: Stage $st --- Training model..."
../steps/train_model.sh --train-config "$conf/train.conf" \
        --eval-config "$conf_dir/eval.conf" \
        --train-threads "$train_thr" \
        --eval-threads "$eval_thr" \
        --start-halving-impr "$start_halving_impr" \
        --end-halving-impr "$end_halving_impr" \
        "$train_file" "$valid_file" "$dir" || exit 1;
fi
((st++))

if shu-in-range $st $stage; then
if [ ! -z "$test_file" ]; then
echo "$0: Stage $st --- Testing model..."
../steps/eval_model.sh --config-file "$conf_dir/eval.conf" \
        --eval-threads "$eval_thr" \
        "$dir" "$test_file" || exit 1;
fi
fi
((st++))

end_ts=`date +%s`
echo "$0: Elapse time: $(shu-diff-timestamp $begin_ts $end_ts)"

exit 0;
