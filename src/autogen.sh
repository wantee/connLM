#!/bin/bash

ATLASINC=""
ATLASLIB=""

if [ "`basename $PWD`" != "src" ]; then
  echo 'You must run "autogen.sh" from the src/ directory.'
  exit 1
fi

$PWD/../misc/git-hooks/create-hook-symlinks || exit 1

TOOL_DIR="$PWD/../tools/"
REV_FILE=.git-revs
ST_UTILS_ROOT="$TOOL_DIR/stutils"
if [ ! -e "$ST_UTILS_ROOT/include/stutils/st_macro.h" ]; then
  git clone https://github.com/wantee/stutils.git $ST_UTILS_ROOT || exit 1
else
  ( cd "$ST_UTILS_ROOT" && git checkout master && git pull ) || exit 1
fi
if [ -e "$TOOL_DIR/$REV_FILE" ]; then
  commit=`grep stutils "$TOOL_DIR/$REV_FILE" | awk '{print $2}'`
  (cd "$ST_UTILS_ROOT" && git rev-parse --verify connlm > /dev/null && git branch -d connlm)
  (cd "$ST_UTILS_ROOT" && git checkout -b connlm $commit) || exit 1
fi
( cd "$ST_UTILS_ROOT/src" && make ) || exit 1

SH_UTILS_ROOT="$TOOL_DIR/shutils"
if [ ! -e "$SH_UTILS_ROOT/shutils.sh" ]; then
  git clone https://github.com/wantee/shutils $SH_UTILS_ROOT || exit 1
else
  ( cd "$SH_UTILS_ROOT" && git checkout master && git pull ) || exit 1
fi
if [ -e "$TOOL_DIR/$REV_FILE" ]; then
  commit=`grep shutils "$TOOL_DIR/$REV_FILE" | awk '{print $2}'`
  (cd "$SH_UTILS_ROOT" && git rev-parse --verify connlm > /dev/null && git branch -d connlm)
  (cd "$SH_UTILS_ROOT" && git checkout -b connlm $commit) || exit 1
fi

mkfile=blas.mk
> $mkfile
if [ "`uname`" == "Linux" ]; then
  if echo -e '#include <mkl.h>\nint main(){MKLVersion v; mkl_get_version(&v); return 0;}' | gcc -lmkl_gf_lp64 -lmkl_sequential -lmkl_core -lmkl_sequential -lmkl_core -xc - -lm -lpthread -o /dev/null >/dev/null 2>&1; then
    echo "Using MKL for blas"
    echo "CFLAGS += -D_USE_BLAS_" >> $mkfile
    echo "CFLAGS += -D_HAVE_MKL_" >> $mkfile
    echo "LDFLAGS += -lmkl_gf_lp64 -lmkl_sequential -lmkl_core -lmkl_sequential -lmkl_core" >> $mkfile
  else
    ATLASLIB=$(dirname "`ldconfig -p | grep 'libatlas.so$' | cut -d'>' -f2`" 2>/dev/null)
    if [ -n "$ATLASLIB" ]; then
      FLAG="-L $ATLASLIB -lcblas -latlas -llapack"
      if [ -n "$ATLASINC" ]; then
        FLAG=" -I $ATLASINC"
      fi
      if echo -e '#include <cblas.h>\nint main(){ATL_buildinfo(); return 0;}' | gcc $FLAG -xc - -o /dev/null > /dev/null 2>&1; then
        echo "Using ATLAS for blas"
        echo "CFLAGS += -D_USE_BLAS_" >> $mkfile
        echo "CFLAGS += -D_HAVE_ATLAS_" >> $mkfile
        echo "LDFLAGS += $FLAG" >> $mkfile
      fi
    fi
  fi

elif [ "`uname`" == "Darwin" ]; then
  if [ -e /System/Library/Frameworks/Accelerate.framework ]; then
    echo "Using Accelerate framework for blas"
    echo "CFLAGS += -D_USE_BLAS_" >> $mkfile
    echo "CFLAGS += -D_HAVE_ACCELERATE_" >> $mkfile
    echo "LDFLAGS += -framework Accelerate" >> $mkfile
  fi
fi

echo "Finish autogen"
exit 0

