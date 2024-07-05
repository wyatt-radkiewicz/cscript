#ifndef _test_common_h_
#define _test_common_h_

#include <stdarg.h>
#include <stdio.h>

#include "../common.h"
#include "../state.h"
#include "../lexer.h"
#include "../type.h"
#include "../vm.h"
#include "../cnms.h"

//
// Test helper functions
//
static const char *curr_test = NULL;

static bool _perr(const char *const file, const int line, const char *const msg, ...) {
	printf("test_%s [%s:%d]: ", curr_test, file, line);

	va_list args;
	va_start(args, msg);
	vprintf(msg, args);
	va_end(args);

	printf("\n");
	return false;
}
#define perr(...) (_perr(__FILE__, __LINE__, __VA_ARGS__))

typedef bool (*test_pfn_t)(void);

typedef struct {
	test_pfn_t fn;
	const char *name;
} test_t;

static void error_handler(void *const user, const int line, const char *const msg) {
	printf("test_%s compile error: %s\n", curr_test, msg);
}

#endif

