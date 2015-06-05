#!/bin/bash

grep -v "Test Speed" | sed 's/W\/s [0-9.]*k//g'
