CC = gcc

OUTINCDIR = ../include/connlm/

BINDIR=../bin
OUTLIBDIR=../lib
BIN_OBJ=./bin_obj
SO_OBJ=./so_obj

BINDIR_DOUBLE = $(BINDIR)/double/
OUTLIBDIR_DOUBLE = $(OUTLIBDIR)/double/
BIN_OBJ_DOUBLE = $(BIN_OBJ)/double/
SO_OBJ_DOUBLE = $(SO_OBJ)/double/
BIN_TEST_OBJ_DOUBLE = $(BIN_OBJ_DOUBLE)/test/

BINDIR_FLOAT = $(BINDIR)/float/
OUTLIBDIR_FLOAT = $(OUTLIBDIR)/float/
BIN_OBJ_FLOAT = $(BIN_OBJ)/float/
SO_OBJ_FLOAT = $(SO_OBJ)/float/
BIN_TEST_OBJ_FLOAT = $(BIN_OBJ_FLOAT)/test/

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
LDFLAGS += -Wl,-rpath,$(shell pwd)/../tools/stutils/lib/
LDFLAGS_DOUBLE += -Wl,-rpath,$(shell pwd)/$(OUTLIBDIR_DOUBLE)
LDFLAGS_FLOAT += -Wl,-rpath,$(shell pwd)/$(OUTLIBDIR_FLOAT)

CFLAGS += -g -Wall -Winline -pipe
CFLAGS += -DNDEBUG
#CFLAGS += -D_TIME_PROF_
CFLAGS_DOUBLE = -D_USE_DOUBLE_=1
CFLAGS_FLOAT = -D_USE_DOUBLE_=0

INC = connlm.h vocab.h output.h param.h rnn.h maxent.h lbl.h ffnn.h
TARGET_INC = $(addprefix $(OUTINCDIR)/, $(INC))

OBJ_SO = utils.o vocab.o output.o param.o rnn.o maxent.o lbl.o ffnn.o \
         connlm.o

ifeq ($(UNAME_S),Darwin)
TARGET_SO_DOUBLE = $(OUTLIBDIR_DOUBLE)/libconnlm.dylib
SO_FLAGS_DOUBLE = -dynamiclib -install_name $(shell pwd)/$(TARGET_SO_DOUBLE)

TARGET_SO_FLOAT = $(OUTLIBDIR_FLOAT)/libconnlm.dylib
SO_FLAGS_FLOAT = -dynamiclib -install_name $(shell pwd)/$(TARGET_SO_FLOAT)
else
TARGET_SO_DOUBLE = $(OUTLIBDIR_DOUBLE)/libconnlm.so
SO_FLAGS_DOUBLE = -shared

TARGET_SO_FLOAT = $(OUTLIBDIR_FLOAT)/libconnlm.so
SO_FLAGS_FLOAT = -shared
endif

BIN = connlm-train connlm-test connlm-info connlm-copy \
      connlm-vocab connlm-init connlm-output connlm-gen connlm-merge

TARGET_BIN_DOUBLE = $(addprefix $(BINDIR_DOUBLE)/, $(BIN))
TARGET_BIN_FLOAT = $(addprefix $(BINDIR_FLOAT)/, $(BIN))

TEST_BIN = maxent-test utils-test param-test
ifneq (,$(findstring _USE_BLAS_,$(CFLAGS)))
    TEST_BIN += blas-test
endif

TARGET_TEST_BIN_DOUBLE = $(addprefix $(BIN_TEST_OBJ_DOUBLE)/, $(TEST_BIN))
TARGET_TEST_BIN_FLOAT = $(addprefix $(BIN_TEST_OBJ_FLOAT)/, $(TEST_BIN))

.PHONY: all clean preparedir test gen-doc

all: preparedir $(TARGET_BIN_DOUBLE) $(TARGET_BIN_FLOAT)

preparedir: preparedir_double preparedir_float

preparedir_common:
	@mkdir -p ${OUTINCDIR}

preparedir_double: preparedir_common
	@mkdir -p $(BIN_OBJ_DOUBLE)
	@mkdir -p $(SO_OBJ_DOUBLE)
	@mkdir -p ${BINDIR_DOUBLE}
	@mkdir -p ${OUTLIBDIR_DOUBLE}
	@mkdir -p ${BIN_TEST_OBJ_DOUBLE}

preparedir_float: preparedir_common
	@mkdir -p $(BIN_OBJ_FLOAT)
	@mkdir -p $(SO_OBJ_FLOAT)
	@mkdir -p ${BINDIR_FLOAT}
	@mkdir -p ${OUTLIBDIR_FLOAT}
	@mkdir -p ${BIN_TEST_OBJ_FLOAT}

$(TARGET_INC) : $(INC)
	cp $^ ${OUTINCDIR}

$(SO_OBJ_DOUBLE)/%.o : %.c
	$(CC) $(CFLAGS) $(CFLAGS_DOUBLE) -fPIC -c -o $@ $<

