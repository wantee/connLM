#!/usr/bin/env python

from __future__ import print_function
import subprocess
import argparse
import os
import sys
import Queue

sys.path.append(os.path.abspath(os.path.dirname(sys.argv[0])))

import connlmfst as cf

parser = argparse.ArgumentParser(description="This script renumbers the "
    "states of a FST according to ilab and output the final state mapping.",
    formatter_class=argparse.ArgumentDefaultsHelpFormatter)

parser.add_argument("fst_txt", type=str, help="The FST file.")
parser.add_argument("state_map", type=str, help="The final state mapping.")

# echo command line to stderr for logging.
print(subprocess.list2cmdline(sys.argv), file=sys.stderr)

args = parser.parse_args()

print("  Loading FST...")
fst = cf.FST(None)
fst.load(args.fst_txt)
#print(fst)

print("  Mapping states...")
fst.sort_ilab()

sid_map = [-1] * fst.num_states()
new_sid = 0

q = Queue.Queue()
in_queue = [False] * fst.num_states()

sid = fst.init_sid
q.put(sid)
in_queue[sid] = True
while not q.empty():
  sid = q.get()
  if sid_map[sid] == -1:
    sid_map[sid] = new_sid
    new_sid += 1
  for arc in fst.states[sid]:
    if not in_queue[arc.to]:
      q.put(arc.to)
      in_queue[arc.to] = True

with open(args.state_map, "w") as f:
  for (sid, new_sid) in enumerate(sid_map):
    assert new_sid != -1
    print("%d\t%d" % (sid, new_sid), file=f)

print("Finish to renumber FST.", file=sys.stderr)
