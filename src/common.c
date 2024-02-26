#include <stdio.h>
#include <string.h>

#include "common.h"
#include "lexer.h"

static const char *errty_to_str(const errty_t ty) {
	switch (ty) {
#define enumdef(e) case e: return #e;
errty_def
#undef enumdef
	default: return "ERR";
	}
}

inline static uint32_t ident_map_hash(const char *str, const size_t len) {
	return ((str[0] * 0x92) ^ str[1]) ^ ((str[len - 1] * 0x13 - str[len - 2]) << 8);
}

ident_map_t ident_map_init(const size_t maxlen_minus_1) {
	return (ident_map_t){
		.lenmask = maxlen_minus_1,
		.len = 0,
	};
}
void ident_map_add(ident_map_t *const map, const char *str, const size_t len, const int user) {
	if (map->len > map->lenmask) return;

	map->len++;
	ident_ent_t ent = (ident_ent_t){
		.str = str,
		.len = len,
		.user = user,
		.psl = 0,
	};

	int i = ident_map_hash(ent.str, ent.len) & map->lenmask;
	while (map->ents[i].str) {
		if (ent.psl > map->ents[i].psl) {
			// swap
			const ident_ent_t tmp = map->ents[i];
			map->ents[i] = ent;
			ent = tmp;
		}

		i = (i + 1) & map->lenmask;
		ent.psl++;
	}

	map->ents[i] = ent;
}
bool ident_map_get(ident_map_t *const map, const char *str, const size_t len, int **const user) {
	int i = ident_map_hash(str, len) & map->lenmask, psl = 0;
	while (map->ents[i].str && psl <= map->ents[i].psl) {
		if (map->ents[i].len == len && memcmp(map->ents[i].str, str, len) == 0) {
			if (user) *user = &map->ents[i].user;
			return true;
		}

		psl++;
		i = (i + 1) & map->lenmask;
	}

	if (user) *user = NULL;
	return false;
}

void err_print(const err_t *const err) {
	printf("%zu:%zu (%s): %s\n", err->line, err->chr, errty_to_str(err->ty), err->msg ? err->msg : "***");
}
