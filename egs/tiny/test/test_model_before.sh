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
  echo "Usage: $0 <model> [mini|sync|mini+sync]"
  exit 1
fi

rm -rf "test/tmp/case-$SHU_CASE"
mkdir -p "test/tmp/case-$SHU_CASE"
mkdir -p "test/output/"
cp -r "conf" "test/tmp/case-$SHU_CASE"

model=$1

if [[ "$2" =~ "mini" ]]; then
  perl -i -pe 's/MINI_BATCH[[:space:]]*:[[:space:]]*0/MINI_BATCH : 1/' \
       "test/tmp/case-$SHU_CASE/conf/$model/train.conf"
fi

if [[ "$2" =~ "sync" ]]; then
  perl -i -pe 's/SYNC_SIZE[[:space:]]*:[[:space:]]*0/SYNC_SIZE : 1/' \
       "test/tmp/case-$SHU_CASE/conf/$model/train.conf"
fi
