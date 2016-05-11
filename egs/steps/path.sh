
EGS_DIR=$(cd `dirname $BASH_SOURCE`/..; pwd)

CONNLM_BIN=$EGS_DIR/../float/bin
CONNLM_LIB=$EGS_DIR/../float/lib

if [ ! -z "$CONNLM_REALTYPE" ]; then
  if [ "$CONNLM_REALTYPE" == "double" ]; then
    CONNLM_BIN=$EGS_DIR/../double/bin
    CONNLM_LIB=$EGS_DIR/../double/lib
  elif [ "$CONNLM_REALTYPE" == "float" ]; then
    CONNLM_BIN=$EGS_DIR/../float/bin
    CONNLM_LIB=$EGS_DIR/../float/lib
  fi
fi

if [ ! -z "$1" ]; then
  if [ "$1" == "double" ]; then
    CONNLM_BIN=$EGS_DIR/../double/bin
    CONNLM_LIB=$EGS_DIR/../double/lib
  elif [ "$1" == "float" ]; then
    CONNLM_BIN=$EGS_DIR/../float/bin
    CONNLM_LIB=$EGS_DIR/../float/lib
  fi
fi

ST_UTILS_LIB=$EGS_DIR/../tools/stutils/lib

export PATH=$CONNLM_BIN:$PATH
#if [ `uname` ==  "Darwin" ]; then
#export DYLD_LIBRARY_PATH=$CONNLM_LIB:$ST_UTILS_LIB:$DYLD_LIBRARY_PATH
#else
#export LD_LIBRARY_PATH=$CONNLM_LIB:$ST_UTILS_LIB:$LD_LIBRARY_PATH
#fi

export LC_ALL=C

USE_VAL=${USE_VAL:-0}
if [ "$USE_VAL" -eq 1 ]; then
  VAL_EXES=""
  for f in `ls $CONNLM_BIN/*`; do
    if [ -x $f ]; then
      VAL_EXES+=" ${f##*/}"
    fi
  done
  . $EGS_DIR/utils/valgrind.sh
fi

. $EGS_DIR/../tools/shutils/shutils.sh
