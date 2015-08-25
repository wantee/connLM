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
conf=$conf_dir/$model_type
dir=$exp/$model_type

../steps/learn_vocab.sh --config-file $conf_dir/vocab.conf \
    $train_file $exp || exit 1;

../steps/init_model.sh --init-config-file $conf/init.conf \
        --output-config-file $conf/output.conf \
        --class-size "$class_size" \
          --train-file $train_file \
          --train-config $conf/train.conf \
          --train-threads $tr_thr \
        $exp/vocab.clm $dir || exit 1;

../steps/train_model.sh --train-config $conf/train.conf \
        --test-config $conf_dir/test.conf \
        --train-threads $tr_thr \
        --test-threads $test_thr \
        $train_file $valid_file $dir || exit 1;

../steps/test_model.sh --config-file $conf_dir/test.conf \
        --test-threads $test_thr \
        $dir $test_file || exit 1;
fi
((st++))

if shu-in-range $st $steps; then
echo
echo "Step $st: ${stepnames[$st]} ..."
train_file="$data_dir/general/train"
valid_file="$data_dir/general/valid"

exp=$exp_dir/general
conf=$conf_dir/$model_type
dir=$exp/$model_type

../steps/learn_vocab.sh --config-file $conf_dir/vocab.conf \
    $train_file $exp || exit 1;

../steps/init_model.sh --init-config-file $conf/init.conf \
        --output-config-file $conf/output.conf \
        --class-size "$class_size" \
          --train-file $train_file \
          --train-config $conf/train.conf \
          --train-threads $tr_thr \
        $exp/vocab.clm $dir || exit 1;

../steps/train_model.sh --train-config $conf/train.conf \
        --test-config $conf_dir/test.conf \
        --train-threads $tr_thr \
        --test-threads $test_thr \
        $train_file $valid_file $dir || exit 1;

../steps/test_model.sh --config-file $conf_dir/test.conf \
        --test-threads $test_thr \
        $dir $test_file || exit 1;
fi
((st++))

if shu-in-range $st $steps; then
echo
echo "Step $st: ${stepnames[$st]} ..."
echo "Scoring indomain corpus..."
../steps/test_model.sh --config-file $conf_dir/test.conf \
        --test-threads 1 \
        --out-prob "$exp_dir/indomain/$model_type/indomain.prob" \
        --test-opts "--print-sent-prob=true --out-log-base=10" \
        --log-file "$exp_dir/indomain/$model_type/log/score.log" \
        "$exp_dir/indomain/$model_type" "$data_dir/general.corpus" \
        > "$exp_dir/indomain/$model_type/log/score_sh.log" 2>&1 &
pid=$!

echo "Scoring general corpus..."
../steps/test_model.sh --config-file $conf_dir/test.conf \
        --test-threads 1 \
        --out-prob "$exp_dir/general/$model_type/general.prob" \
        --test-opts "--print-sent-prob=true --out-log-base=10" \
        --log-file "$exp_dir/general/$model_type/log/score.log" \
        "$exp_dir/general/$model_type" "$data_dir/general.corpus" 
ret=$?
if [ $ret -ne 0 ]; then
  echo "Score general corpus failed."
  kill $pid
  exit 1
fi

wait $pid
ret=$?
if [ $ret -ne 0 ]; then
  echo "Score indomain corpus failed."
  exit 1
fi
echo "Indomain score log: "
cat "$exp_dir/indomain/$model_type/log/score_sh.log"
fi
((st++))

if shu-in-range $st $steps; then
echo
echo "Step $st: ${stepnames[$st]} ..."
./local/select.sh "$data_dir/general.corpus" \
         "$exp_dir/indomain/$model_type/indomain.prob" \
         "$exp_dir/general/$model_type/general.prob" $thresh \
         "$exp_dir/selected.$model_type.$thresh" || exit 1
fi
((st++))

