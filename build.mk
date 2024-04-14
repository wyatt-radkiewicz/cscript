SRCS := $(shell find $(SRC_DIR) -type f -name "*.c" )
OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))

CFLAGS       := -I$(SRC_DIR) $(CFLAGS) $(DEPS_CFLAGS)
LDFLAGS      := $(LDFLAGS)

ifeq ($(TARGET),dbg)
CFLAGS += -g -O0 -Wall -std=gnu2x -DDEBUG
endif
ifeq ($(TARGET),rel)
CFLAGS += -O2 -Wall -std=gnu2x
endif

$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)
run: $(PROG)
	$(PROG)
tools: $(BUILD_DIR)/keywordgen

$(BUILD_DIR)/keywordgen: tools/keywordgen.c src/lexer.h src/common.h
	mkdir -p $(BUILD_DIR) 
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(BUILD_DIR)/keywordgen tools/keywordgen.c

# Compiling
# This variable should be used as a macro which takes in
#   $(1) the source file
#   $(2) file.o: prereqs
define COMPILE
$$(BUILD_DIR)/$(dir $(1))$(2)
	mkdir -p $$(dir $$@)
	$(CC) $(CFLAGS) -o $$@ -c $(1)
endef

$(foreach src,$(SRCS),$(eval $(call COMPILE,$(src),$(shell $(CC) $(CFLAGS) -M $(src) | tr -d '\n' | tr '\\' ' '))))

# Clean up
.PHONY: clean
clean:
	rm -rf build_rel/ build_dbg/

