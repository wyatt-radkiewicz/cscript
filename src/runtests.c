#include <stdio.h>

#include "common.h"

void printf_errcb(const char *verbose, const char *simple, int line) {
	printf("%s", verbose);
}

const char *tokname(tokty_t t) {
	switch (t) {
#define TOK(name) \
	case TOK_##name: return #name;
#define SINGLE_TOK(c1, name) \
	case TOK_##name: return #name;
#define DOUBLE_TOK(c1, name1, c2, name2) \
	case TOK_##name1: return #name1; \
	case TOK_##name2: return #name2;
#define TRIPLE_TOK(c1, name1, c2, name2, c3, name3) \
	case TOK_##name1: return #name1; \
	case TOK_##name2: return #name2; \
	case TOK_##name3: return #name3;
#include "tokens.h"
#undef TOK
#undef SINGLE_TOK
#undef DOUBLE_TOK
#undef TRIPLE_TOK
	}
}

const char *tyname(tycls_t ty) {
	switch (ty) {
#define TY(e) case TY_##e: return #e;
TYCLS_T
#undef TY
	}
}

void print_ty(const ty_t *ty) {
	printf("TY_%s %s", tyname(ty->cls), ty->mut ? "mut" : "const");
}

int main(int argc, char **argv) {
	const char *const src = "&[] u8";

	cnm_t cnm = {
		.src = src,
		.tok = {
			.ty = TOK_UNINIT,
			.src.str = src,
		},
		.err = {
			.cb = printf_errcb,
		},
		.filename = "test.cnm",
	};

	//do {
	//	printf("%s\n", tokname(tnext(&cnm)->ty));
	//} while (cnm.tok.ty != TOK_EOF && cnm.err.nerrs == 0);
	
	tnext(&cnm);
	ty_t buf[4];
	size_t n = parsety(&cnm, buf, 4);
	printf("%zu\n", n);
	for (int i = 0; i < n; i++) {
		print_ty(buf + i);
		printf("\n");
	}

	return 0;
}

