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

typedef struct ident_ent {
	const char *str;
	int len, psl, user;
} ident_ent_t;
typedef struct ident_map {
	size_t lenmask;
	size_t len;
	ident_ent_t ents[];
} ident_map_t;

ident_map_t ident_map_init(const size_t maxlen);
void ident_map_add(ident_map_t *const map, const char *str, const size_t len, const int user);
bool ident_map_get(ident_map_t *const map, const char *str, const size_t len, int **const user);

void err_print(const err_t *const err);

#endif

