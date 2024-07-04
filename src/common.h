#ifndef _common_h_
#define _common_h_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct {
	const char *s;
	size_t len;
} strview_t;

#define make_strview(S) ((strview_t){ .s = S, .len = sizeof(S) - 1 })

static inline bool strview_eq(const strview_t lhs, const strview_t rhs) {
	if (lhs.len != rhs.len) return false;
	return memcmp(lhs.s, rhs.s, lhs.len) == 0;
}

#define arrlen(A) (sizeof(A) / sizeof((A)[0]))

typedef struct cnms_s cnms_t;

#endif

