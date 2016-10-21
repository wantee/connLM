#!/bin/bash

# Begin configuration section.
train_config=./conf/train.conf
eval_config=./conf/eval.conf
train_threads=1
eval_threads=1
max_iters=50
min_iters=0 # keep training, disable weight rejection, start learn-rate halving as usual,
keep_lr_iters=0 # fix learning rate for N initial epochs,
start_halving_impr=0.003
end_halving_impr=0.0003
halving_factor=0.5
# end configuration sections

echo "$0 $@"  # Print the command line for logging
function print_help()
{
  echo "usage: $0 <train-file> <valid-file> <exp-dir>"
  echo "e.g.: $0 data/train data/valid exp/rnn"
  echo "options: "
  echo "     --train-config <config-file>         # default: ./conf/train.conf, trian config file."
  echo "     --eval-config <config-file>          # default: ./conf/eval.conf, valid config file."
  echo "     --max-iters <number>                 # default: 20, maximum number of iterations"
  echo "     --train-threads <number>             # default: 1, number of training threads"
  echo "     --eval-threads <number>              # default: 1, number of evaluating threads"
  echo "     --min-iters <number>                 # default: 0, minimum number of iterations"
  echo "     --keep-lr-iters <number>             # default: 0, fix learning rate for N initial iterations"
  echo "     --start-halving-impr <value>         # default: 0.003, improvement starting halving"
  echo "     --end-halving-impr <value>           # default: 0.0003, improvement ending halving"
  echo "     --halving-factor <value>             # default: 0.5, halving factor"
}

help_message=`print_help`

[ -f `dirname $0`/path.sh ] && . `dirname $0`/path.sh
[ -f ./path.sh ] && . ./path.sh

. ../utils/parse_options.sh || exit 1

if [ $# -ne 3 ]; then
  print_help 1>&2
  exit 1;
fi

train_file=$1
valid_file=$2
dir=$3
mdl_init=init.clm

begin_date=`date +"%Y-%m-%d %H:%M:%S"`
begin_ts=`date +%s`
log_dir="$dir/log"

if [ ! -e "$dir/.learn_rate" ]; then
  shu-run connlm-train --dry-run=true --log-file="$log_dir/conf.log" \
             --config="$train_config" "$dir/$mdl_init" || exit 1
  shu-run "connlm-info --log-file=/dev/null $dir/$mdl_init \
          | ../utils/get_wt_names.pl > $dir/.wt.names" || exit 1
  shu-run "../utils/parse_lr.pl $dir/.wt.names < $log_dir/conf.log \
          > $dir/.learn_rate" || exit 1
fi

mdl_best="$mdl_init"
[ -e "$dir/.mdl_best" ] && mdl_best="$(cat $dir/.mdl_best)"

shu-run connlm-eval --log-file="$log_dir/valid.prerun.log" \
           --config="$eval_config" \
           --num-thread=$eval_threads \
           "$dir/$mdl_best" "$valid_file" \
|| exit 1;

loss=`../utils/get_value.sh "Entropy" $log_dir/valid.prerun.log`
echo "Prerun Valid Entropy: $(printf "%.6f" $loss)"

halving=0
[ -e $dir/.halving ] && halving=$(cat $dir/.halving)

rand_seed=1
for iter in $(seq -w $max_iters); do
  echo -n "ITERATION $iter: "
  mdl_next=${iter}.clm

  [ -e $dir/.done_iter$iter ] && echo -n "skipping... " && ls $dir/$mdl_next* && continue

  lrs=$(cat "$dir/.learn_rate")

  plrs=$(echo `sed -e 's/^--//;s/\^learn-rate=/=/' $dir/.learn_rate`)
  echo -n "LR($plrs), "

  shu-run connlm-train --log-file="$log_dir/train.${iter}.log" \
             --config="$train_config" \
             --num-thread=$train_threads \
             $lrs \
             --reader^random-seed=$rand_seed \
             "$dir/$mdl_best" "$train_file" "$dir/$mdl_next" \
  || exit 1;

  rand_seed=$((rand_seed+1))

  tr_loss=`../utils/get_value.sh "Entropy" $log_dir/train.${iter}.log`
  wpc=`../utils/get_value.sh "words/sec" $log_dir/train.${iter}.log`
  wpc=`bc <<< "scale=1; $wpc / 1000"`k
  echo -n "TrEnt $(printf "%.6f" $tr_loss), "
  echo -n "W/s $wpc, "

  shu-run connlm-eval --log-file="$log_dir/valid.${iter}.log" \
             --config="$eval_config" \
             --num-thread=$eval_threads \
             "$dir/$mdl_next" "$valid_file" \
  || exit 1;

  loss_new=`../utils/get_value.sh "Entropy" $log_dir/valid.${iter}.log`
  echo "CVEnt: $(printf "%.6f" $loss_new)"

  # accept or reject new parameters (based on objective function)
  loss_prev=$loss
  if [ 1 == $(bc <<< "$loss_new < $loss") -o $iter -le $keep_lr_iters -o $iter -le $min_iters ]; then
    loss=$loss_new
    mdl_best=$mdl_next
    echo "nnet accepted ($mdl_best)"
    echo $mdl_best > $dir/.mdl_best
  else
    echo "nnet rejected ($mdl_next)"
  fi

  # create .done file as a mark that iteration is over
  touch $dir/.done_iter$iter
  cp "$dir/.learn_rate" "$dir/.learn_rate.$iter" || exit 1

  # no learn-rate halving yet, if keep_lr_iters set accordingly
  [ $iter -le $keep_lr_iters ] && continue

  # stopping criterion
  rel_impr=$(bc <<< "scale=10; ($loss_prev-$loss)/$loss_prev")
  if [ 1 == $halving -a 1 == $(bc <<< "$rel_impr < $end_halving_impr") ]; then
    if [ $iter -le $min_iters ]; then
      echo "$0: we were supposed to finish, but we continue as min_iters: $min_iters"
      continue
    fi
    echo "$0: finished, too small rel. improvement $rel_impr"
    break
  fi

  # start annealing when improvement is low
  if [ 1 == $(bc <<< "$rel_impr < $start_halving_impr") ]; then
    halving=1
    echo $halving >$dir/.halving
  fi

  # do annealing
  if [ 1 == $halving ]; then
    ../utils/scale_lr.pl $halving_factor < "$dir/.learn_rate.$iter" \
        > "$dir/.learn_rate" || exit 1
  fi
done

# select the best network
if [ $mdl_best != $mdl_init ]; then
  ( cd $dir; ln -sf $mdl_best final.clm; )
  echo "$0: Succeeded training the Neural Network : $dir/final.clm"
else
  echo "$0: Error training neural network."
  exit 1
fi

../utils/check_log.sh -b "$begin_date" $log_dir/*.wf

end_ts=`date +%s`
echo "$0: Elapse time: $(shu-diff-timestamp $begin_ts $end_ts)"
words=`../utils/get_value.sh "Words" $log_dir/train.${iter}.log`
echo "$0: Effective speed: $(bc <<< "scale=1; ($iter * $words) / ($end_ts - $begin_ts) / 1000")k words/sec"

exit 0;

