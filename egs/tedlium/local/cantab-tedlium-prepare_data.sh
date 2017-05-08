#!/bin/bash
#
# Copyright  2014  Nickolay V. Shmyrev
#            2014  Brno University of Technology (Author: Karel Vesely)
#            2016  Johns Hopkins University (Author: Daniel Povey)
# Apache 2.0

# To be run from one directory above this script.

set -e
set -o pipefail

if [ $# -ne 3 ]; then
  echo "Usage: $0 <corpus_dir> <num_valid> <data_dir>"
  exit 1
fi

corpus=$1
num_valid=$2
data=$3

mkdir -p $data
data=`cd $data; pwd`

export LC_ALL=C

tail -n +$num_valid < $corpus/cantab-TEDLIUM/cantab-TEDLIUM.txt \
       | sed 's/ <\/s>//g' | gzip -c > $data/train.gz
head -n $num_valid < $corpus/cantab-TEDLIUM/cantab-TEDLIUM.txt \
       | sed 's/ <\/s>//g'  > $data/valid || exit 1
ln -sf $data/valid $data/test

awk '{print $1}' $corpus/cantab-TEDLIUM/cantab-TEDLIUM.dct | sort | uniq | \
      grep -v "<s>" > $data/wordlist || exit 1
