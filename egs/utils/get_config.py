#!/usr/bin/env python

from __future__ import print_function
import argparse
import subprocess
import os
import sys
import re

parser = argparse.ArgumentParser(description="This script parse config file"
    "and print the value of a given entry.",
    formatter_class=argparse.ArgumentDefaultsHelpFormatter)

parser.add_argument("config_file", type=str, help="config file.")
parser.add_argument("key", type=str, help="entry key.")

# echo command line to stderr for logging.

args = parser.parse_args()

confs = {}
with open(args.config_file, "r") as f:
  for line in f:
    line = line.strip()
    if line == '' or line[0] == '#':
      continue
    fields = re.split('[:| |\t|\n]+', line)
    assert len(fields) == 2
    assert fields[0] not in confs
    confs[fields[0]] = fields[1]

print(confs[args.key])
