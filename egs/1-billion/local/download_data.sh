#!/bin/bash

remove_archive=false

if [ "$1" == --remove-archive ]; then
  remove_archive=true
  shift
fi

if [ $# -ne 2 ]; then
  echo "Usage: $0 [--remove-archive] <url> <data>"
  echo "With --remove-archive it will remove the archive after successfully un-tarring it."
  exit 1
fi

url=$1
data=$2

if [ -z "$url" ]; then
  echo "$0: empty URL."
  exit 1;
fi

if [ -z "$data" ]; then
  echo "$0: empty data."
  exit 1;
fi
mkdir -p $data

archive=training-monolingual.tgz
archive_size="10582262657"

if [ -f $data/.complete ]; then
  echo "$0: data was already successfully extracted, nothing to do."
  exit 0;
fi

if [ -f $data/$archive ]; then
  size=$(/bin/ls -l $data/$archive | awk '{print $5}')
  if [ $archive_size != $size ]; then
    echo "$0: removing existing file $data/$archive because its has wrong size"
    rm $data/$archive
  else
    echo "$data/$archive exists and appears to be complete."
  fi
fi

if [ ! -f $data/$archive ]; then
  if ! which wget >/dev/null; then
    echo "$0: wget is not installed."
    exit 1;
  fi
  echo "$0: downloading data from $url.  This may take some time, please be patient."

  if ! wget -P $data --no-check-certificate $url; then
    echo "$0: error executing wget $url"
    exit 1;
  fi
fi

echo "$0: Un-tarring data."
tar -xvf $data/$archive -C $data \
    --wildcards training-monolingual/news.20??.en.shuffled
if [ $? -ne 0 ]; then
    echo "$0: error un-tarring data."
    exit 1;
fi

echo "$0: Generating corpus from data."
(
  if [ ! -d 1-billion-word-language-modeling-benchmark ]; then \
    git clone https://github.com/ciprian-chelba/1-billion-word-language-modeling-benchmark.git; \
  fi && \
  cd $data/1-billion-word-language-modeling-benchmark && \
  ln -sf $data/$archive tar_archives/$archive && \
  TMPDIR=$data ./scripts/get-data.sh \
  && sed 's/  / /g' README.corpus_generation_checkpoints > checkpoints \
  && md5sum -c checkpoints
)
if [ $? -ne 0 ]; then
    echo "$0: error generating corpus."
    exit 1;
fi

touch $data/.complete

echo "$0: Successfully downloaded and generated corpus."

if $remove_archive; then
  echo "$0: removing $data/$archive file since --remove-archive option was supplied."
  rm $data/$archive
fi
