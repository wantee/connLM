#!/bin/bash

set -e

if [ "`basename $PWD`" != "tiny" ]; then
  echo 'You must run "$0" from the tiny/ directory.'
  exit 1
fi

if [ -z "$SHU_CASE"  ]; then
  echo '$SHU_CASE not defined'
  exit 1
fi

source ../steps/path.sh

_case_dir="test/tmp/case-$SHU_CASE/"

connlm-init --log-file="${_case_dir}/exp/log/complex.init.log" \
            "${_case_dir}/exp/rnn/output.clm" \
            "conf/rnn/complex.topo" \
            "${_case_dir}/exp/rnn/complex.init.clm"

connlm-draw --verbose=true --log-file="${_case_dir}/exp/log/complex.draw.log" \
            "mdl,-o:${_case_dir}/exp/rnn/complex.init.clm" \
            "test/output/case-$SHU_CASE.rnn.dot"
