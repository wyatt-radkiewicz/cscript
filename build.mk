###############################################################################
# The actual script that builds cscript
#
# Put in a different file because many variables change with configure
###############################################################################

#
# Sources and objects
#
SRCS 		:=$(shell find $(SRC_DIR) -type f -name "*.c")
TEST_SRCS	:=$(shell find $(TEST_DIR) -type f -name "*.c")
OBJS		:=$(patsubst %.c,$(TARGET_DIR)/%.o,$(SRCS))
TEST_OBJS	:=$(patsubst %.c,$(TEST_TARGET_DIR)/%.o,$(TEST_SRCS))

#
# Setting flags and linker flags
#

# Common compiler flags
CFLAGS_COMMON	:=-Iinclude -I$(SRC_DIR) $(CFLAGS) -Wall -Wextra -pedantic -Wmissing-prototypes -Wstrict-prototypes -Wold-style-definition

# ANSI Standards compliance flags
ifeq ($(ANSI_STRICT),1)
CFLAGS_COMMON	+= -std=c89 -DANSI_STRICT
endif
ifeq ($(ANSI_STRICT),0)
CFLAGS_COMMON	+= -std=c99
endif

# Build mode dependent compiler flags
ifeq ($(BUILD_MODE),dbg)
CFLAGS_COMMON	+= -g -O0 -Wall
endif
ifeq ($(BUILD_MODE),rel)
CFLAGS_COMMON	+= -O2 -Wall -DNDEBUG
endif

# Test/library dependent compiler flags
TEST_CFLAGS		:=$(CFLAGS_COMMON) -I$(TEST_DIR)
CFLAGS			:=-fPIC -shared $(CFLAGS_COMMON) -ffreestanding

# Library flags
LDFLAGS_COMMON  :=$(LDFLAGS) -lm
LDFLAGS			:=$(LDFLAGS_COMMON) -fPIC -shared -ffreestanding
TEST_LDFLAGS	:=$(LDFLAGS_COMMON) -L./$(TARGET_DIR) -l$(LIB)

# Misc. Changes
ifeq ($(BUILD_MODE),dbg)
CFLAGS			+= -rdynamic
LDFLAGS			+= -rdynamic
endif

#
# Compiling
# This variable should be used as a macro which takes in
#   $(1) The source file
#   $(2) The build directory
#   $(3) CFLAGS to be used
#   $(4) In the format of (obtained via gcc -M):
#   	 file.o: prereqs
#
define COMPILE
$(2)/$(dir $(1))$(4)
	mkdir -p $$(dir $$@)
	$(CC) $(3) -o $$@ -c $(1)
endef

#
# Building the Library
#
lib: $(TARGET)
$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

$(foreach src,$(SRCS),$(eval $(call COMPILE,$(src),$(TARGET_DIR),$(CFLAGS),$(shell $(CC) $(CFLAGS) -M $(src) | tr -d '\n' | tr '\\' ' '))))

#
# Testing
#
test: $(TEST_TARGET)
	LD_LIBRARY_PATH="$(LD_LIBRARY_PATH):$(shell pwd)/$(TARGET_DIR)" $(TEST_TARGET)
$(TEST_TARGET): $(TEST_OBJS) $(TARGET)
	$(CC) -o $@ $(TEST_OBJS) $(TEST_LDFLAGS)


$(foreach src,$(TEST_SRCS),$(eval $(call COMPILE,$(src),$(TEST_TARGET_DIR),$(TEST_CFLAGS),$(shell $(CC) $(TEST_CFLAGS) -M $(src) | tr -d '\n' | tr '\\' ' '))))

#
# Clean up
#
.PHONY: clean
clean:
	rm -rf build_rel/ build_dbg/

