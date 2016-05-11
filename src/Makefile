PROJECT = connlm

include flags.mk

REV_INC=config.h
GIT_COMMIT_MACRO = CONNLM_GIT_COMMIT

INCS = utils.h \
       param.h \
       connlm.h \
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

SRCS = utils.c \
       param.c \
       connlm.c \
       vocab.c \
       output.c \
       component.c \
       input.c \
       layers/layer.c \
       layers/sigmoid_layer.c \
       layers/tanh_layer.c \
       glues/glue.c \
       glues/sum_glue.c \
       glues/append_glue.c \
       glues/clone_glue.c \
       weights/weight.c \
       weights/embedding_weight.c \
       weights/output_weight.c \
       graph.c

BINS = bin/connlm-train \
       bin/connlm-eval \
       bin/connlm-info \
       bin/connlm-copy \
       bin/connlm-vocab \
       bin/connlm-init \
       bin/connlm-output \
       bin/connlm-gen \
       bin/connlm-draw \
#       bin/connlm-merge maybe rename to connlm-edit

#TESTS = tests/maxent-test
TESTS = tests/utils-test \
        tests/param-test \
        tests/connlm-test \
        tests/input-test \
        tests/output-test
ifneq (,$(findstring _USE_BLAS_,$(CFLAGS)))
    TESTS += tests/blas-test
endif

#VAL_TESTS = tests/maxent-test
VAL_TESTS = tests/utils-test \
            tests/param-test \
            tests/connlm-test \
            tests/input-test \
            tests/output-test

define get_target

.PHONY: $(1) $(1)-double $(1)-float

$(1): $(1)-float $(1)-double

$(1)-float: PREFIX = float
$(1)-float: CPPFLAGS = -D_USE_DOUBLE_=0
$(1)-float: float$(1)

$(1)-double: PREFIX = double
$(1)-double: CPPFLAGS = -D_USE_DOUBLE_=1
$(1)-double: double$(1)

endef

$(eval $(call get_target,all))

$(eval $(call get_target,test))

$(eval $(call get_target,val-test))

$(eval $(call get_target,clean-test))

clean: clean-static
	rm -rf ../float/
	rm -rf ../double/
	rm -rf .objs/
	rm -rf .deps/

COMMON_MK_DIR=../tools/stutils/src
include $(COMMON_MK_DIR)/common_static.mk

PREFIX = float
include $(COMMON_MK_DIR)/common.mk

PREFIX = double
include $(COMMON_MK_DIR)/common.mk
