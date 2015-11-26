ifeq ($(shell uname -s),Darwin)
target_so = $(OUTLIB_DIR)/$(1)/libconnlm.dylib
so_flags = -dynamiclib -install_name $(abspath $(call target_so,$(1)))
else
target_so = $(OUTLIB_DIR)/$(1)/libconnlm.so
so_flags = -shared
endif

CFLAGS_double = -D_USE_double_=1
CFLAGS_float = -D_USE_double_=0

define get_targets
$(addprefix $(BIN_DIR)/$(1)/,$(2))
endef

# $(1): double or float
# $(2): so_obj
define compile_so_obj

$(SO_OBJ_DIR)/$(1)/$(2) : $(2:.o=.c) $(2:.o=.h)
	@mkdir -p "$$(dir $$@)"
	$(CC) $(CFLAGS) $(CFLAGS_$(1)) -fPIC -c -o $$@ $$<

endef

# $(1): double or float
# $(2): so_objs
define compile_all_so_obj
$(foreach obj,$(2),$(call compile_so_obj,$(1),$(obj)))
endef

# $(1): double or float
# $(3): so_objs
define link_so

$(call target_so,$(1)) : $(addprefix $(SO_OBJ_DIR)/$(1)/,$(2))
	@mkdir -p "$$(dir $$@)"
	$(CC) $(CFLAGS) $(CFLAGS_$(1)) \
          $(call so_flags,$(1)) -o $$@ $$^ $(LDFLAGS)

endef

# $(1): double or float
# $(2): bin_obj
define compile_bin_obj

$(BIN_OBJ_DIR)/$(1)/$(2).o : bin/$(2).c
	@mkdir -p "$$(dir $$@)"
	$(CC) $(CFLAGS) $(CFLAGS_$(1)) -c -o $$@ $$<

endef

# $(1): double or float
# $(2): bin_objs
define compile_all_bin_obj
$(foreach obj,$(2),$(call compile_bin_obj,$(1),$(obj)))
endef

# $(1): double or float
# $(3): bin
define link_bin

$(BIN_DIR)/$(1)/$(2) : $(BIN_OBJ_DIR)/$(1)/$(2).o \
                      $(call target_so,$(1)) $(TARGET_INC)
	@mkdir -p "$$(dir $$@)"
	$(CC) $(CFLAGS) $(CFLAGS_$(1)) -o $$@ $$< \
          -L$(OUTLIB_DIR)/$(1) -lconnlm $(LDFLAGS) \
          -Wl,-rpath,$(abspath $(OUTLIB_DIR)/$(1))

endef

# $(1): double or float
# $(3): bins
define link_all_bin

$(foreach bin,$(2),$(call link_bin,$(1),$(bin)))

endef

#################################################
# TEST
#################################################
define get_test_targets
$(addprefix $(BIN_OBJ_DIR)/test/$(1)/,$(2))
endef

# $(1): double or float
# $(2): test_obj
define compile_test_obj

$(BIN_OBJ_DIR)/test/$(1)/$(2).o : $(2).c
	@mkdir -p "$$(dir $$@)"
	$(CC) $(CFLAGS) $(CFLAGS_$(1)) -UNDEBUG -c -o $$@ $$<

endef

# $(1): double or float
# $(2): test_objs
define compile_all_test_obj
$(foreach obj,$(2),$(call compile_test_obj,$(1),$(obj)))
endef

# $(1): double or float
# $(3): bin
define link_test

$(BIN_OBJ_DIR)/test/$(1)/$(2) : $(BIN_OBJ_DIR)/test/$(1)/$(2).o \
                      $(call target_so,$(1)) $(TARGET_INC)
	@mkdir -p "$$(dir $$@)"
	$(CC) $(CFLAGS) $(CFLAGS_$(1)) -o $$@ $$< \
          -L$(OUTLIB_DIR)/$(1) -lconnlm $(LDFLAGS) \
          -Wl,-rpath,$(abspath $(OUTLIB_DIR)/$(1))

endef

# $(1): double or float
# $(3): bins
define link_all_test

$(foreach bin,$(2),$(call link_test,$(1),$(bin)))

endef

define test_targets

test-$(1): $(call get_test_targets,$(1),$(2))
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

# To debug, replace $(eval, ...) to $(info, ...)

