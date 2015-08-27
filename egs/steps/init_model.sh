#!/bin/bash

# Begin configuration section.
output_config_file=""
init_config_file=""
class_size="" # if not empty, choose a optimal size among candidate sizes,
              # which are seperated by semicolon
hs=""
train_file="" # must be given if $class_size not empty
train_line=200000 # lines used for test speed
train_config=""   
train_threads=""
# end configuration sections

echo "$0 $@"  # Print the command line for logging
function print_help()
{
  echo "usage: $0 <vocab-clm> <exp-dir>"
  echo "e.g.: $0 exp/vocab.clm exp/rnn"
  echo "options: "
  echo "     --init-config-file <config-file>    # init config file."
  echo "     --output-config-file <config-file>  # output config file."
  echo "     --class-size <size1;size2>          # candidate class sizes."
  echo "       --train-file <file>               # following options are used for choosing class size."
  echo "       --train-config <config>"           
  echo "       --train-threads <threads>"         
}

help_message=`print_help`

[ -f `dirname $0`/path.sh ] && . `dirname $0`/path.sh
[ -f ./path.sh ] && . ./path.sh

. ../utils/parse_options.sh || exit 1

if [ $# -ne 2 ]; then 
  print_help 1>&2 
  exit 1;
fi


vocab=$1
dir=$2

begin_date=`date +"%Y-%m-%d %H:%M:%S"`
begin_ts=`date +%s`
mkdir -p $dir

model_out=$dir/init.clm
init_log=$dir/log/init.log
output_mdl=$dir/output.clm
output_log=$dir/log/output.log

# choosing class size
init_dir=$dir/init
log_dir=$dir/log/init
train_part=$init_dir/train.part

function init_model ()
{
  cls_size=`echo $cls_size | tr -d '[[:space:]]'`
  if [ -z "$cls_size" ]; then
    output_opt=""
  else
    output_opt="--class-size=$cls_size"
  fi

  hs=`echo $hs | tr -d '[[:space:]]'`
  if [ ! -z "$hs" ]; then
    output_opt+=" --hs=$hs"
  fi

  if [ -z "$output_config_file" ]; then
  connlm-output --log-file=$output_log $output_opt \
             $vocab $output_mdl \
  || exit 1; 
  else
  connlm-output --log-file=$output_log $output_opt \
             --config=$output_config_file \
             $vocab $output_mdl \
  || exit 1; 
  fi
  if [ -z "$init_config_file" ]; then
  connlm-init --log-file=$init_log \
             $output_mdl $init_mdl \
  || exit 1; 
  else
  connlm-init --log-file=$init_log \
             --config=$init_config_file \
             $output_mdl $init_mdl \
  || exit 1; 
  fi
}

if [ -e "$model_out" ]; then
  echo "$0: Skipping Initializing $model_out ..."
  exit 0
fi

echo "$0: Initializing model $vocab to $model_out ..."
if [ -z "$class_size" ] || ! [[ $class_size == *";"* ]]; then
  init_mdl=$model_out
  cls_size=$class_size

  init_model
else
  if [ -z "$train_file" ]; then
    shu-err "train-file must be given when class-size not empty"
    exit 1
  fi

  sizes=`echo $class_size | tr ';' '\n'`
  for size in $sizes; do
    if ! [[ $size =~ ^[0-9]+$ ]]; then
      shu-err "size should be a number"
      exit 1
    fi
  done

  mkdir -p $init_dir
  mkdir -p $log_dir

  head -n $train_line $train_file > $train_part
  max_speed=0
  max_mdl=""
  for size in $sizes; do
    echo "$0: Trying class size: $size ..."
    output_mdl="$init_dir/output.$size.clm"
    output_log="$log_dir/output.$size.log"
    init_mdl="$init_dir/init.$size.clm"
    init_log="$log_dir/init.$size.log"
    cls_size=$size

    init_model

    train_log="$log_dir/train.$size.log"
    connlm-train --log-file=$train_log \
               --config="$train_config" \
               --num-thread=$train_threads \
               "$train_part" "$init_mdl" "-" > /dev/null \
    || exit 1; 

    speed=`../utils/get_value.sh "words/sec" $train_log`
    echo "$0:   Class size: $size, Speed: $speed."
    if [ 1 == $(bc <<< "$max_speed < $speed") ]; then
      max_speed=$speed
      max_mdl=$init_mdl
    fi
  done

  echo "$0: Best model: $max_mdl, Speed: $max_speed."
  cp $max_mdl $model_out
fi

if [ -z "$class_size" ] || ! [[ $class_size == *";"* ]]; then
  ../utils/check_log.sh -b "$begin_date" $init_log.wf $output_log.wf
else
  ../utils/check_log.sh -b "$begin_date" $log_dir/*.wf
fi

end_ts=`date +%s`
echo "$0: Elapse time: $(shu-diff-timestamp $begin_ts $end_ts)"

exit 0;

