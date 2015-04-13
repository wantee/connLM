#!/bin/sh

set -e

if [ "`basename $PWD`" != "src" ]; then
  echo 'You must run "autogen.sh" from the src/ directory.'
  exit 1
fi

ST_UTILS_ROOT=$PWD/../tools/stutils
if [ ! -e "$ST_UTILS_ROOT/include/stutils/st_macro.h" ]; then
  git clone https://github.com/wantee/stutils.git $ST_UTILS_ROOT
  cd $ST_UTILS_ROOT/src
  make
fi

echo "Finish autogen"
exit 0