$(TARGET_SO_DOUBLE) : $(addprefix $(SO_OBJ_DOUBLE)/, $(OBJ_SO))
	$(CC) $(CFLAGS) $(CFLAGS_DOUBLE) $(SO_FLAGS_DOUBLE) \
          -o $@ $^ $(LDFLAGS) $(LDFLAGS_DOUBLE)

$(BIN_OBJ_DOUBLE)/%.o : %.c
	$(CC) $(CFLAGS) $(CFLAGS_DOUBLE) -c -o $@ $^

$(TARGET_BIN_DOUBLE) : $(BINDIR_DOUBLE)/connlm-% \
                     : $(addprefix $(BIN_OBJ_DOUBLE)/, connlm-%.o) \
                       $(TARGET_SO_DOUBLE) $(TARGET_INC)
	$(CC) $(CFLAGS) $(CFLAGS_DOUBLE) -o $@ $< \
          -L$(OUTLIBDIR_DOUBLE) -lconnlm $(LDFLAGS) $(LDFLAGS_DOUBLE)

$(SO_OBJ_FLOAT)/%.o : %.c
	$(CC) $(CFLAGS) $(CFLAGS_FLOAT) -fPIC -c -o $@ $<

$(TARGET_SO_FLOAT) : $(addprefix $(SO_OBJ_FLOAT)/, $(OBJ_SO))
	$(CC) $(CFLAGS) $(CFLAGS_FLOAT) $(SO_FLAGS_FLOAT) \
          -o $@ $^ $(LDFLAGS) $(LDFLAGS_FLOAT)

$(BIN_OBJ_FLOAT)/%.o : %.c
	$(CC) $(CFLAGS) $(CFLAGS_FLOAT) -c -o $@ $^

$(TARGET_BIN_FLOAT) : $(BINDIR_FLOAT)/connlm-% \
                     : $(addprefix $(BIN_OBJ_FLOAT)/, connlm-%.o) \
                       $(TARGET_SO_FLOAT) $(TARGET_INC)
	$(CC) $(CFLAGS) $(CFLAGS_FLOAT) -o $@ $< \
          -L$(OUTLIBDIR_FLOAT) -lconnlm $(LDFLAGS) $(LDFLAGS_FLOAT)

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

gen-doc: prepare-doc
	rm -rf html/*
	doxygen doc/Doxyfile > /dev/null

prepare-doc:
	mkdir -p html
	if [ ! -d html/.git ]; then \
      git clone git@github.com:wantee/connLM.git html; \
    fi
	cd html && \
    git checkout gh-pages

publish-doc: gen-doc
	cd html && \
	git add * && \
	git commit -a -m"generate doc on $(shell date)" && \
	git push origin gh-pages

$(BIN_TEST_OBJ_DOUBLE)/%-test.o : %-test.c
	$(CC) $(CFLAGS) $(CFLAGS_DOUBLE) -UNDEBUG -c -o $@ $^

$(TARGET_TEST_BIN_DOUBLE) : $(BIN_TEST_OBJ_DOUBLE)/%-test\
                   : $(addprefix $(BIN_TEST_OBJ_DOUBLE)/, %-test.o) \
                     $(TARGET_SO_DOUBLE) $(TARGET_INC)
	$(CC) $(CFLAGS) $(CFLAGS_DOUBLE) -o $@ $< \
          -L$(OUTLIBDIR_DOUBLE) -lconnlm $(LDFLAGS) $(LDFLAGS_DOUBLE)

$(BIN_TEST_OBJ_FLOAT)/%-test.o : %-test.c
	$(CC) $(CFLAGS) $(CFLAGS_FLOAT) -UNDEBUG -c -o $@ $^

$(TARGET_TEST_BIN_FLOAT) : $(BIN_TEST_OBJ_FLOAT)/%-test\
                   : $(addprefix $(BIN_TEST_OBJ_FLOAT)/, %-test.o) \
                     $(TARGET_SO_FLOAT) $(TARGET_INC)
	$(CC) $(CFLAGS) $(CFLAGS_FLOAT) -UNDEBUG -o $@ $< \
          -L$(OUTLIBDIR_FLOAT) -lconnlm $(LDFLAGS) $(LDFLAGS_FLOAT) 

test: test_double test_float

test_double: preparedir $(TARGET_TEST_BIN_DOUBLE)
	@result=0; for x in $(TARGET_TEST_BIN_DOUBLE); do printf "Running $$x ..."; ./$$x >/dev/null 2>&1; if [ $$? -ne 0 ]; then echo "... FAIL"; result=1; else echo "... SUCCESS";  fi;  done; exit $$result

test_float: preparedir $(TARGET_TEST_BIN_FLOAT)
	@result=0; for x in $(TARGET_TEST_BIN_FLOAT); do printf "Running $$x ..."; ./$$x >/dev/null 2>&1; if [ $$? -ne 0 ]; then echo "... FAIL"; result=1; else echo "... SUCCESS";  fi;  done; exit $$result

