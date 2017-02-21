#!/bin/bash

if [ $# -lt 1 ]; then
    echo "Usage: $0 <key> <log-file>"
    exit 1
fi

sep="[:=]"
pattern="$1\s*$sep\s*[-+0-9\.]*|$1\s*$sep\s*[Tt][Rr][Uu][Ee]|$1\s*$sep\s*[Ff][Aa][Ll][Ss][e]|$1\s*$sep\s*[-+]?inf|$1\s*$sep\s*[-+]?nan"
if [ $# -ge 2 ]; then
    cat $2 | grep -E -o "$pattern" \
      | tail -n 1 | awk -F"$sep" '{print $2}' | sed -e 's/^\s*//' -e 's/\s*$//g'
else
    grep -E -o "$pattern" \
      | tail -n 1 | awk -F"$sep" '{print $2}' | sed -e 's/^\s*//' -e 's/\s*$//g'
fi
