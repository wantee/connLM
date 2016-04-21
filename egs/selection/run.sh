#!/bin/bash

data_dir="./data"

conf_dir="./conf/"
exp_dir="./exp/"

model_type="rnn+maxent"
thresh=0

class_size=""
#class_size="50;100;150;200"
tr_thr=1
test_thr=1
score_job=4
hdfs_corpus=""
hdfs_output=""

realtype="float"

stepnames[1]="Prepare data"
stepnames[2]="Train in-domain model"
stepnames[3]="Train general model"
stepnames[4]="Score general corpus"
stepnames[5]="Select data"

steps_len=${#stepnames[*]}

function print_help()
{
  echo "usage: $0 [steps]"
  echo "e.g.: $0 -3,5,7-9,10-"
  echo "  stpes could be a number range within 1-$steps_len:"
  st=1
  while [ $st -le $steps_len ]
  do
  echo "   step$st:	${stepnames[$st]}"
  ((st++))
  done
  echo ""
  echo "options: "
  echo "     --thresh <thresh>             # threshold to be selected."
  echo "     --model_type <model_type>     # model type."
  echo "     --conf-dir <conf-dir>         # config directory."
  echo "     --exp-dir <exp-dir>           # exp directory."
  echo "     --hdfs-corpus <hdfs://path/to/corpus> # hdfs path of corpus."
  echo "     --hdfs-output <hdfs://path/to/output-dir> # hdfs path of output dir."
}

help_message=`print_help`

if [ ! -z "$realtype" ]; then
export CONNLM_REALTYPE="$realtype"
fi

. ../steps/path.sh || exit 1

. ../utils/parse_options.sh || exit 1

if [ $# -gt 1 ] || ! shu-valid-range $1; then
  print_help 1>&2
  exit 1
fi

steps=$1

mkdir -p "$exp_dir"
cp -r "${conf_dir%/}" "$exp_dir"
conf_dir="$exp_dir/`basename $conf_dir`"

mkdir -p "$data_dir"
test_file="$data_dir/test"

########################################
# replace following lines to corpus
########################################
ln -sf "`pwd`/../tiny/data/train" "$data_dir/general.corpus"
ln -sf "`pwd`/../tiny/data/valid" "$data_dir/indomain.corpus"
ln -sf "`pwd`/../tiny/data/test" "$data_dir/test"

st=1
if shu-in-range $st $steps; then
echo
echo "Step $st: ${stepnames[$st]} ..."
./local/prep_data.sh --valid-percentage "0.1" --shuf-text true \
    $data_dir || exit 1;
fi
((st++))

if shu-in-range $st $steps; then
echo
echo "Step $st: ${stepnames[$st]} ..."
train_file="$data_dir/indomain/train"
valid_file="$data_dir/indomain/valid"

exp=$exp_dir/indomain

../steps/learn_vocab.sh --config-file $conf_dir/vocab.conf \
    $train_file $exp || exit 1;

../steps/run_standalone.sh --class-size "$class_size" \
      --train-thr $tr_thr --test-thr $test_thr \
    $model_type $conf_dir $exp $train_file $valid_file $test_file \
  || exit 1;
fi
((st++))

if shu-in-range $st $steps; then
echo
echo "Step $st: ${stepnames[$st]} ..."
train_file="$data_dir/general/train"
valid_file="$data_dir/general/valid"

exp=$exp_dir/general

../steps/learn_vocab.sh --config-file $conf_dir/vocab.conf \
    $train_file $exp || exit 1;

../steps/run_standalone.sh --class-size "$class_size" \
      --train-thr $tr_thr --test-thr $test_thr \
    $model_type $conf_dir $exp $train_file $valid_file $test_file \
  || exit 1;
fi
((st++))

if shu-in-range $st $steps; then
echo
echo "Step $st: ${stepnames[$st]} ..."
if [ -n "$hdfs_corpus" ]; then
  if [ -z "$hdfs_output" ]; then
    shu-err "--hdfs-output must be specified"
    exit 1
  fi
  ./local/mr_score.sh "$hdfs_corpus" "$hdfs_output/score" "$exp_dir" \
         "$exp_dir/indomain/$model_type" "$exp_dir/general/$model_type" \
         || exit 1
else
  ./local/score.sh --test-conf "$conf_dir/test.conf" \
      "$data_dir/general.corpus" \
      "$exp_dir/indomain/$model_type/" "$exp_dir/general/$model_type/" \
      $score_job  || exit 1
fi
fi
((st++))

if shu-in-range $st $steps; then
echo
echo "Step $st: ${stepnames[$st]} ..."
if [ -n "$hdfs_corpus" ]; then
  if [ -z "$hdfs_output" ]; then
    shu-err "--hdfs-output must be specified"
    exit 1
  fi
  ./local/mr_select.sh "$hdfs_output/score" "$thresh" \
         "$exp_dir" "$hdfs_output/selected.$model_type.$thresh" \
         || exit 1
  hadoop fs -cat "$hdfs_output/selected.$model_type.$thresh/part-*" \
         > $exp_dir/selected.$model_type.$thresh || exit 1
else
  ./local/select.sh "$data_dir/general.corpus" \
         "$exp_dir/indomain/$model_type/score" \
         "$exp_dir/general/$model_type/score" $thresh \
         "$exp_dir/selected.$model_type.$thresh" || exit 1
fi
fi
((st++))

