GCC = gcc
 
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
LDFLAGS+=
else
LDFLAGS+=-lm
endif

CFLAGS += -I../tools/stutils/include/stutils/
LDFLAGS += -L../tools/stutils/lib/
LDFLAGS += -lstutils -lpthread
LDFLAGS += -Wl,-rpath '-Wl,$$ORIGIN'

real_type=float
CFLAGS += -D REAL_TYPE=$(real_type)
ifeq ($(real_type), double)
CFLAGS += -D REAL_FMT="\"%lf\""
else
CFLAGS += -D REAL_FMT="\"%f\""
endif

CFLAGS += -g -Wall -Winline -pipe 

ifndef CONNLM_NO_OPT 
  ifeq ($(shell echo 'int main(){return 0;}' | gcc -xc - -Ofast -o /dev/null >/dev/null 2>&1 ; echo $$?),0)
  CFLAGS += -Ofast
  else
  CFLAGS += -O3
  endif
  CFLAGS += -march=native -funroll-loops -ffast-math
endif

#CFLAGS += -D_MAT_X_VEC_RAW_

#CFLAGS += -D_TIME_PROF_
CFLAGS += -DNDEBUG
#CFLAGS += -pg
#LDFLAGS += -pg

BINDIR = ../bin
OUTLIBDIR = ../lib
OUTINCDIR = ../include/connlm
BIN_OBJ = ./bin_obj
SO_OBJ = ./so_obj

INC = connlm.h vocab.h output.h param.h rnn.h maxent.h lbl.h ffnn.h
TARGET_INC = $(addprefix $(OUTINCDIR)/, $(INC))

OBJ_SO = utils.o vocab.o output.o param.o rnn.o maxent.o lbl.o ffnn.o \
         connlm.o 

ifeq ($(UNAME_S),Darwin)
TARGET_SO = $(OUTLIBDIR)/libconnlm.dylib
SO_FLAGS = -dynamiclib
else
TARGET_SO = $(OUTLIBDIR)/libconnlm.so
SO_FLAGS = -shared
endif

BIN = connlm-train connlm-test connlm-info connlm-copy \
      connlm-vocab connlm-init connlm-output

TARGET_BIN = $(addprefix $(BINDIR)/, $(BIN))

.PHONY: all clean preparedir

all: preparedir $(TARGET_BIN)

preparedir: 
	@mkdir -p $(BIN_OBJ)
	@mkdir -p $(SO_OBJ)
	@mkdir -p ${BINDIR}
	@mkdir -p ${OUTLIBDIR}
	@mkdir -p ${OUTINCDIR}

$(TARGET_INC) : $(INC)
	cp $^ ${OUTINCDIR}

$(SO_OBJ)/%.o : %.c 
	$(GCC) $(CFLAGS) -fPIC -c -o $@ $<

$(TARGET_SO) : $(addprefix $(SO_OBJ)/, $(OBJ_SO))
	$(GCC) $(CFLAGS) $(SO_FLAGS) -o $@ $^ $(LDFLAGS)

$(BIN_OBJ)/%.o : %.c
	$(GCC) $(CFLAGS) -c -o $@ $^ 

$(TARGET_BIN) : $(BINDIR)/connlm-%: $(addprefix $(BIN_OBJ)/, connlm-%.o) $(TARGET_SO) $(TARGET_INC)
	$(GCC) $(CFLAGS) -o $@ $< $(LDFLAGS) -L$(OUTLIBDIR) -lconnlm 

lint-check : *.h *.cpp *.c
	splint +posixlib +D__gnuc_va_list=int -fileextensions $^ 

clean:
	rm -rf ${BIN_OBJ}
	rm -rf ${SO_OBJ}
	rm -rf ${BINDIR}
	rm -rf ${OUTLIBDIR}
	rm -rf ${OUTINCDIR}
	rm -rf ../include
	rm -f tags cscope.*

gen-doc:
	doxygen doc/Doxyfile
	cd html && git add * && git commit -a -m"generate doc on $(shell date)" && git push origin gh-pages

