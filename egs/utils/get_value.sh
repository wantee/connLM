#!/bin/bash

if [ $# -lt 1 ]; then
    echo "Usage: $0 <key> <log-file>"
    exit 1
fi

pattern="$1: [-+0-9\.]*|$1: [Tt][Rr][Uu][Ee]|$1: [Ff][Aa][Ll][Ss][e]|$1: [-+]?inf|$1: [-+]?nan"
if [ $# -ge 2 ]; then
    cat $2 | grep -E -o "$pattern" \
      | tail -n 1 | awk -F":" '{print $2}'
else 
    grep -E -o "$pattern" \
      | tail -n 1 | awk -F":" '{print $2}'
fi

