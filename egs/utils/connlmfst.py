#!/usr/bin/env python

from __future__ import print_function
import numpy as np

EPS = "<eps>"
BOS = "<s>"
EOS = "</s>"
ANY = "<any>"
PHI = "#phi"

ANY_ID = -100

class Arc:
  def __init__(self, source, to, ilab, olab, weight):
    self.source = source
    self.to = to
    self.ilab = ilab
    self.olab = olab
    self.weight = weight

  def __str__(self):
    return "%d -> %d (%d:%d/%f)" % (self.source, self.to, self.ilab,
                                    self.olab, self.weight)

class FST:
  def __init__(self, vocab):
    self.init_sid = -1
    self.final_sid = -1
    self.start_sid = -1
    self.any_sid = -1
    self.states = []

    self._vocab = vocab
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
        if not self._vocab is None:
          assert (arc.ilab == arc.olab) or \
               (arc.ilab == self._vocab[PHI] and arc.olab == self._vocab[EPS])

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
    assert arc.ilab == self._vocab[PHI]
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

    for word in words:
      arc = self.find_arc(sid, word)
      if arc is None:
        return []
      else:
        path.append(arc)
      sid = arc.to

    return path

  def create_logp_cache(self):
    if self._logp_cache is None:
      self._logp_cache = np.zeros((self.num_states(), len(self._vocab)))

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
    rfst = FST(self._vocab)

    rfst.init_sid = self.final_sid
    rfst.final_sid = self.init_sid
    rfst.start_sid = self.start_sid
    rfst.any_sid = self.any_sid

    rfst.states = [[] for i in xrange(len(self.states))]
    for state in self.states:
      for arc in state:
        if drop_backoff and arc.ilab == self._vocab[PHI]:
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
