#!/bin/bash

grep -E -v "Test Speed|Elapse time|Effective speed" \
       | sed 's/W\/s [0-9.]*k//g' \
       | sed 's/case-[0-9]*/case-n/g' \
       | perl -pe 's/Speed:\s*[0-9.]*k?//g'
