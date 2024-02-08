#ifndef _common_h_
#define _common_h_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define arrlen(arr) (sizeof(arr) / sizeof((arr)[0]))

typedef enum errty {
	err_unexpected,
	err_expected,
} errty_t;

typedef struct err {
	errty_t ty;
	const char *msg;
} err_t;

#endif

