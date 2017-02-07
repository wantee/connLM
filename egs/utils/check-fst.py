#!/usr/bin/env python

from __future__ import print_function
import argparse
import sys
import bisect
import numpy as np

parser = argparse.ArgumentParser(description="This script validates a FST "
    "generated by connlm-tofst"
    "  check-fst.py --fst-converter-log=exp/rnn/log/fst.converter.log "
    "--num-prob-sents=10 "
    "exp/rnn/words.txt exp/rnn/g.ssyms exp/rnn/g.txt",
    formatter_class=argparse.ArgumentDefaultsHelpFormatter)

parser.add_argument("--fst-converter-log", type=str, default="",
                    help="Log file written by connlm-tofst. "
                         "Check the logging informations, If specified.")
parser.add_argument("--num-prob-sents", type=int, default=0,
                    help="Number of random sentences to be sampled for "
                    "checking FST weight. Will not do this check "
                    "if it is set to be less than zero.")
parser.add_argument("word_syms", type=str, help="word symbols file.")
parser.add_argument("state_syms", type=str, help="state symbols file.")
parser.add_argument("fst_txt", type=str, help="FST file with ids for labels. "
                    "The FST must be sorted by state id.")

# echo command line to stderr for logging.
print(' '.join(sys.argv), file=sys.stderr)

args = parser.parse_args()

EPS = "<eps>"
BOS = "<s>"
EOS = "</s>"
ANY = "<any>"
PHI = "#phi"

ANY_ID = -100

print("  Loading word syms...")
vocab = {}
with open(args.word_syms, "r") as f:
  for line in f:
    if line.strip() == '':
      continue
    fields = line.split()
    assert len(fields) == 2
    assert fields[0] != "<any>"
    assert fields[0] not in vocab
    vocab[fields[0]] = int(fields[1])

vocab[ANY] = ANY_ID

print("  Loading state syms...")
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

class Arc:
  def __init__(self, source, to, ilab, olab, weight):
    self.source = source
    self.to = to
    self.ilab = ilab
    self.olab = olab
    self.weight = weight

    assert (self.ilab == vocab[PHI] and self.olab == vocab[EPS]) \
           or (self.ilab == self.olab)

  def __str__(self):
    return "%d -> %d (%d:%d/%f)" % (self.source, self.to, self.ilab,
                                    self.olab, self.weight)

class FST:
  def __init__(self):
    self.init_sid = -1
    self.final_sid = -1
    self.start_sid = -1
    self.any_sid = -1
    self.states = []

    self._ilab_sorted = False
    self._logp_cache = None

  def load(self, fst_file):
    with open(fst_file, "r") as f:
      for line in f:
        if line.strip() == '':
          continue

        fields = line.split()
        assert len(fields) == 1 or len(fields) == 5

        if len(fields) == 1:
          assert self.final_sid == -1
          self.final_sid = int(fields[0])
          continue

        arc = Arc(int(fields[0]), int(fields[1]), int(fields[2]),
                  int(fields[3]), float(fields[4]))
        if self.init_sid == -1:
          self.init_sid = arc.source
          self.start_sid = arc.to
        elif self.any_sid == -1: # the second arc must be from <any> state
          self.any_sid = arc.source

        while len(self.states) <= arc.source:
          self.states.append([])
        self.states[arc.source].append(arc)

    assert self.init_sid != -1
    assert self.start_sid != -1
    assert self.any_sid != -1
    assert self.final_sid != -1

    for state in self.states:
      # check no duplicated arcs
      labs = map(lambda arc: arc.ilab, state)
      assert len(set(labs)) == len(labs)

  def num_states(self):
    return len(self.states)

  def num_arcs(self):
    return reduce(lambda x, y: x + len(y), self.states, 0)

  def __str__(self):
    s = "Init: %d\n" % (self.init_sid)
    s += "Final: %d\n" % (self.final_sid)
    s += "Start: %d\n" % (self.start_sid)
    s += "Any: %d\n" % (self.any_sid)
    s += "#state: %d\n" % (self.num_states())
    s += "#arc: %d\n" % (self.num_arcs())

    return s

  def sort_ilab(self):
    if self._ilab_sorted:
      return

    for state in self.states:
      state.sort(key=lambda arc: arc.ilab)

    self._ilab_sorted = True

  def get_backoff_arc(self, sid):
    # the last arc must be backoff
    arc = self.states[sid][-1]
    assert arc.ilab == vocab[PHI]
    return arc

  def find_arc(self, sid, word):
    assert self._ilab_sorted == True

    arcs = self.states[sid]
    lo = 0
    hi = len(arcs)
    while lo < hi:
      mid = (lo+hi)//2
      if arcs[mid].ilab < word:
        lo = mid + 1
      elif arcs[mid].ilab > word:
        hi = mid
      else:
        return arcs[mid]

    return None

  def find_path(self, words, start_sid=-1):
    path = []
    if start_sid < 0:
      sid = self.init_sid
    else:
      sid = start_sid
    path.append(sid)

    for word in words:
      arc = self.find_arc(sid, word)
      if arc is None:
        return []
      else:
        path.append(arc.to)
      sid = arc.to

    return path

  def create_logp_cache(self):
    self._logp_cache = np.zeros((fst.num_states(), len(vocab)))

  def get_word_logp(self, sid, word):
    if not self._logp_cache is None and self._logp_cache[sid, word] != 0.0:
      return self._logp_cache[sid, word]

    arc = self.find_arc(sid, word)
    if not arc is None:
      logp = -arc.weight
    else:
      # backoff
      logp = 0.0
      backoff_arc = self.get_backoff_arc(sid)
      logp += -backoff_arc.weight
      logp += self.get_word_logp(backoff_arc.to, word)

    if not self._logp_cache is None:
      assert logp != 0.0
      self._logp_cache[sid, word] = logp

    return logp

  def reverse(self, drop_backoff=True):
    rfst = FST()

    rfst.init_sid = self.final_sid
    rfst.final_sid = self.init_sid
    rfst.start_sid = self.start_sid
    rfst.any_sid = self.any_sid

    rfst.states = [[] for i in xrange(len(self.states))]
    for state in self.states:
      for arc in state:
        if drop_backoff and arc.ilab == vocab[PHI]:
          continue
        rarc = Arc(arc.to, arc.source, arc.ilab, arc.olab, arc.weight)
        rfst.states[rarc.source].append(rarc)

    if drop_backoff:
      for sid, state in enumerate(rfst.states):
        if sid == rfst.final_sid or sid == rfst.any_sid:
          assert len(state) == 0
        elif sid != rfst.init_sid:
          # all state except init and final should have only one child
          assert len(state) == 1
      # init state, final state, <any> state, <s> state have no backoff arc
      assert rfst.num_arcs() == self.num_arcs() - self.num_states() + 4
    else:
      assert rfst.num_arcs() == self.num_arcs()

    # every state except (init, final) should have one </s> arcs
    assert len(rfst.states[rfst.init_sid]) == self.num_states() - 2

    return rfst

