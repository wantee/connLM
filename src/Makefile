PROJECT = connlm

include flags.mk

REV_INC=config.h
GIT_COMMIT_MACRO = CONNLM_GIT_COMMIT

INCS = utils.h \
       param.h \
       reader.h \
       driver.h \
       connlm.h \
       vocab.h \
       output.h \
       component.h \
       input.h \
       weight.h \
       layers/layer.h \
       layers/linear_layer.h \
       layers/sigmoid_layer.h \
       layers/tanh_layer.h \
       glues/glue.h \
       glues/sum_glue.h \
       glues/concat_glue.h \
       glues/clone_glue.h \
       glues/direct_glue.h \
       glues/fc_glue.h \
       glues/emb_glue.h \
       glues/out_glue.h \
       graph.h \
       updaters/updater.h \
       updaters/output_updater.h \
       updaters/layer_updater.h \
       updaters/component_updater.h \
       updaters/glue_updaters/glue_updater.h \
       updaters/glue_updaters/direct_glue_updater.h \
       updaters/glue_updaters/fc_glue_updater.h \
       updaters/glue_updaters/emb_glue_updater.h \
       updaters/glue_updaters/out_glue_updater.h \
       updaters/wt_updater.h

SRCS = utils.c \
       param.c \
       reader.c \
       driver.c \
       connlm.c \
       vocab.c \
       output.c \
       component.c \
       input.c \
       weight.c \
       layers/layer.c \
       layers/linear_layer.c \
       layers/sigmoid_layer.c \
       layers/tanh_layer.c \
       glues/glue.c \
       glues/sum_glue.c \
       glues/concat_glue.c \
       glues/clone_glue.c \
       glues/direct_glue.c \
       glues/fc_glue.c \
       glues/emb_glue.c \
       glues/out_glue.c \
       graph.c \
       updaters/updater.c \
       updaters/output_updater.c \
       updaters/layer_updater.c \
       updaters/component_updater.c \
       updaters/glue_updaters/glue_updater.c \
       updaters/glue_updaters/direct_glue_updater.c \
       updaters/glue_updaters/fc_glue_updater.c \
       updaters/glue_updaters/emb_glue_updater.c \
       updaters/glue_updaters/out_glue_updater.c \
       updaters/wt_updater.c

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
        tests/output-test \
        tests/input-test \
        tests/layer-test \
        tests/glue-test \
        tests/comp-test \
        tests/connlm-test
ifneq (,$(findstring _USE_BLAS_,$(CFLAGS)))
    TESTS += tests/blas-test
endif

#VAL_TESTS = tests/maxent-test
VAL_TESTS = tests/utils-test \
            tests/input-test \
            tests/output-test \
            tests/layer-test \
            tests/glue-test \
            tests/comp-test \
            tests/connlm-test

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
