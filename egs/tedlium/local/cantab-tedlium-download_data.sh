#!/bin/bash

url=http://cantabresearch.com/cantab-TEDLIUM.tar.bz2
remove_archive=false

if [ "$1" == --remove-archive ]; then
  remove_archive=true
  shift
fi

if [ $# -ne 1 ]; then
  echo "Usage: $0 [--remove-archive] <corpus_dir>"
  echo "With --remove-archive it will remove the archive after successfully un-tarring it."
  exit 1
fi

corpus=$1
mkdir -p $corpus

archive=cantab-TEDLIUM.tar.bz2
archive_size="1696090122"
data_dir=cantab-TEDLIUM/

if [ -f $corpus/.complete ]; then
  echo "$0: data was already successfully extracted, nothing to do."
  exit 0;
fi

if [ -f $corpus/$archive ]; then
  size=$(/bin/ls -l $corpus/$archive | awk '{print $5}')
  if [ $archive_size != $size ]; then
    echo "$0: removing existing file $corpus/$archive because its has wrong size"
    rm $corpus/$archive
  else
    echo "$corpus/$archive exists and appears to be complete."
  fi
fi

if [ ! -f $corpus/$archive ]; then
  if ! which wget >/dev/null; then
    echo "$0: wget is not installed."
    exit 1;
  fi
  echo "$0: downloading data from $url.  This may take some time, please be patient."

  if ! wget -P $corpus --no-check-certificate $url; then
    echo "$0: error executing wget $url"
    exit 1;
  fi
fi

if ! tar -xvf $corpus/$archive -C $corpus $data_dir; then
  echo "$0: error un-tarring archive $corpus/$archive"
  exit 1;
fi

touch $corpus/.complete

echo "$0: Successfully downloaded and un-tarred $corpus/$archive"

if $remove_archive; then
  echo "$0: removing $corpus/$archive file since --remove-archive option was supplied."
  rm $corpus/$archive
fi
