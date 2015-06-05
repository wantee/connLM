ST_UTILS_LIB=$PWD/../../tools/stutils/lib
CONNLM_BIN=$PWD/../../bin
CONNLM_LIB=$PWD/../../lib

export PATH=$CONNLM_BIN:$PWD:$PATH
if [ `uname` ==  "Darwin" ]; then
export DYLD_LIBRARY_PATH=$CONNLM_LIB:$ST_UTILS_LIB:$DYLD_LIBRARY_PATH
else
export LD_LIBRARY_PATH=$CONNLM_LIB:$ST_UTILS_LIB:$LD_LIBRARY_PATH
fi

export LC_ALL=C

USE_VAL=0
if [ "$USE_VAL" -eq 1 ]; then
  VAL_EXES=""
  for f in `ls $CONNLM_BIN/*`; do
    if [ -x $f ]; then
      VAL_EXES+=" ${f##*/}"
    fi
  done
  . ../utils/valgrind.sh
fi

. $PWD/../../tools/shutils/shutils.sh

