ifeq ($(shell uname -s),Darwin)
target_so = $(OUTLIB_DIR)/$(1)/libconnlm.dylib
so_flags = -dynamiclib -install_name $(abspath $(call target_so,$(1)))
else
target_so = $(OUTLIB_DIR)/$(1)/libconnlm.so
so_flags = -shared
endif

CFLAGS_double = -D_USE_DOUBLE_=1
CFLAGS_float = -D_USE_DOUBLE_=0

define get_targets
$(addprefix $(BIN_DIR)/$(1)/,$(2))
endef

# $(1): double or float
# $(2): obj
define compile_one_obj

$(OBJ_DIR)/$(1)/$(2) : $(2:.o=.c) $(2:.o=.h)
	@mkdir -p "$$(dir $$@)"
	$(CC) $(CFLAGS) $(CFLAGS_$(1)) -fPIC -c -o $$@ $$<

endef

# $(1): double or float
# $(2): objs
define compile_objs
$(foreach obj,$(2),$(call compile_one_obj,$(1),$(obj)))
endef

# $(1): double or float
# $(2): objs
define link_so

$(call target_so,$(1)) : $(addprefix $(OBJ_DIR)/$(1)/,$(2))
	@mkdir -p "$$(dir $$@)"
	$(CC) $(CFLAGS) $(CFLAGS_$(1)) \
          $(call so_flags,$(1)) -o $$@ $$^ $(LDFLAGS)

endef

# $(1): double or float
# $(2): bin
define link_one_bin

$(call get_targets,$(1),$(2)) : $(2).c $(call target_so,$(1)) $(TARGET_INC)
	@mkdir -p "$$(dir $$@)"
	$(CC) $(CFLAGS) $(CFLAGS_$(1)) -o $$@ $$< \
          -L$(OUTLIB_DIR)/$(1) -lconnlm $(LDFLAGS) \
          -Wl,-rpath,$(abspath $(OUTLIB_DIR)/$(1))

endef

# $(1): double or float
# $(3): bins
define link_bins

$(foreach bin,$(2),$(call link_one_bin,$(1),$(bin)))

endef

#################################################
# TEST
#################################################
define get_test_targets
$(addprefix $(OBJ_DIR)/$(1)/,$(2))
endef

# $(1): double or float
# $(2): test_bin
define link_one_test

$(OBJ_DIR)/$(1)/$(2) : $(2).c $(call target_so,$(1)) $(TARGET_INC)
	@mkdir -p "$$(dir $$@)"
	$(CC) $(CFLAGS) $(CFLAGS_$(1)) -UNDEBUG -o $$@ $$< \
          -L$(OUTLIB_DIR)/$(1) -lconnlm $(LDFLAGS) \
          -Wl,-rpath,$(abspath $(OUTLIB_DIR)/$(1))

endef

# $(1): double or float
# $(2): test_bins
define link_tests

$(foreach bin,$(2),$(call link_one_test,$(1),$(bin)))

endef

define test_targets

test-$(1): inc $(call get_test_targets,$(1),$(2))
	@result=0; \
    for x in $(call get_test_targets,$(1),$(2)); do \
      printf "Running $$$$x ..."; \
      ./$$$$x >/dev/null 2>&1; \
      if [ $$$$? -ne 0 ]; then \
        echo "... FAIL"; \
        result=1; \
      else \
        echo "... SUCCESS"; \
      fi; \
    done; \
    exit $$$$result

endef

define val_test_targets

val-test-$(1): inc $(call get_test_targets,$(1),$(2))
	@result=0; \
    for x in $(call get_test_targets,$(1),$(2)); do \
      printf "Valgrinding $$$$x ..."; \
      valgrind ./$$$$x; \
      if [ $$$$? -ne 0 ]; then \
        echo "... FAIL"; \
        result=1; \
      else \
        echo "... SUCCESS"; \
      fi; \
    done; \
    exit $$$$result

endef

define copy_inc

    @mkdir -p "$(2)/$(dir $(1))"
    @cp "$(1)" "$(2)/$(dir $(1))"

endef
# To debug, replace $(eval, ...) to $(info, ...)

