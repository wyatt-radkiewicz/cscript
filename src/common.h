#ifndef _common_h_
#define _common_h_

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "cnm.h"

#define ARRSZ(a) (sizeof(a) / sizeof((a)[0]))

#define min(a, b) ({ \
	typeof(a) __min_a = (a); \
	typeof(b) __min_b = (b); \
	__min_a < __min_b ? __min_a : __min_b; \
})
#define max(a, b) ({ \
	typeof(a) __max_a = (a); \
	typeof(b) __max_b = (b); \
	__max_a < __max_b ? __max_a : __max_b; \
})

typedef struct cnm_s cnm_t;

typedef struct {
	const char *str;
	size_t len;
} strview_t;

#define SV(s) ((strview_t){ .str = s, .len = sizeof(s) - 1})

static inline bool strview_eq(const strview_t a, const strview_t b) {
	if (a.len != b.len)
		return false;
	return memcmp(a.str, b.str, a.len) == 0;
}

typedef enum {
#define TOK(name) TOK_##name,
#define SINGLE_TOK(c1, name) TOK_##name,
#define DOUBLE_TOK(c1, name1, c2, name2) TOK_##name1, TOK_##name2,
#define TRIPLE_TOK(c1, name1, c2, name2, c3, name3) TOK_##name1, TOK_##name2, TOK_##name3,
#include "tokens.h"
#undef TOK
#undef SINGLE_TOK
#undef DOUBLE_TOK
#undef TRIPLE_TOK
} tokty_t;

typedef struct {
	tokty_t ty;
	strview_t src;
} tok_t;

const tok_t *tnext(cnm_t *cnm);

// Main CNM state
// This 'should' be seperate structs, but making it one big one helps code
// readability and makes things easier for me (except for when it doesn't)
// so deal wit it.
struct cnm_s {
	// Source/lexing
	char filename[128];
	const char *src;
	tok_t tok;

	// Error handling
	struct {
		cnm_errcb_t cb;
		char pbuf[1024];
		int nerrs;
	} err;
};

typedef struct {
	strview_t area;
	char comment[256];
	bool critical;
} errinf_t;

// Handles errors and warnings
void doerr(cnm_t *cnm, int code, const char *desc,
	bool iserr, errinf_t *infs, int ninfs);

#endif

