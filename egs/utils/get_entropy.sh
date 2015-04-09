#!/bin/bash

if [ $# -ne 1 ]; then
    echo "Usage: $0 <log-file>"
    exit 1
fi

cat $1 | grep -o 'Entropy: [-+0-9\.]*' | tail -n 1 | awk -F":" '{print $2}'

