GCC = gcc
 
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
LDFLAGS+=
else
LDFLAGS+=-lm
endif

LDFLAGS += -lstutils -lpthread
LDFLAGS += -Wl,-rpath '-Wl,$$ORIGIN'

CFLAGS += -D REAL_TYPE=float

BINDIR=../bin
CFLAGS += -g -Wall -Winline -pipe -Wno-narrowing 
CFLAGS += -Ofast -march=native -funroll-loops -ffast-math
#CFLAGS += -D_MAT_X_VEC_DEBfloafloat
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

TARGET_INC = connlm.h rnn.h 

OBJ_SO = utils.o rnn.o maxent.o lbl.o ffnn.o connlmlib.o

TARGET_SO = libconnlm.so
TARGET1 = connlm

OBJ1 = $(OBJ_SO) connlm.o 

TARGET_BIN = $(TARGET1)

.PHONY: all clean preparedir

all:  preparedir $(TARGET_BIN) $(TARGET_SO) 

preparedir: 
	rm -rf $(BIN_OBJ)
	mkdir -p $(BIN_OBJ)
	rm -rf $(SO_OBJ)
	mkdir -p $(SO_OBJ)
	rm -rf ${BINDIR}
	mkdir -p ${BINDIR}
	rm -rf ${OUTLIBDIR}
	mkdir -p ${OUTLIBDIR}
	rm -rf ${OUTINCDIR}
	mkdir -p ${OUTINCDIR}

$(SO_OBJ)/%.o : %.c 
	$(GCC) $(CFLAGS) -fPIC -c -o $@ $<

$(TARGET_SO) : $(addprefix $(SO_OBJ)/, $(OBJ_SO))
	$(GCC) $(CFLAGS) -shared -o $@ $^ $(LDFLAGS)
	mv ${TARGET_SO} ${OUTLIBDIR}
	cp ${TARGET_INC} ${OUTINCDIR}


$(BIN_OBJ)/%.o : %.c
	$(GCC) $(CFLAGS) -c -o $@ $^ 

$(TARGET1) : $(addprefix $(BIN_OBJ)/, $(OBJ1))
	$(GCC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	mv ${TARGET1} ${BINDIR}

lint-check : *.h *.cpp *.c
	splint +posixlib +D__gnuc_va_list=int -fileextensions $^ 

clean:
	rm -rf ${BIN_OBJ}
	rm -rf ${SO_OBJ}
	rm -rf ${BINDIR}
	rm -rf ${OUTLIBDIR}
	rm -rf ${OUTINCDIR}
	rm -f tags cscope.*

gen-doc:
	doxygen doc/Doxyfile
	cd html && git add * && git commit -a -m"generate doc on $(shell date)" && git push origin gh-pages

