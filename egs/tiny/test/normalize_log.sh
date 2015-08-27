#!/bin/bash

grep -E -v "Test Speed|Elapse time|Effective speed" \
       | sed 's/W\/s [0-9.]*k//g' \
       | perl -pe 's/Speed:\s*[0-9.]*k?//g'
