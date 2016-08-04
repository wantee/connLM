#!/bin/bash

set -e

source tools/shutils/shutils.sh

if [ $TRAVIS_BRANCH == 'master' ]; then
  if [ $CASE -eq 2 ]; then
    make -C src test
  fi

  if [ $CASE -eq 3 ]; then
    (cd egs/selection; shu-testing)
  fi

  ( cd egs/tiny; shu-testing $CASE )
elif [ $TRAVIS_BRANCH == 'graph' ]; then
  if [ $CASE -eq 1 ]; then
    echo "Testing miscs"
    make -C src test
    (cd egs/tiny; shu-testing 1-3)
  fi

  if [ $CASE -eq 2 ]; then
    echo "Testing MaxEnt related cases"
    (cd egs/tiny; shu-testing 4,9,12,14);
  fi

  if [ $CASE -eq 3 ]; then
    echo "Testing CBOW related cases"
    (cd egs/tiny; shu-testing 5,10,13,15);
  fi
fi
