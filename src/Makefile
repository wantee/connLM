CC = gcc

OUTINC_DIR = ../include/
BIN_DIR=../bin
OUTLIB_DIR=../lib

OBJ_DIR=./.objs
DEP_DIR=./.deps

include flags.mk
include templates.mk

INC = utils.h \
      connlm.h param.h rnn.h maxent.h lbl.h ffnn.h \
      vocab.h \
      output.h \
      component.h \
      input.h \
      layers/layer.h \
      layers/sigmoid_layer.h \
      layers/tanh_layer.h \
      glues/glue.h \
      glues/sum_glue.h \
      glues/append_glue.h \
      glues/clone_glue.h \
      weights/weight.h \
      weights/embedding_weight.h \
      weights/output_weight.h \
      graph.h

CONFIG_INC=config.h
OUT_CONFIG=$(OUTINC_DIR)/connlm/config.h

OBJS = utils.o \
       param.o \
       rnn.o \
       maxent.o \
       lbl.o \
       ffnn.o \
       connlm.o \
       vocab.o \
       output.o \
       component.o \
       input.o \
       layers/layer.o \
       layers/sigmoid_layer.o \
       layers/tanh_layer.o \
       glues/glue.o \
       glues/sum_glue.o \
       glues/append_glue.o \
       glues/clone_glue.o \
       weights/weight.o \
       weights/embedding_weight.o \
       weights/output_weight.o \
       graph.o

BINS = bin/connlm-train \
       bin/connlm-eval \
       bin/connlm-info \
       bin/connlm-copy \
       bin/connlm-vocab \
       bin/connlm-init \
       bin/connlm-output \
       bin/connlm-gen \
       bin/connlm-merge

TEST_BINS = tests/maxent-test \
            tests/utils-test \
            tests/param-test \
            tests/connlm-test
ifneq (,$(findstring _USE_BLAS_,$(CFLAGS)))
    TEST_BINS += tests/blas-test
endif

VAL_TEST_BINS = tests/maxent-test \
                tests/utils-test \
                tests/param-test \
                tests/connlm-test

.PHONY: all double float clean inc config
.PHONY: depend depend-double depend-float
.PHONY: test test-double test-float
.PHONY: prepare-doc gen-doc publish-doc

all: double float 

config:
	@../tools/replace_revision.sh $(CONFIG_INC) $(OUT_CONFIG)

$(OUT_CONFIG): $(CONFIG_INC)
	@../tools/replace_revision.sh $(CONFIG_INC) $(OUT_CONFIG) 1

inc: $(INC) config
	@mkdir -p "$(OUTINC_DIR)/connlm"
	$(foreach f,$(INC),$(call copy_inc,$f,"$(OUTINC_DIR)/connlm"))

double: inc $(OUT_CONFIG) $(call get_targets,double,$(BINS))

float: inc $(OUT_CONFIG) $(call get_targets,float,$(BINS))

mktargets = $(foreach type, float double, $(eval $(call $(1),$(type),$(2))))

$(call mktargets,compile_objs,$(OBJS))

$(call mktargets,link_so,$(OBJS))

$(call mktargets,link_bins,$(BINS))

lint-check : *.h *.c
	splint +posixlib +D__gnuc_va_list=int -fileextensions $^

clean:
	rm -rf $(OBJ_DIR)
	rm -rf $(DEP_DIR)
	rm -rf $(BIN_DIR)
	rm -rf $(OUTLIB_DIR)
	rm -rf $(OUTINC_DIR)
	rm -f tags cscope.*

test: test-double test-float

$(call mktargets,link_tests,$(TEST_BINS))

$(call mktargets,test_targets,$(TEST_BINS))

val-test: val-test-double val-test-float

$(call mktargets,val_test_targets,$(VAL_TEST_BINS))

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

.deps/%.mk:%.c
	mkdir -p `dirname "$@"`
	$(CC) $(CFLAGS) -M $< > $@
	echo '	$(CC) -c $(CFLAGS) $< -o $@.o' >> $@
