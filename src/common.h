#ifndef _common_h_
#define _common_h_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef DEBUG
#include <stdio.h>
#define dbgprintf(...) (printf(__VA_ARGS__))
#else
#define dbgprintf(...)
#endif

#define arrlen(arr) (sizeof(arr) / sizeof((arr)[0]))

#define errty_def \
	enumdef(err_noerr) \
	enumdef(err_toomany) \
	enumdef(err_unexpected) \
	enumdef(err_expected) \
	enumdef(err_unimplemented) \
	enumdef(err_limit)

typedef enum errty {
#define enumdef(e) e,
errty_def
#undef enumdef
} errty_t;

typedef struct err {
	errty_t ty;
	const char *msg;
	size_t line, chr;
} err_t;

typedef struct lexer lexer_t;

void err_print(const err_t *const err);

#endif

