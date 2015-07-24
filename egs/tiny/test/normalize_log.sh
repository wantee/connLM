#!/bin/bash

grep -E -v "Test Speed|Elapse time" | sed 's/W\/s [0-9.]*k//g'
