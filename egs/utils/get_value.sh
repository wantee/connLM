#!/bin/bash

if [ $# -ne 2 ]; then
    echo "Usage: $0 <key> <log-file>"
    exit 1
fi

cat $2 | grep -o "$1: [-+0-9\.]*" | tail -n 1 | awk -F":" '{print $2}'

