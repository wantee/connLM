#!/bin/bash

export PATH=.:$PATH
export LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH

tmp_dir=$(mktemp -d)
mkfifo "$tmp_dir/in1" "$tmp_dir/in2"
mkfifo "$tmp_dir/out1" "$tmp_dir/out2"

connlm-eval --log-file=/dev/stderr --reader.epoch-size=1 --print-sent-prob=true --out-log-base=10 $1 "$tmp_dir/in1" "$tmp_dir/out1" &
pid1=$!
connlm-eval --log-file=/dev/stderr --reader.epoch-size=1 --print-sent-prob=true --out-log-base=10 $2 "$tmp_dir/in2" "$tmp_dir/out2" &
pid2=$!
tee "$tmp_dir/in1" "$tmp_dir/in2" | paste - "$tmp_dir/out1" "$tmp_dir/out2"

wait $pid1 $pid2
rm -rf "$tmp_dir"
