#!/bin/bash

# run a standalone model using standard layout config
# Begin configuration section.
class_size="" # if not empty, choose a optimal size among candidate sizes,
              # which are seperated by semicolon
train_thr=1
test_thr=1
stage=""
# end configuration sections

echo "$0 $@"  # Print the command line for logging
function print_help()
{
  echo "usage: $0 <model-type> <conf-dir> <exp-dir> <train-file> <valid-file> [test-file]"
  echo "e.g.: $0 maxent~rnn conf exp data/train data/valid data/test"
  echo "options: "
  echo "     --train-thr <threads>    # default: 1."         
  echo "     --test-thr <threads>     # default: 1."         
  echo "     --class-size <xx:xx:xx>  # default: \"\". Class sizes to be tried."
}

help_message=`print_help`

[ -f `dirname $0`/path.sh ] && . `dirname $0`/path.sh
[ -f ./path.sh ] && . ./path.sh

. ../utils/parse_options.sh || exit 1

if [ $# -lt 5 ]; then 
  print_help 1>&2 
  exit 1;
fi

function model2filter() {
  filter=""
  for m in `echo $1 | tr '~' ' '`; do
    case "$m" in
      maxent) filter+="m,";;
      rnn) filter+="r,";;
      lbl) filter+="l,";;
      ffnn) filter+="f,";;
      *) echo "$0: Unsupported model: $m"; exit 1;;
    esac
  done
  echo ${filter%,}
}

model_type=$1
conf_dir=$2
exp_dir=$3
train_file=$4
valid_file=$5
test_file=$6

begin_date=`date +"%Y-%m-%d %H:%M:%S"`
begin_ts=`date +%s`

models=(${model_type//\~/ })
for model in ${models[@]}; do
  case "$model" in
    maxent|rnn|lbl|ffnn) ;;
    *) echo "$0: Unsupported model: $model"; exit 1;;
  esac
done

conf="$conf_dir/$model_type"
exp="$exp_dir/$model_type"
mkdir -p $exp 

# for run_standalone to get standard layout
ln -sf "`pwd`/$exp_dir/vocab.clm" $exp/vocab.clm || exit 1
ln -sf "`pwd`/$conf_dir/test.conf" $conf/test.conf || exit 1

model="${models[0]}"

st=1
if shu-in-range $st $stage; then
echo "$0: Stage $st --- Training $model..."
../steps/run_standalone.sh --class-size "$class_size" \
      --train-thr $train_thr --test-thr $test_thr \
    $model $conf $exp $train_file $valid_file $test_file || exit 1
fi
((st++))

set -o pipefail
models=("${models[@]:1}")
for i in `seq ${#models[@]}`; do
  if shu-in-range $st $stage; then
  echo "$0: Stage $st --- Initializing $m..."
  m=${models[$((i - 1))]}
  cls=`connlm-info $exp/$model/final.clm 2>/dev/null | ../utils/get_value.sh "Class size"`
  hs=`connlm-info $exp/$model/final.clm 2>/dev/null | ../utils/get_value.sh "HS"`
  
  # for run_standalone to get standard layout
  ln -sf "`pwd`/$conf/$model/output.conf" "$conf/$m/output.conf" || exit 1

  ../steps/run_standalone.sh --hs "$hs" --class-size "$cls" \
        --stage 1 --train-thr $train_thr --test-thr $test_thr \
      $m $conf $exp $train_file $valid_file $test_file || exit 1

  if [ $i -eq ${#models[@]} ]; then
    this_conf=$conf_dir
    this_exp=$exp_dir
    this_init_dir=$exp
  else
    this_conf=$conf
    this_exp=$exp
    this_init_dir="$exp/$model~$m"
    mkdir -p "$this_init_dir"
  fi
  echo "$0: Stage $st --- Mergine $model with $m..."
  connlm-merge --log-file=$exp/log/merge.$model~$m.log \
         mdl,$(model2filter $model):$exp/$model/final.clm \
         mdl,$(model2filter $m):$exp/$m/init.clm \
         "$this_init_dir/init.clm" || exit 1;

  model="$model~$m"

  echo "$0: Stage $st --- Training $model..."
  if [ $i -eq ${#models[@]} ]; then
    ../steps/run_standalone.sh --stage 2- \
          --train-thr $train_thr --test-thr $test_thr \
        $model $conf_dir $exp_dir $train_file $valid_file $test_file || exit 1
  else
    ../steps/run_standalone.sh --stage 2- \
          --train-thr $train_thr --test-thr $test_thr \
        $model $conf $exp $train_file $valid_file $test_file || exit 1
  fi
  fi
  ((st++))
done

end_ts=`date +%s`
echo "$0: Elapse time: $(shu-diff-timestamp $begin_ts $end_ts)"

exit 0;

