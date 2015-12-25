ifndef CONNLM_NO_OPT
  ifeq ($(shell echo 'int main(){return 0;}' | $(CC) -xc - -Ofast -o /dev/null >/dev/null 2>&1 ; echo $$?),0)
  CFLAGS += -Ofast
  else
  CFLAGS += -O3
  endif
  CFLAGS += -march=native -funroll-loops
  ifneq ($(CC),icc)
    CFLAGS += -ffast-math
  endif
  include ./blas.mk
else
  CFLAGS += -O0
endif

ifeq ($(shell uname -s),Darwin)
LDFLAGS+=
else
LDFLAGS+=-lm
endif

CFLAGS += -I. -I../tools/stutils/include/stutils/ -I$(OUTINC_DIR)
LDFLAGS += -L../tools/stutils/lib/
LDFLAGS += -lstutils -lpthread
LDFLAGS += -Wl,-rpath,$(abspath ../tools/stutils/lib/)

CFLAGS += -g -Wall -Winline -pipe
CFLAGS += -DNDEBUG
#CFLAGS += -D_TIME_PROF_

