#!/bin/bash

set -e

source tools/shutils/shutils.sh

if [ $TRAVIS_BRANCH == 'master' ] || [ $TRAVIS_BRANCH == 'develop' ]; then
  if [ $CASE -eq 1 ]; then
    echo "Testing miscs"
    make -C src test
    (cd egs/tiny; shu-testing 1-2)
  fi

  if [ $CASE -eq 2 ]; then
    echo "Testing MaxEnt related cases"
    (cd egs/tiny; shu-testing 3-6);
  fi

  if [ $CASE -eq 3 ]; then
    echo "Testing CBOW related cases"
    (cd egs/tiny; shu-testing 7-10);
  fi

  if [ $CASE -eq 4 ]; then
    echo "Testing FFNN related cases"
    (cd egs/tiny; shu-testing 11-14);
  fi

  if [ $CASE -eq 5 ]; then
    echo "Testing RNN related cases"
    (cd egs/tiny; shu-testing 15-18);
  fi

  if [ $CASE -eq 6 ]; then
    echo "Testing crossing-RNN related cases"
    (cd egs/tiny; shu-testing 19-22);
  fi

  if [ $CASE -eq 7 ]; then
    echo "Testing RNN+MaxEnt related cases"
    (cd egs/tiny; shu-testing 23-26);
  fi

  if [ $CASE -eq 8 ]; then
    echo "Testing MaxEnt~RNN related cases"
    (cd egs/tiny; shu-testing 27-30);
  fi

  if [ $CASE -eq 9 ]; then
    echo "Testing FST converter related cases"
    # install numpy on travis
    if [ "$CI" == "true" ] && [ "$TRAVIS" == "true" ]; then
      wget http://repo.continuum.io/miniconda/Miniconda2-latest-Linux-x86_64.sh -O miniconda.sh || exit 1
      chmod +x miniconda.sh && ./miniconda.sh -b -p /home/travis/mc || exit 1
      export PATH=/home/travis/mc/bin:$PATH
      conda install --yes numpy>=1.7.0 || exit 1
    fi
    (cd egs/tiny; shu-testing 31);
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
