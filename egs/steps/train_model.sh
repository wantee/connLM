#!/bin/bash

# Begin configuration section.
train_config=./conf/train.conf
test_config=./conf/test.conf
max_iters=50
min_iters=0 # keep training, disable weight rejection, start learn-rate halving as usual,
keep_lr_iters=0 # fix learning rate for N initial epochs,
start_halving_impr=0.003
end_halving_impr=0.0003
halving_factor=0.5
# end configuration sections

echo "$0 $@"  # Print the command line for logging
[ -f path.sh ] && . ./path.sh

. ../utils/parse_options.sh || exit 1

if [ $# -ne 3 ]; then 
  echo "usage: $0 <train-file> <valid-file> <exp-dir>"
  echo "e.g.: $0 data/train data/valid exp/rnn"
  echo "options: "
  echo "     --train-config <config-file>         # default: ./conf/train.conf, trian config file."
  echo "     --test-config <config-file>          # default: ./conf/test.conf, valid config file."
  echo "     --max-iters <number>                 # default: 20, maximum number of iterations"
  echo "     --min-iters <number>                 # default: 0, minimum number of iterations"
  echo "     --keep-lr-iters <number>             # default: 0, fix learning rate for N initial iterations"
  echo "     --start-halving-impr <value>         # default: 0.01, improvement starting halving"
  echo "     --end-halving-impr <value>           # default: 0.001, improvement ending halving"
  echo "     --halving-factor <value>             # default: 0.5, halving factor"
  exit 1;
fi

train_file=$1
valid_file=$2
dir=$3
mdl_init=init.clm

log_dir=$dir/log

if [ ! -e $dir/.learn_rate ]; then
  connlm-train --dry-run=true --log-file="$log_dir/conf.log" \
             --config="$train_config" || exit 1
  ../utils/parse_lr.pl < "$log_dir/conf.log" > "$dir/.learn_rate" || exit 1
fi

lr_rnn=$(sed -n '1p' $dir/.learn_rate)
lr_maxent=$(sed -n '2p' $dir/.learn_rate)
lr_lbl=$(sed -n '3p' $dir/.learn_rate)
lr_ffnn=$(sed -n '4p' $dir/.learn_rate)

mdl_best=$mdl_init
[ -e $dir/.mdl_best ] && mdl_best=$(cat $dir/.mdl_best)

connlm-test --log-file="$log_dir/valid.prerun.log" \
           --config="$test_config" \
           "$dir/$mdl_best" "$valid_file" \
|| exit 1; 

loss=`../utils/get_entropy.sh $log_dir/valid.prerun.log`
echo "Prerun Valid Entropy: $(printf "%.4f" $loss)"

halving=0
[ -e $dir/.halving ] && halving=$(cat $dir/.halving)

rand_seed=1
for iter in $(seq -w $max_iters); do
  echo -n "ITERATION $iter: "
  mdl_next=${iter}.clm

  [ -e $dir/.done_iter$iter ] && echo -n "skipping... " && ls $dir/$mdl_next* && continue 
  
  connlm-train --log-file="$log_dir/train.${iter}.log" \
             --config="$train_config" \
             --rnn^learn-rate=$lr_rnn \
             --maxent^learn-rate=$lr_maxent \
             --lbl^learn-rate=$lr_lbl \
             --ffnn^learn-rate=$lr_ffnn \
             --random-seed=$rand_seed \
             "$train_file" "$dir/$mdl_best" "$dir/$mdl_next" \
  || exit 1; 

  rand_seed=$((rand_seed+1))

  tr_loss=`../utils/get_entropy.sh $log_dir/train.${iter}.log`
  wpc=`../utils/get_wpc.sh $log_dir/train.${iter}.log`
  wpc=`bc <<< "scale=1; $wpc / 1000"`k
  echo -n "TrEnt $(printf "%.4f" $tr_loss), "
  echo -n "lr($(printf "%.6g" $lr_rnn)/$(printf "%.6g" $lr_maxent)"
  echo -n "/$(printf "%.6g" $lr_lbl)/$(printf "%.6g" $lr_ffnn)), "
  echo -n "W/s $wpc, "
  
  connlm-test --log-file="$log_dir/valid.${iter}.log" \
             --config="$test_config" \
             "$dir/$mdl_next" "$valid_file" \
  || exit 1; 
  
  loss_new=`../utils/get_entropy.sh $log_dir/valid.${iter}.log`
  echo "CVEnt: $(printf "%.4f" $loss_new)"

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
  
  # no learn-rate halving yet, if keep_lr_iters set accordingly
  [ $iter -le $keep_lr_iters ] && continue 

  # stopping criterion
  rel_impr=$(bc <<< "scale=10; ($loss_prev-$loss)/$loss_prev")
  if [ 1 == $halving -a 1 == $(bc <<< "$rel_impr < $end_halving_impr") ]; then
    if [ $iter -le $min_iters ]; then
      echo we were supposed to finish, but we continue as min_iters : $min_iters
      continue
    fi
    echo finished, too small rel. improvement $rel_impr
    break
  fi

  # start annealing when improvement is low
  if [ 1 == $(bc <<< "$rel_impr < $start_halving_impr") ]; then
    halving=1
    echo $halving >$dir/.halving
  fi
  
  # do annealing
  if [ 1 == $halving ]; then
    lr_rnn=$(awk "BEGIN{print($lr_rnn*$halving_factor)}")
    lr_maxent=$(awk "BEGIN{print($lr_maxent*$halving_factor)}")
    lr_lbl=$(awk "BEGIN{print($lr_lbl*$halving_factor)}")
    lr_ffnn=$(awk "BEGIN{print($lr_ffnn*$halving_factor)}")
    echo $lr_rnn > $dir/.learn_rate
    echo $lr_maxent >> $dir/.learn_rate
    echo $lr_lbl >> $dir/.learn_rate
    echo $lr_ffnn >> $dir/.learn_rate
  fi
done

# select the best network
if [ $mdl_best != $mdl_init ]; then 
  ( cd $dir; ln -s $mdl_best final.clm; )
  echo "Succeeded training the Neural Network : $dir/final.clm"
else
  "Error training neural network..."
  exit 1
fi
exit 0;

