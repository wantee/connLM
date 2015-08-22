#!/bin/bash

data=./corpus

# base url for downloads.
data_url=www.openslr.org/resources/12
lm_url=www.openslr.org/resources/11

train_file=./data/train
valid_file=./data/valid
test_file=./data/test
vocab_file=./data/vocab

conf_dir=./conf/
exp_dir=./exp/

#class_size=""
class_size="50;100;150;200;250;300;350;400"
tr_thr=24
test_thr=24

realtype="float"

stepnames[1]="Prepare data"
stepnames[2]="Learn Vocab"
stepnames[3]="Train MaxEnt model"
stepnames[4]="Train RNN model"
stepnames[5]="Train RNN+MaxEnt model"

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

st=1
if shu-in-range $st $steps; then
echo
echo "Step $st: ${stepnames[$st]} ..."
for part in dev-clean test-clean dev-other test-other train-clean-100 train-clean-360 train-other-500; do
  local/download_and_untar.sh $data $data_url $part
done

local/download_lm.sh $lm_url $data || exit 1

if [ ! -e "$vocab_file" ]; then
  mkdir -p `basename $vocab_file` || exit 1
  cp $data/librispeech-vocab.txt $vocab_file
fi

if [ ! -e "$train_file" ]; then
  echo "Preparing train..."
  mkdir -p `basename $train_file` || exit 1
  python local/filt.py $vocab_file <(gunzip -c $data/librispeech-lm-norm.txt.gz)  | \
             perl ../utils/shuf.pl > $train_file || exit 1
fi

if [ ! -e "$valid_file" ]; then
  echo "Preparing valid..."
  mkdir -p `basename $valid_file` || exit 1
  > $valid_file || exit 1
  for part in dev-clean dev-other train-clean-100 train-clean-360 train-other-500; do
    local/data_prep.sh $data/LibriSpeech/$part $valid_file || exit 1
  done
fi

if [ ! -e "$test_file" ]; then
  echo "Preparing test..."
  mkdir -p `basename $test_file` || exit 1
  > $test_file || exit 1
  for part in test-clean test-other; do
    local/data_prep.sh $data/LibriSpeech/$part $test_file || exit 1
  done
  fi
fi
((st++))

if shu-in-range $st $steps; then
echo
echo "Step $st: ${stepnames[$st]} ..."
../steps/learn_vocab.sh --config-file $conf_dir/vocab.conf \
    $train_file $exp_dir || exit 1;
fi
((st++))

if shu-in-range $st $steps; then
echo
echo "Step $st: ${stepnames[$st]} ..."
conf=$conf_dir/maxent
dir=$exp_dir/maxent
../steps/init_model.sh --init-config-file $conf/init.conf \
        --output-config-file $conf/output.conf \
        --class-size "$class_size" \
          --train-file $train_file \
          --train-config $conf/train.conf \
          --train-threads $tr_thr \
        $exp_dir/vocab.clm $dir || exit 1;

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
conf=$conf_dir/rnn
dir=$exp_dir/rnn
../steps/init_model.sh --init-config-file $conf/init.conf \
        --output-config-file $conf/output.conf \
        --class-size "$class_size" \
          --train-file $train_file \
          --train-config $conf/train.conf \
          --train-threads $tr_thr \
        $exp_dir/vocab.clm $dir || exit 1;

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
conf=$conf_dir/rnn+maxent
dir=$exp_dir/rnn+maxent
../steps/init_model.sh --init-config-file $conf/init.conf \
        --output-config-file $conf/output.conf \
        --class-size "$class_size" \
          --train-file $train_file \
          --train-config $conf/train.conf \
          --train-threads $tr_thr \
        $exp_dir/vocab.clm $dir || exit 1;

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

