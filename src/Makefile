CC = gcc
 
BINDIR = ../bin
OUTLIBDIR = ../lib
OUTINCDIR = ../include/connlm
BIN_OBJ = ./bin_obj
SO_OBJ = ./so_obj

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

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
LDFLAGS+=
else
LDFLAGS+=-lm
endif

CFLAGS += -I../tools/stutils/include/stutils/
LDFLAGS += -L../tools/stutils/lib/
LDFLAGS += -lstutils -lpthread
LDFLAGS += -Wl,-rpath,'$$ORIGIN'
LDFLAGS += -Wl,-rpath,$(shell pwd)/$(OUTLIBDIR)
LDFLAGS += -Wl,-rpath,$(shell pwd)/../tools/stutils/lib/

CFLAGS += -D _USE_DOUBLE_=0

CFLAGS += -g -Wall -Winline -pipe 
CFLAGS += -DNDEBUG
#CFLAGS += -D_TIME_PROF_


INC = connlm.h vocab.h output.h param.h rnn.h maxent.h lbl.h ffnn.h
TARGET_INC = $(addprefix $(OUTINCDIR)/, $(INC))

OBJ_SO = utils.o vocab.o output.o param.o rnn.o maxent.o lbl.o ffnn.o \
         connlm.o 

ifeq ($(UNAME_S),Darwin)
TARGET_SO = $(OUTLIBDIR)/libconnlm.dylib
SO_FLAGS = -dynamiclib -install_name $(shell pwd)/$(TARGET_SO)
else
TARGET_SO = $(OUTLIBDIR)/libconnlm.so
SO_FLAGS = -shared
endif

BIN = connlm-train connlm-test connlm-info connlm-copy \
      connlm-vocab connlm-init connlm-output connlm-gen

TARGET_BIN = $(addprefix $(BINDIR)/, $(BIN))

TEST_BIN = maxent-test utils-test param-test
ifneq (,$(findstring _USE_BLAS_,$(CFLAGS)))
    TEST_BIN += blas-test
endif

TARGET_TEST_BIN = $(addprefix $(BINDIR)/, $(TEST_BIN))

.PHONY: all clean preparedir test gen-dco

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
	$(CC) $(CFLAGS) -fPIC -c -o $@ $<

$(TARGET_SO) : $(addprefix $(SO_OBJ)/, $(OBJ_SO))
	$(CC) $(CFLAGS) $(SO_FLAGS) -o $@ $^ $(LDFLAGS)

$(BIN_OBJ)/%.o : %.c
	$(CC) $(CFLAGS) -c -o $@ $^ 

$(TARGET_BIN) : $(BINDIR)/connlm-%: $(addprefix $(BIN_OBJ)/, connlm-%.o) \
                $(TARGET_SO) $(TARGET_INC)
	$(CC) $(CFLAGS) -o $@ $< -L$(OUTLIBDIR) -lconnlm $(LDFLAGS) 

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
	doxygen doc/Doxyfile > /dev/null
	cd html && git add * && git commit -a -m"generate doc on $(shell date)" && git push origin gh-pages

$(TARGET_TEST_BIN) : $(BINDIR)/%-test: $(addprefix $(BIN_OBJ)/, %-test.o) $(TARGET_SO) $(TARGET_INC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) -L$(OUTLIBDIR) -lconnlm

test: preparedir $(TARGET_TEST_BIN)
	@result=0; for x in $(TARGET_TEST_BIN); do printf "Running $$x ..."; ./$$x >/dev/null 2>&1; if [ $$? -ne 0 ]; then echo "... FAIL $$x"; result=1; else echo "... SUCCESS";  fi;  done; exit $$result

