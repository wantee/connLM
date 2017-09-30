#!/bin/bash

BLASINC=""
BLASLIB=""

if [ "`basename $PWD`" != "src" ]; then
  echo 'You must run "autogen.sh" from the src/ directory.'
  exit 1
fi

(cd .. && git submodule update --init) || exit 1

TOOL_DIR="$PWD/../tools/"
ST_UTILS_ROOT="$TOOL_DIR/stutils"
( cd "$ST_UTILS_ROOT/src" && make ) || exit 1

mkfile=blas.mk
> $mkfile
if [ "`uname`" == "Linux" ]; then
  if echo -e '#include <mkl.h>\nint main(){MKLVersion v; mkl_get_version(&v); return 0;}' | gcc -lmkl_gf_lp64 -lmkl_sequential -lmkl_core -lmkl_sequential -lmkl_core -xc - -lm -lpthread -o /dev/null >/dev/null 2>&1; then
    echo "Using MKL for blas"
    echo "CFLAGS += -D_USE_BLAS_" >> $mkfile
    echo "CFLAGS += -D_HAVE_MKL_" >> $mkfile
    echo "LDFLAGS += -lmkl_gf_lp64 -lmkl_sequential -lmkl_core -lmkl_sequential -lmkl_core" >> $mkfile
  else
    if [ -z "$BLASLIB" ]; then
      lib=`/sbin/ldconfig -p | grep 'libopenblas.so' | head -n 1 | cut -d'>' -f2`
      if [ -n "$lib" ]; then
        libdir=$(dirname "$lib" 2>/dev/null)
        BLASINC="-I /usr/include/openblas/"
        BLASLIB="-L $libdir -lopenblas"
        if echo -e '#include <stdio.h>\n#include <cblas.h>\nint main(){printf("%s\\n", openblas_get_config()); return 0;}' | gcc $BLASINC $BLASLIB -xc - -o /dev/null; then
          BLASNAME=OpenBLAS
        fi
      else
        # atlas
        lib=`/sbin/ldconfig -p | grep 'libatlas.so' | head -n 1 | cut -d'>' -f2`
        if [ -n "$lib" ]; then
          libdir=$(dirname "$lib" 2>/dev/null)
          BLASLIB="-L $libdir -lcblas -latlas -llapack"
        else
          lib=`/sbin/ldconfig -p | grep 'libsatlas.so' | head -n 1 | cut -d'>' -f2`
          if [ -n "$lib" ]; then
            libdir=$(dirname "$lib" 2>/dev/null)
            BLASLIB="-L $libdir -lsatlas"
          fi
        fi
        if [ -n "$BLASLIB" ]; then
          if echo -e '#include <cblas.h>\nvoid ATL_buildinfo(void);\nint main(){ATL_buildinfo(); return 0;}' | gcc $BLASINC $BLASLIB -xc - -o /dev/null; then
            BLASNAME=ATLAS
          fi
        fi
      fi
    else
      BLASNAME=CUSTOM
    fi
    if [ -n "$BLASNAME" ]; then
        echo "Using $BLASNAME for blas"
        echo "CFLAGS += -D_USE_BLAS_" >> $mkfile
        echo "CFLAGS += -D_HAVE_ATLAS_" >> $mkfile
        echo "CFLAGS += $BLASINC" >> $mkfile
        echo "LDFLAGS += $BLASLIB" >> $mkfile
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
