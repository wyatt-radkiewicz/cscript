#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <setjmp.h>

#include "cscript.h"

#define RED "\x1B[31;49m"
#define DEFAULT "\x1B[39;49m"

static char *loadfile(const char *const path) {
	FILE *file = fopen(path, "r");
	if (!file) {
		printf(RED"Error while loading file: %s"DEFAULT"\n", path);
		return NULL;
	}

	fseek(file, 0, SEEK_END);
	size_t len = ftell(file);
	fseek(file, 0, SEEK_SET);

	char *str = malloc(len + 1);
	if (fread(str, 1, len, file) != len) {
		free(str);
		fclose(file);
		return NULL;
	}
	str[len] = '\0';
	fclose(file);

	return str;
}

static int ntests;
static int ntests_passed;
static jmp_buf test_jmpbuf;

static void signal_handler(int signal) {
	(void)signal;
	longjmp(test_jmpbuf, false);
}

#define TRYRUN(NAME, EXPR) {					\
		printf("test: " #NAME "\t\t");			\
		ntests++;								\
		if (handle_segv && setjmp(test_jmpbuf)) {	\
			printf(RED"*FAILED* (SEGV)"DEFAULT"\n");\
			goto jmpbuf_goto_##NAME;				\
		}										\
		if (EXPR) {								\
			printf("PASSED!\n");				\
			ntests_passed++;					\
		} else {								\
			printf(RED"*FAILED*"DEFAULT"\n");	\
		}										\
	}											\
	jmpbuf_goto_##NAME:

static uint8_t test_arena_buffer[1024*4];
static int test_arena_loc;

static void *alloc(void *usr, size_t size) {
	(void)usr;

	void *ptr = test_arena_buffer + test_arena_loc;
	test_arena_loc += alignuu(size, alignof(max_align_t));
	return ptr;
}

int main(int argc, char **argv) {
	printf("================================================================================\n");
	printf("=== cscript tester ===\n");
	printf("================================================================================\n");
	
	const bool handle_segv = argc != 2 || strcmp(argv[1], "dbg");
	if (handle_segv) signal(SIGSEGV, signal_handler);

	char *file = loadfile("examples/cscript.cun");
	if (!file) return -1;

	cs__state state;
	TRYRUN(cs__state_init, cs__state_init(&state, (cs_arenainf){ .fn = alloc }));
	free(file);

	printf("tests passed %d/%d\n", ntests_passed, ntests);
	printf("tests failed %d\n", ntests - ntests_passed);

	return 0;
}

