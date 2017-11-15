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

if [ $# -lt 1 ]; then
  echo "Usage: $0 <model> [mini]"
  exit 1
fi

rm -rf "test/tmp/case-$SHU_CASE"
mkdir -p "test/tmp/case-$SHU_CASE"
mkdir -p "test/output/"
cp -r "conf" "test/tmp/case-$SHU_CASE"

model=$1

if [[ "$2" =~ "mini" ]]; then
  echo "[READER]" >> "test/tmp/case-$SHU_CASE/conf/$model/train.conf"
  echo "MINI_BATCH_SIZE : 10" >> "test/tmp/case-$SHU_CASE/conf/$model/train.conf"
fi
