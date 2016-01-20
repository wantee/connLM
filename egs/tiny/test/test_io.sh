#!/bin/bash

source ../steps/path.sh

if [ "`basename $PWD`" != "tiny" ]; then
  echo 'You must run "$0" from the tiny/ directory.'
  exit 1
fi

train_file=${1:-./data/train}
dir=${2:-./test/tmp/}
mkdir -p $dir

shu-run connlm-vocab $train_file $dir/vocab.clm || exit 1

for cls in 0 100; do
  for hs in true false; do
    for cls_hs in true false; do
      for rnn in 0 1; do
        for lbl in 0 1; do
          for maxent in 0 1; do
            for ffnn in 0 1; do
              shu-run connlm-output --class-size=$cls \
                                    --hs=$hs \
                                    --class-hs=$cls_hs \
                                    $dir/vocab.clm $dir/output.clm \
              || exit 1

              shu-run connlm-init --rnn^scale=$rnn \
                                  --lbl^scale=$lbl \
                                  --maxent^scale=$maxent \
                                  --ffnn^scale=$ffnn \
                                  $dir/output.clm $dir/init.clm
              ret=$?
              if [ `expr $rnn + $lbl + $maxent + $ffnn` -eq 0 ] \
                 || [ $maxent -ne 0 -a $hs == true ]; then
                if [ $ret -eq 0 ]; then
                  exit 1
                fi
                continue
              fi
              if [ $ret -ne 0 ]; then
                exit 1
              fi

              info=`shu-run connlm-info $dir/init.clm` || exit 1
              t=`echo "$info" | grep "Class size" \
                  | awk -F':' '{gsub (" ", "", $2); print $2}'`
              if [ "$t" -ne "$cls" ]; then
                shu-err "Class size not match"
                exit 1
              fi

              t=`echo "$info" | grep "^HS" \
                  | awk -F':' '{gsub (" ", "", $2); print $2}'`
              if [ "$t" != "$hs" ]; then
                shu-err "HS not match"
                exit 1
              fi

              t=`echo "$info" | grep "Class HS" \
                  | awk -F':' '{gsub (" ", "", $2); print $2}'`
              if [ $hs == true ]; then
                if [ "$t" != "$cls_hs" ]; then
                  shu-err "$t $hs $cls_hs Class HS not match"
                  exit 1
                fi
              else
                if [ -n "$t" ]; then
                  shu-err "$t Class HS not match"
                  exit 1
                fi
              fi

              t=`echo "$info" | grep "^<RNN>" \
                  | awk -F':' '{gsub (" ", "", $2); print $2}'`
              if [ 1 -eq "$rnn" ]; then
                if [ "$t" -ne "$rnn" ]; then
                  shu-err "RNN not match"
                  exit 1
                fi
              else
                if [ "$t" != "None" ]; then
                  shu-err "RNN not match"
                  exit 1
                fi
              fi

              t=`echo "$info" | grep "^<MAXENT>" \
                  | awk -F':' '{gsub (" ", "", $2); print $2}'`
              if [ 1 -eq "$maxent" ]; then
                if [ "$t" -ne "$maxent" ]; then
                  shu-err "MAXENT not match"
                  exit 1
                fi
              else
                if [ "$t" != "None" ]; then
                  shu-err "MAXENT not match"
                  exit 1
                fi
              fi

              t=`echo "$info" | grep "^<LBL>" \
                  | awk -F':' '{gsub (" ", "", $2); print $2}'`
              if [ 1 -eq "$lbl" ]; then
                if [ "$t" -ne "$lbl" ]; then
                  shu-err "LBL not match"
                  exit 1
                fi
              else
                if [ "$t" != "None" ]; then
                  shu-err "LBL not match"
                  exit 1
                fi
              fi

              t=`echo "$info" | grep "^<FFNN>" \
                  | awk -F':' '{gsub (" ", "", $2); print $2}'`
              if [ 1 -eq "$ffnn" ]; then
                if [ "$t" -ne "$ffnn" ]; then
                  shu-err "FFNN not match"
                  exit 1
                fi
              else
                if [ "$t" != "None" ]; then
                  shu-err "FFNN not match"
                  exit 1
                fi
              fi

              # Testing connlm-copy
              shu-run connlm-copy --binary=true \
                                  $dir/init.clm $dir/bin.clm \
              || exit 1

              shu-run connlm-copy --binary=false \
                                  $dir/bin.clm $dir/txt.clm \
              || exit 1

              shu-run connlm-copy --binary=false \
                                  $dir/txt.clm $dir/txt1.clm \
              || exit 1

              shu-run connlm-copy --binary=true \
                                  $dir/txt.clm $dir/bin1.clm \
              || exit 1

              diff $dir/txt.clm $dir/txt1.clm || exit
            done
          done
        done
      done
    done
  done
done

echo "All Passed."
exit 0
