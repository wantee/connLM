#!/usr/bin/env python

from __future__ import print_function
import subprocess
import argparse
import os
import sys
import random
import numpy as np

sys.path.append(os.path.abspath(os.path.dirname(sys.argv[0])))

import connlmfst as cf

parser = argparse.ArgumentParser(description="This script validates a FST "
    "generated by connlm-tofst.",
    formatter_class=argparse.ArgumentDefaultsHelpFormatter)

parser.add_argument("--sent-prob", type=str, default="",
                    help="Sentences probability file to be checked, the "
                    "prob file is typically output by connlm-eval")
parser.add_argument("--skip-bow-percent", type=float, default="0.0",
                    help="percentage of states to skip checking bow "
                    "(For speedup)")
parser.add_argument("word_syms", type=str, help="word symbols file.")
parser.add_argument("state_syms", type=str, help="state symbols file.")
parser.add_argument("fst_txt", type=str, help="FST file with ids for labels. "
                    "The FST must be sorted by state id.")

# echo command line to stderr for logging.
print(subprocess.list2cmdline(sys.argv), file=sys.stderr)

args = parser.parse_args()

print("  Loading word syms...", file=sys.stderr)
vocab = {}
with open(args.word_syms, "r") as f:
  for line in f:
    if line.strip() == '':
      continue
    fields = line.split()
    assert len(fields) == 2
    assert fields[0] != cf.WILDCARD
    assert fields[0] not in vocab
    vocab[fields[0]] = int(fields[1])
vocab[cf.WILDCARD] = cf.WILDCARD_ID

print("  Loading state syms...", file=sys.stderr)
ssyms = []
with open(args.state_syms, "r") as f:
  sid = 0
  for line in f:
    if line.strip() == '':
      continue

    fields = line.split()
    assert len(fields) <= 2
    assert int(fields[0]) == sid

    if len(fields) == 1:
      sid += 1
      ssyms.append([])
      continue

    ssyms.append(map(lambda x: int(vocab[x]), fields[1].split(":")))
    sid += 1

print("  Loading FST...", file=sys.stderr)
fst = cf.FST(vocab)
fst.load(args.fst_txt)
print(fst)

print("  Checking ssyms...", file=sys.stderr)
rfst = fst.reverse()
#print(rfst)
for sid, syms in enumerate(ssyms):
  if sid == rfst.init_sid:
    assert syms == [vocab[cf.EOS]]
    continue

  rsyms = []
  s = sid
  while len(rfst.states[s]) > 0:
    arc = rfst.states[s][0]
    rsyms.append(arc.ilab)
    s = arc.to
  if s == rfst.wildcard_sid:
    rsyms.append(cf.WILDCARD_ID)
  rsyms.reverse()
  assert rsyms == syms

print("  Checking backoff arc...", file=sys.stderr)
fst.sort_ilab()
for sid, state in enumerate(fst.states):
  if sid == fst.init_sid:
    assert len(state) == 1
    assert state[0].ilab == vocab[cf.BOS]
    continue

  if sid == fst.wildcard_sid:
    # no backoff
    assert len(state) == len(vocab) - 4 # <eps>, <s>, #phi, <wildcard>
    assert [ arc.ilab for arc in state ] == range(1, 1 + len(state))
    continue

  if sid == fst.final_sid:
    continue

  backoff_arc = fst.get_backoff_arc(sid)

  # 1. check the backoff state is the suffix of current state
  backoff_syms = ssyms[backoff_arc.to]
  assert len(backoff_syms) >= 1
  assert backoff_syms[0] == vocab[cf.WILDCARD]
  if len(backoff_syms) > 1:
    assert backoff_syms[1:] == ssyms[sid][-(len(backoff_syms)-1):]

  # 2. check the backoff state is the *longest* suffix of current state
  assert len(backoff_syms) <= len(ssyms[sid])
  if len(backoff_syms) == len(ssyms[sid]):
    pass
  else:
    m = (len(backoff_syms) - 1) + 1
    n = m + 1
    if ssyms[sid][0] == cf.WILDCARD_ID:
      end = len(ssyms[sid]) - 1
    else:
      end = len(ssyms[sid])

    while n < end:
      longer_path = backoff_syms[:]
      for i in range(m, n):
        longer_path.insert(1, ssyms[sid][-i])

      assert fst.find_path(longer_path) == []
      n += 1

if args.sent_prob != '':
  print("  Checking sentence probabilities...", file=sys.stderr)
  fst.create_logp_cache()

  sents = []
  with open(args.sent_prob, "r") as f:
    sent = [vocab[cf.BOS]]
    logprobs = [0.0]
    for line in f:
      line = line.strip()
      fields = line.split("\t")
      if len(fields) == 3:
        assert int(fields[0]) == vocab[fields[2]] - 1
        sent.append(vocab[fields[2]])
        logprobs.append(float(fields[1]))
        if fields[2] == cf.EOS:
          sents.append((sent, logprobs))
          sent = [vocab[cf.BOS]]
          logprobs = [0.0]

  for (sent, logprobs) in sents:
    path = fst.find_path(sent, partial=True)
    # </s> may not exist
    assert len(path) == len(sent) or len(path) + 1 == len(sent)
    for arc, logp in zip(path, logprobs):
      assert np.isclose(-arc.weight, logp)

print("  Checking bows...", file=sys.stderr)
fst.sort_ilab()
fst.create_logp_cache()
for (sid, state) in enumerate(fst.states):
  if sid == fst.init_sid:
    assert len(state) == 1
    assert state[0].ilab == vocab[cf.BOS]
    assert state[0].weight == 0.0
    continue
  if sid == fst.final_sid:
    continue

  if random.random() < args.skip_bow_percent:
    continue

  total_logp = -np.inf
  for word in vocab:
    if word in [cf.BOS, cf.EPS, cf.PHI, cf.WILDCARD]:
      continue
    logp = fst.get_word_logp(sid, vocab[word])
    total_logp = np.logaddexp(total_logp, logp)

  assert np.isclose(total_logp, 0.0, atol=1e-5) or print(sid, total_logp, file=sys.stderr)

print("Finish to validate FST.", file=sys.stderr)
