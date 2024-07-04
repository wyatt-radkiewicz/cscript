SRCS :=src/state.c src/tests.c src/lexer.c
BUILD :=build
TESTER :=$(BUILD)/cnms_tester
OBJDIR :=$(BUILD)/obj
OBJS :=$(patsubst src/%.c,$(OBJDIR)/%.o,$(SRCS))
CFLAGS :=$(CFLAGS)

test: $(TESTER)
	$<
dbg: CFLAGS +=-O0 -g
dbg: $(TESTER)
	lldb $<

$(TESTER): $(OBJS)
	$(CC) $(LDFLAGS) $^ -o $@

define COMPILE
$$(OBJDIR)/$(1)
	mkdir -p $$(OBJDIR)
	$$(CC) $$(CFLAGS) -c $(2) -o $$@
endef

$(foreach src,$(SRCS),$(eval $(call COMPILE,$(shell $(CC) $(CFLAGS) -M $(src) | tr -d '\\'),$(src))))

.PHONY: clean
clean:
	rm -rf $(BUILD)

