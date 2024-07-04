#include <stdio.h>

#include "common.h"
#include "state.h"
#include "lexer.h"
#include "type.h"
#include "vm.h"
#include "cnms.h"


//
// Test helper functions
//
static const char *curr_test = NULL;

static bool _perr(const int line, const char *const msg) {
	printf("test_%s [%d]: %s\n", curr_test, line, msg);
	return false;
}
#define perr(M) (_perr(__LINE__, M))

typedef bool (*test_pfn_t)(void);

typedef struct {
	test_pfn_t fn;
	const char *name;
} test_t;

//
// Actual tests
//
static bool test_lexer1(void) {
	static const strview_t src = make_strview("test = 50 >> -9.0\n");
	tok_t tok;
	tok_init(&tok, src);

	// uninit
	if (tok.id != tok_uninit) return perr("expected tok_uninit");
	if (tok.src.s != src.s + 0) return perr("unexpected src location");
	if (tok.src.len != 0) return perr("unexpected src len");

	// test
	if (!tok_next(&tok, src, false)) return false;
	if (tok.id != tok_ident) return perr("expected tok_ident");
	if (tok.src.s != src.s + 0) return perr("unexpected src location");
	if (tok.src.len != 4) return perr("unexpected src len");

	// =
	if (!tok_next(&tok, src, false)) return false;
	if (tok.id != tok_eq) return perr("expected tok_eq");
	if (tok.src.s != src.s + 5) return perr("unexpected src location");
	if (tok.src.len != 1) return perr("unexpected src len");

	// 50
	if (!tok_next(&tok, src, false)) return false;
	if (tok.id != tok_int) return perr("expected tok_int");
	if (tok.src.s != src.s + 7) return perr("unexpected src location");
	if (tok.src.len != 2) return perr("unexpected src len");

	// >>
	if (!tok_next(&tok, src, false)) return false;
	if (tok.id != tok_bsr) return perr("expected tok_bsr");
	if (tok.src.s != src.s + 10) return perr("unexpected src location");
	if (tok.src.len != 2) return perr("unexpected src len");

	// -
	if (!tok_next(&tok, src, false)) return false;
	if (tok.id != tok_dash) return perr("expected tok_dash");
	if (tok.src.s != src.s + 13) return perr("unexpected src location");
	if (tok.src.len != 1) return perr("unexpected src len");

	// 9.0
	if (!tok_next(&tok, src, false)) return false;
	if (tok.id != tok_fp) return perr("expected tok_fp");
	if (tok.src.s != src.s + 14) return perr("unexpected src location");
	if (tok.src.len != 3) return perr("unexpected src len");

	// newln
	if (!tok_next(&tok, src, false)) return false;
	if (tok.id != tok_newln) return perr("expected tok_newln");
	if (tok.src.s != src.s + 18) return perr("unexpected src location");
	if (tok.src.len != 1) return perr("unexpected src len");

	// eof
	if (!tok_next(&tok, src, false)) return false;
	if (tok.id != tok_eof) return perr("expected tok_eof");
	if (tok.src.s != src.s + 19) return perr("unexpected src location");
	if (tok.src.len != 1) return perr("unexpected src len");

	// eof
	if (!tok_next(&tok, src, false)) return false;
	if (tok.id != tok_eof) return perr("expected tok_eof");
	if (tok.src.s != src.s + 19) return perr("unexpected src location");
	if (tok.src.len != 1) return perr("unexpected src len");

	return true;
}

static const test_t tests[] = {
	{ .fn = test_lexer1, .name = "lexer1" },
};

//
// Main test runner
//
int main(const int argc, char **const argv) {
	int npassed = 0;

	for (int i = 0; i < arrlen(tests); i++) {
		curr_test = tests[i].name;
		const bool res = tests[i].fn();

		printf("test %16s: ", curr_test);
		if (res) {
			npassed++;
			printf("[PASS]\n");
		} else {
			printf("[\033[0;31mFAIL\033[0m]\n");
		}
	}

	printf("passed %d/%d\n", npassed, (int)arrlen(tests));

	return 0;
}

