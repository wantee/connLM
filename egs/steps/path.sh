
CONNLM_BIN=$PWD/../../bin/float
CONNLM_LIB=$PWD/../../lib/float

if [ ! -z "$CONNLM_REALTYPE" ]; then
  if [ "$CONNLM_REALTYPE" == "double" ]; then
    CONNLM_BIN=$PWD/../../bin/double
    CONNLM_LIB=$PWD/../../lib/double
  elif [ "$CONNLM_REALTYPE" == "float" ]; then
    CONNLM_BIN=$PWD/../../bin/float
    CONNLM_LIB=$PWD/../../lib/float
  fi
fi

if [ ! -z "$1" ]; then
  if [ "$1" == "double" ]; then
    CONNLM_BIN=$PWD/../../bin/double
    CONNLM_LIB=$PWD/../../lib/double
  elif [ "$1" == "float" ]; then
    CONNLM_BIN=$PWD/../../bin/float
    CONNLM_LIB=$PWD/../../lib/float
  fi
fi

ST_UTILS_LIB=$PWD/../../tools/stutils/lib

export PATH=$CONNLM_BIN:$PWD:$PATH
if [ `uname` ==  "Darwin" ]; then
export DYLD_LIBRARY_PATH=$CONNLM_LIB:$ST_UTILS_LIB:$DYLD_LIBRARY_PATH
else
export LD_LIBRARY_PATH=$CONNLM_LIB:$ST_UTILS_LIB:$LD_LIBRARY_PATH
fi

export LC_ALL=C

USE_VAL=${USE_VAL:-0}
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

