#!/bin/bash

data=./corpus

# base url for downloads.
data_url=http://www.fit.vutbr.cz/~imikolov/rnnlm/simple-examples.tgz

train_file=./data/train
valid_file=./data/valid
test_file=./data/test
vocab_file=./data/vocab

conf_dir=./conf/
exp_dir=./exp/

#class_size=""
class_size="100"
tr_thr=4
test_thr=4

realtype="float"

stepnames[1]="Prepare data"
stepnames[2]="Learn Vocab"
stepnames[3]="Train MaxEnt model"
stepnames[4]="Train RNN model"
stepnames[5]="Train RNN+MaxEnt model"
stepnames[6]="Train RNN~MaxEnt merge model"

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

st=1
if shu-in-range $st $steps; then
echo
echo "Step $st: ${stepnames[$st]} ..."
local/download_data.sh $data_url $data || exit 1

data=`cd $data; pwd`
mkdir -p `dirname "$train_file"`
ln -sf $data/simple-examples/data/ptb.train.txt $train_file || exit 1
mkdir -p `dirname "$valid_file"`
ln -sf $data/simple-examples/data/ptb.valid.txt $valid_file || exit 1
mkdir -p `dirname "$test_file"`
ln -sf $data/simple-examples/data/ptb.test.txt $test_file || exit 1

fi
((st++))

if shu-in-range $st $steps; then
echo
echo "Step $st: ${stepnames[$st]} ..."
../steps/learn_vocab.sh $train_file $exp_dir || exit 1;
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

if shu-in-range $st $steps; then
echo
echo "Step $st: ${stepnames[$st]} ..."

echo "Training MaxEnt..."
local_exp=$exp_dir/maxent~rnn

maxent_conf=$conf_dir/maxent~rnn/maxent
maxent_dir=$local_exp/maxent

conf=$maxent_conf
dir=$maxent_dir
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

echo "Initializing RNN..."
rnn_conf=$conf_dir/maxent~rnn/rnn
rnn_dir=$local_exp/rnn
cls=`connlm-info $maxent_dir/final.clm 2>/dev/null | ../utils/get_value.sh "Class size"`
hs=`connlm-info $maxent_dir/final.clm 2>/dev/null | ../utils/get_value.sh "HS"`

conf=$rnn_conf
dir=$rnn_dir
../steps/init_model.sh --init-config-file $conf/init.conf \
        --hs "$hs" \
        --class-size "$cls" \
          --train-file $train_file \
          --train-config $conf_dir/maxent~rnn/train.conf \
          --train-threads $tr_thr \
        $exp_dir/vocab.clm $dir || exit 1;

fi

echo "Mergine MaxEnt~RNN..."
connlm-merge --log-file=$local_exp/log/merge.log \
       mdl,m:$local_exp/maxent/final.clm \
       mdl,r:$local_exp/rnn/init.clm \
       $local_exp/init.clm || exit 1;

echo "Training MaxEnt~RNN..."
conf=$conf_dir/maxent~rnn/
dir=$local_exp/
../steps/train_model.sh --train-config $conf/train.conf \
        --test-config $conf_dir/test.conf \
        --train-threads $tr_thr \
        --test-threads $test_thr \
        $train_file $valid_file $dir || exit 1;

../steps/test_model.sh --config-file $conf_dir/test.conf \
        --test-threads $test_thr \
        $dir $test_file || exit 1;

((st++))

