#!/bin/bash

set -e

source tools/shutils/shutils.sh

if [ $TRAVIS_BRANCH == 'master' ] || [ $TRAVIS_BRANCH == 'develop' ]; then
  if [ $CASE -eq 1 ]; then
    echo "Testing miscs"
    make -C src test
    (cd egs/tiny; shu-testing 1-2)
    (cd egs/selection; shu-testing)
  fi

  if [ $CASE -eq 2 ]; then
    echo "Testing MaxEnt related cases"
    (cd egs/tiny; shu-testing 3-4);
  fi

  if [ $CASE -eq 3 ]; then
    echo "Testing CBOW related cases"
    (cd egs/tiny; shu-testing 5-6);
  fi

  if [ $CASE -eq 4 ]; then
    echo "Testing FFNN related cases"
    (cd egs/tiny; shu-testing 7-8);
  fi

  if [ $CASE -eq 5 ]; then
    echo "Testing RNN related cases"
    (cd egs/tiny; shu-testing 9-10);
  fi

  if [ $CASE -eq 6 ]; then
    echo "Testing crossing-RNN related cases"
    (cd egs/tiny; shu-testing 11-12);
  fi

  if [ $CASE -eq 7 ]; then
    echo "Testing RNN+MaxEnt related cases"
    (cd egs/tiny; shu-testing 13-14);
  fi

  if [ $CASE -eq 8 ]; then
    echo "Testing MaxEnt~RNN related cases"
    (cd egs/tiny; shu-testing 15-16);
  fi

elif [ $TRAVIS_BRANCH == 'legacy' ]; then
  if [ $CASE -eq 2 ]; then
    make -C src test
  fi

  if [ $CASE -eq 3 ]; then
    (cd egs/selection; shu-testing)
  fi

  ( cd egs/tiny; shu-testing $CASE )
fi
