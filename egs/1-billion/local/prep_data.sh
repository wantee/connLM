#!/bin/bash

if [ $# -lt 1 ]; then
  echo "Usage: $0 <corpus_dir>"
  exit 1
fi

dir=$1
dir=`cd $dir; pwd`

archive=training-monolingual.tgz
echo "$0: Generating corpus from data."
if [ ! -d $dir/1-billion-word-language-modeling-benchmark ]; then
  git clone https://github.com/ciprian-chelba/1-billion-word-language-modeling-benchmark.git $dir/1-billion-word-language-modeling-benchmark || exit 1
fi
(
  cd $dir/1-billion-word-language-modeling-benchmark && \
  ln -sf $dir/$archive tar_archives/$archive && \
  ln -sf $dir/training-monolingual . && \
  TMPDIR=$dir ./scripts/get-data.sh \
  && sed 's/  / /g' README.corpus_generation_checkpoints > checkpoints \
  && md5sum -c checkpoints
)
if [ $? -ne 0 ]; then
    echo "$0: error generating corpus."
    exit 1;
fi

echo "$0: Successfully generated corpus."
