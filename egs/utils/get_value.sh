#!/bin/bash

if [ $# -lt 1 ]; then
    echo "Usage: $0 <key> <log-file>"
    exit 1
fi

pattern="$1:\s*[-+0-9\.]*|$1:\s*[Tt][Rr][Uu][Ee]|$1:\s*[Ff][Aa][Ll][Ss][e]|$1:\s*[-+]?inf|$1:\s*[-+]?nan"
if [ $# -ge 2 ]; then
    cat $2 | grep -E -o "$pattern" \
      | tail -n 1 | awk -F"[:]" '{print $2}' | sed 's/^\s*|\s*$//g'
else 
    grep -E -o "$pattern" \
      | tail -n 1 | awk -F"[:]" '{print $2}' | sed 's/^\s*|\s*$//g'
fi
