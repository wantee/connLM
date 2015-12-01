CC = gcc

OUTINC_DIR = ../include/connlm/

BIN_DIR=../bin
OUTLIB_DIR=../lib
BIN_OBJ_DIR=./bin_obj
SO_OBJ_DIR=./so_obj

include flags.mk
include templates.mk

INC = utils.h config.h \
      connlm.h param.h rnn.h maxent.h lbl.h ffnn.h \
      vocab.h \
      output.h \
      component.h \
      input.h \
      layers/layer.h \
      layers/sigmoid_layer.h \
      layers/tanh_layer.h \
      weights/weight.h \
      weights/embedding_weight.h \
      weights/output_weight.h

OBJ_SO = utils.o param.o rnn.o maxent.o lbl.o ffnn.o \
         connlm.o \
         vocab.o \
         output.o \
         component.o \
         input.o \
         layers/layer.o \
         layers/sigmoid_layer.o \
         layers/tanh_layer.o \
         weights/weight.o \
         weights/embedding_weight.o \
         weights/output_weight.o \

BIN = connlm-train connlm-eval connlm-info connlm-copy \
      connlm-vocab connlm-init connlm-output connlm-gen connlm-merge

TEST_BIN = maxent-test utils-test param-test connlm-test
ifneq (,$(findstring _USE_BLAS_,$(CFLAGS)))
    TEST_BIN += blas-test
endif
VAL_TEST_BIN = maxent-test utils-test param-test connlm-test

.PHONY: all double float clean inc
.PHONY: test test-double test-float
.PHONY: prepare-doc gen-doc publish-doc

all: double float 

inc: $(INC)
	@mkdir -p $(OUTINC_DIR)
	$(foreach f,$(INC),$(call copy_inc,$f,$(OUTINC_DIR)))

double: inc $(call get_targets,double,$(BIN))

float: inc $(call get_targets,float,$(BIN))

mktargets = $(foreach type, float double, $(eval $(call $(1),$(type),$(2))))

$(call mktargets,compile_all_so_obj,$(OBJ_SO))

$(call mktargets,link_so,$(OBJ_SO))

$(call mktargets,compile_all_bin_obj,$(BIN))

$(call mktargets,link_all_bin,$(BIN))

lint-check : *.h *.c
	splint +posixlib +D__gnuc_va_list=int -fileextensions $^

clean:
	rm -rf $(BIN_OBJ_DIR)
	rm -rf $(SO_OBJ_DIR)
	rm -rf $(BIN_DIR)
	rm -rf $(OUTLIB_DIR)
	rm -rf $(OUTINC_DIR)
	rm -rf ../include
	rm -f tags cscope.*

test: test-double test-float

$(call mktargets,compile_all_test_obj,$(TEST_BIN))

$(call mktargets,link_all_test,$(TEST_BIN))

$(call mktargets,test_targets,$(TEST_BIN))

val-test: val-test-double val-test-float

$(call mktargets,val_test_targets,$(VAL_TEST_BIN))

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

