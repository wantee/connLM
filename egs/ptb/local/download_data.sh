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

archive=simple-examples.tgz
archive_size="34869662"
data_dir=./simple-examples/data

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

if ! tar -xvzf $data/$archive -C $data $data_dir; then
  echo "$0: error un-tarring archive $data/$archive"
  exit 1;
fi

touch $data/.complete

echo "$0: Successfully downloaded and un-tarred $data/$archive"

if $remove_archive; then
  echo "$0: removing $data/$archive file since --remove-archive option was supplied."
  rm $data/$archive
fi
