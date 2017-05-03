#!/usr/bin/env python

from __future__ import print_function
import subprocess
import argparse
import os
import sys
import math

path = os.path.abspath(os.path.dirname(sys.argv[0]))
sys.path.append(path)
sys.path.append(os.path.join(path, os.pardir))

from connlm_common import *
import connlmfst as cf

parser = argparse.ArgumentParser(description="This script convert a "
    "FST into ARPA format.",
    formatter_class=argparse.ArgumentDefaultsHelpFormatter)

parser.add_argument("word_syms", type=str, help="The word symbol table file.")
parser.add_argument("fst_file", type=str, help="The input FST file.")
parser.add_argument("arpa_file", type=str, help="The output ARPA file.")

# echo command line to stderr for logging.
log(subprocess.list2cmdline(sys.argv))

args = parser.parse_args()

def ngram(id2word, words):
  s = ""
  for w in words:
    s += id2word[w] + " "
  return s.strip()

LN_10 = math.log(10)
inf = float("inf")
def weight2str(weight):
  if weight == inf:
    return "-99"
  else:
    return "%f" % (-weight / LN_10)

log("Converting FST to ARPA...")
log("  Loading FST...")
vocab = {}
with open_file(args.word_syms, "r") as f:
  for line in f:
    if line.strip() == '':
      continue
    fields = line.split()
    assert len(fields) == 2
    assert fields[0] != cf.WILDCARD
    assert fields[0] not in vocab
    vocab[fields[0]] = int(fields[1])

id2word = [""] * len(vocab)
for w in vocab:
  id2word[vocab[w]] = w

fst = cf.FST(vocab)
fst.load(args.fst_file)

with open_file(args.arpa_file, "w") as f_arpa:
  ssyms = [[] for _ in xrange(fst.num_states())]
  ssyms[fst.start_sid] = [vocab[cf.BOS]]

  ngram_cnts = []
  order = 0
  bos_rng = [-1, -1]
  wildcard_rng = [fst.wildcard_sid, fst.wildcard_sid + 1]

  while bos_rng[0] < bos_rng[1] or wildcard_rng[0] < wildcard_rng[1]:
    order += 1
    log("Printing %d-gram..." % order)
    cnt = 0
    print("\n\\%d-grams:" % order, file=f_arpa)
    if order == 1:
      print("%s\t%s\t%s" % (weight2str(inf),
                            cf.BOS,
                            weight2str(fst.bow(fst.start_sid))),
            file=f_arpa)
      cnt += 1

    sids = xrange(bos_rng[0], bos_rng[1])
    end = bos_rng[1] - 1
    bos_rng = [sys.maxint, -1]
    for sid in sids:
      for arc in fst.states[sid]:
        if fst.is_backoff_arc(arc):
          continue

        syms = ssyms[arc.source] + [arc.ilab]
        if arc.to > end: # non-highest order and not </s>
          ssyms[arc.to] = syms
          if arc.to < bos_rng[0]:
            bos_rng[0] = arc.to
          if arc.to + 1 > bos_rng[1]:
            bos_rng[1] = arc.to + 1

          print("%s\t%s\t%s" % (weight2str(arc.weight),
                                ngram(id2word, syms),
                                weight2str(fst.bow(arc.to))),
                file=f_arpa)
        else:
          print("%s\t%s" % (weight2str(arc.weight), ngram(id2word, syms)),
                file=f_arpa)

        cnt += 1

    sids = xrange(wildcard_rng[0], wildcard_rng[1])
    end = wildcard_rng[1] - 1
    wildcard_rng = [sys.maxint, -1]
    for sid in sids:
      for arc in fst.states[sid]:
        if fst.is_backoff_arc(arc):
          continue

        syms = ssyms[arc.source] + [arc.ilab]
        if arc.to > end: # non-highest order and not </s>
          ssyms[arc.to] = syms
          if arc.to < wildcard_rng[0]:
            wildcard_rng[0] = arc.to
          if arc.to + 1 > wildcard_rng[1]:
            wildcard_rng[1] = arc.to + 1
          print("%s\t%s\t%s" % (weight2str(arc.weight),
                                ngram(id2word, syms),
                                weight2str(fst.bow(arc.to))),
                file=f_arpa)
        else:
          print("%s\t%s" % (weight2str(arc.weight), ngram(id2word, syms)),
                file=f_arpa)

        cnt += 1

    ngram_cnts.append(cnt)
    log("  %d grams Done." % cnt)

    if order == 1:
       bos_rng = [fst.start_sid, fst.start_sid + 1]
  print("\n\\end\\", file=f_arpa)

if args.arpa_file.endswith('.gz'):
  header_file = args.arpa_file[:-3] + ".header.gz"
else:
  header_file = args.arpa_file + ".header"
with open_file(header_file, "w") as f:
  print("\\data\\", file=f)
  for o in range(order):
    print("ngram %d=%d" % (o + 1, ngram_cnts[o]), file=f)

log("Finish to convert FST.")