print("  Loading FST...")
fst = FST()
fst.load(args.fst_txt)
#print(fst)

print("  Checking ssyms...")
rfst = fst.reverse()
#print(rfst)
for sid, syms in enumerate(ssyms):
  if sid == rfst.init_sid:
    assert syms == [vocab[EOS]]
    continue

  rsyms = []
  s = sid
  while len(rfst.states[s]) > 0:
    arc = rfst.states[s][0]
    rsyms.append(arc.ilab)
    s = arc.to
  if s == rfst.any_sid:
    rsyms.append(ANY_ID)
    rsyms.append(vocab[BOS])
  rsyms.reverse()
  assert rsyms == syms

print("  Checking backoff arc...")
fst.sort_ilab()
for sid, state in enumerate(fst.states):
  if sid == fst.init_sid:
    assert len(state) == 1
    assert state[0].ilab == vocab[BOS]
    continue

  if sid == fst.start_sid or sid == fst.any_sid:
    # no backoff
    assert len(state) == len(vocab) - 4 # <eps>, <s>, #phi, <any>
    assert [ arc.ilab for arc in state ] == range(2, 2 + len(state))
    continue

  if sid == fst.final_sid:
    continue

  backoff_arc = fst.get_backoff_arc(sid)

  # 1. check the backoff state is the suffix of current state
  backoff_syms = ssyms[backoff_arc.to]
  assert len(backoff_syms) >= 2
  assert backoff_syms[0] == vocab[BOS] and backoff_syms[1] == ANY_ID
  if len(backoff_syms) > 2:
    backoff_syms[2:] == ssyms[sid][:-(len(backoff_syms)-2)]

  # 2. check the backoff state is the *longest* suffix of current state
  assert len(backoff_syms) <= len(ssyms[sid])
  if (ssyms[sid][1] == ANY_ID and len(backoff_syms) == len(ssyms[sid]) - 1) \
     or (ssyms[sid][1] != ANY_ID and len(backoff_syms) == len(ssyms[sid])):
    pass
  else:
    m = (len(backoff_syms) - 2) + 1
    n = m + 1
    if ssyms[sid][1] == ANY_ID:
      end = len(ssyms[sid]) - 1
    else:
      end = len(ssyms[sid])

    while n < end:
      longer_path = backoff_syms[:]
      for i in range(m, n):
        longer_path.insert(2, ssyms[sid][-i])

      assert fst.find_path(longer_path) == []
      n += 1

print("  Checking bows...")
fst.sort_ilab()
fst.create_logp_cache()
for (sid, state) in enumerate(fst.states):
  if sid == fst.init_sid:
    assert len(state) == 1
    assert state[0].ilab == vocab[BOS]
    assert state[0].weight == 0.0
    continue
  if sid == fst.final_sid:
    continue

  total_logp = -np.inf
  for word in vocab:
    if word == ANY or word == BOS or word == EPS or word == PHI:
      continue
    logp = fst.get_word_logp(sid, vocab[word])
    total_logp = np.logaddexp(total_logp, logp)

  assert np.isclose(total_logp, 0.0, atol=1e-5)

print("Finish to validate FST.", file=sys.stderr)
