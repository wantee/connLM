#!/bin/bash

# Copyright 2014  Vassil Panayotov 
#           2014  Johns Hopkins University (author: Daniel Povey)
# Apache 2.0

if [ "$#" -ne 2 ]; then
  echo "Usage: $0 <src-dir> <text>"
  echo "e.g.: $0 /export/a15/vpanayotov/data/LibriSpeech/dev-clean text"
  exit 1
fi

src=$1
trans=$2

mkdir -p `basename $trans`

[ ! -d $src ] && echo "$0: no such directory $src" && exit 1;

for reader_dir in $(find $src -mindepth 1 -maxdepth 1 -type d | sort); do
  reader=$(basename $reader_dir)
  if ! [ $reader -eq $reader ]; then  # not integer.
    echo "$0: unexpected subdirectory name $reader"
    exit 1;
  fi

  for chapter_dir in $(find -L $reader_dir/ -mindepth 1 -maxdepth 1 -type d | sort); do
    chapter=$(basename $chapter_dir)
    if ! [ "$chapter" -eq "$chapter" ]; then
      echo "$0: unexpected chapter-subdirectory name $chapter"
      exit 1;
    fi

    chapter_trans=$chapter_dir/${reader}-${chapter}.trans.txt
    [ ! -f  $chapter_trans ] && echo "$0: expected file $chapter_trans to exist" && exit 1
    cat $chapter_trans | cut -d' ' -f2-  >>$trans

  done
done

echo "$0: successfully prepared data"

exit 0
