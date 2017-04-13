#!/bin/bash

rxfilename()
{
  filename=$1

  if [ "${filename##*.}" == "gz" ]; then
    echo "gunzip -c $filename |"
  elif [ "${filename##*.}" == "bz2" ]; then
    echo "bunzip2 -c $filename |"
  else
    echo $filename
  fi
}

wxfilename()
{
  filename=$1

  if [ "${filename##*.}" == "gz" ]; then
    echo "| gzip -c > $filename"
  elif [ "${filename##*.}" == "bz2" ]; then
    echo "| bunzip2 -c > $filename"
  else
    echo $filename
  fi
}
