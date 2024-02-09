#include <stdio.h>

#include "common.h"

static const char *errty_to_str(const errty_t ty) {
	switch (ty) {
#define enumdef(e) case e: return #e;
errty_def
#undef enumdef
	default: return "ERR";
	}
}

void err_print(const err_t *const err) {
	printf("%zu:%zu (%s): %s\n", err->line, err->chr, errty_to_str(err->ty), err->msg ? err->msg : "***");
}
