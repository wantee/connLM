#!/bin/bash

ATLASINC=""
ATLASLIB=""

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
  cd -
fi

SH_UTILS_ROOT=$PWD/../tools/shutils
if [ ! -e "$SH_UTILS_ROOT/shutils.sh" ]; then
  git clone https://github.com/wantee/shutils $SH_UTILS_ROOT
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
    ATLASLIB=$(dirname `ldconfig -p | grep libatlas.so | cut -d'>' -f2` 2>/dev/null)
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

