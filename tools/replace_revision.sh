#!/bin/sh

if [ -e "$2" ]; then
rev_in=`grep "CONNLM_COMMIT" $2 | awk '{print $3}' | tr -d '"'`
else
rev_in=""
fi
rev=`git rev-parse HEAD`;

if [ -n "$3" ] || [ "$rev" != "$rev_in" ]; then
  mkdir -p `dirname $2`
  sed -E "s/#define[[:space:]]+CONNLM_COMMIT[[:space:]].*/#define CONNLM_COMMIT \"$rev\"/" $1 > $2
fi
