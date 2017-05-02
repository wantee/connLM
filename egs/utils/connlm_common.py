#!/usr/bin/env python

# we're using python 3.x style print but want it to work in python 2.x,
from __future__ import print_function
import os
import sys
import datetime
import gzip

__DEBUG__ = False

def log(msg, verbose=False, file=sys.stderr):
  exe = os.path.basename(sys.argv[0])
  if verbose:
    print("%s: [%s] %s" % (exe, datetime.now(), str(msg)), file=file)
  else:
    print("%s: %s" % (exe, str(msg)), file=file)

def logd(msg, verbose=False, file=sys.stderr):
  if __DEBUG__:
    log(msg, verbose=verbose, file=file)

def mkdir_p(path):
  if not os.path.isdir(path):
    os.makedirs(path)

def open_file(filename, mode):
  if filename.endswith('.gz'):
    return gzip.open(filename, mode)
  else:
    return open(filename, mode)
