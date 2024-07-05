#include "test_common.h"

static bool test_lexer1(void) {
	cnms_t st = {
		.src = make_strview("test = 50 >> -9.0\n"),
	};
	tok_init(&st.tok, st.src);

	// uninit
	if (st.tok.id != tok_uninit) return perr("expected tok_uninit");
	if (st.tok.src.s != st.src.s + 0) return perr("unexpected src location");
	if (st.tok.src.len != 0) return perr("unexpected src len");

	// test
	if (!tok_next(&st, false)) return false;
	if (st.tok.id != tok_ident) return perr("expected tok_ident");
	if (st.tok.src.s != st.src.s + 0) return perr("unexpected src location");
	if (st.tok.src.len != 4) return perr("unexpected src len");

	// =
	if (!tok_next(&st, false)) return false;
	if (st.tok.id != tok_eq) return perr("expected tok_eq");
	if (st.tok.src.s != st.src.s + 5) return perr("unexpected src location");
	if (st.tok.src.len != 1) return perr("unexpected src len");

	// 50
	if (!tok_next(&st, false)) return false;
	if (st.tok.id != tok_int) return perr("expected tok_int");
	if (st.tok.src.s != st.src.s + 7) return perr("unexpected src location");
	if (st.tok.src.len != 2) return perr("unexpected src len");

	// >>
	if (!tok_next(&st, false)) return false;
	if (st.tok.id != tok_bsr) return perr("expected tok_bsr");
	if (st.tok.src.s != st.src.s + 10) return perr("unexpected src location");
	if (st.tok.src.len != 2) return perr("unexpected src len");

	// -
	if (!tok_next(&st, false)) return false;
	if (st.tok.id != tok_dash) return perr("expected tok_dash");
	if (st.tok.src.s != st.src.s + 13) return perr("unexpected src location");
	if (st.tok.src.len != 1) return perr("unexpected src len");

	// 9.0
	if (!tok_next(&st, false)) return false;
	if (st.tok.id != tok_fp) return perr("expected tok_fp");
	if (st.tok.src.s != st.src.s + 14) return perr("unexpected src location");
	if (st.tok.src.len != 3) return perr("unexpected src len");

	// newln
	if (!tok_next(&st, false)) return false;
	if (st.tok.id != tok_newln) return perr("expected tok_newln");
	if (st.tok.src.s != st.src.s + 17) return perr("unexpected src location");
	if (st.tok.src.len != 1) return perr("unexpected src len");

	// eof
	if (!tok_next(&st, false)) return false;
	if (st.tok.id != tok_eof) return perr("expected tok_eof");
	if (st.tok.src.s != st.src.s + 18) return perr("unexpected src location");
	if (st.tok.src.len != 1) return perr("unexpected src len");

	// eof
	if (!tok_next(&st, false)) return false;
	if (st.tok.id != tok_eof) return perr("expected tok_eof");
	if (st.tok.src.s != st.src.s + 18) return perr("unexpected src location");
	if (st.tok.src.len != 1) return perr("unexpected src len");

	return true;
}

static bool test_lexer2(void) {
	cnms_t st = {
		.src = make_strview("}{][)(%*/-+> >=< <=!====>><<|&~^!||&&,.0.058\n'A'\"hello\"32foo")
	};
	static const intptr_t tok_locs[tok_max] = {
		[tok_uninit] = 0,
		[tok_eof] = 60,		[tok_ident] = 57,	[tok_int] = 55,
		[tok_str] = 49,		[tok_char] = 46,	[tok_newln] = 44,
		[tok_fp] = 39,

		[tok_dot] = 38,		[tok_comma] = 37,
		[tok_and] = 35,		[tok_or] = 33,		[tok_not] = 32,
		[tok_bxor] = 31,	[tok_bnot] = 30,	[tok_band] = 29,
		[tok_bor] = 28,		[tok_bsl] = 26,		[tok_bsr] = 24,
		[tok_eq] = 23,		[tok_eqeq] = 21,	[tok_neq] = 19,
		[tok_le] = 17,		[tok_lt] = 15,		[tok_ge] = 13,
		[tok_gt] = 11,
		[tok_plus] = 10,	[tok_dash] = 9,		[tok_div] = 8,
		[tok_star] = 7,		[tok_mod] = 6,
		[tok_lpar] = 5,		[tok_rpar] = 4,		[tok_lbrk] = 3,
		[tok_rbrk] = 2,		[tok_lbra] = 1,		[tok_rbra] = 0,
	};
	static const intptr_t tok_lens[tok_max] = {
		[tok_uninit] = 0,
		[tok_eof] = 1,		[tok_ident] = 3,	[tok_int] = 2,
		[tok_str] = 5,		[tok_char] = 1,		[tok_newln] = 1,
		[tok_fp] = 5,

		[tok_dot] = 1,		[tok_comma] = 1,
		[tok_and] = 2,		[tok_or] = 2,		[tok_not] = 1,
		[tok_bxor] = 1,		[tok_bnot] = 1,		[tok_band] = 1,
		[tok_bor] = 1,		[tok_bsl] = 2,		[tok_bsr] = 2,
		[tok_eq] = 1,		[tok_eqeq] = 2,		[tok_neq] = 2,
		[tok_le] = 2,		[tok_lt] = 1,		[tok_ge] = 2,
		[tok_gt] = 1,
		[tok_plus] = 1,		[tok_dash] = 1,		[tok_div] = 1,
		[tok_star] = 1,		[tok_mod] = 1,
		[tok_lpar] = 1,		[tok_rpar] = 1,		[tok_lbrk] = 1,
		[tok_rbrk] = 1,		[tok_lbra] = 1,		[tok_rbra] = 1,
	};

	tok_init(&st.tok, st.src);
	for (int i = tok_max - 1; i > tok_uninit; i--) {
		const char *const tokname = tokid_tostr(i);
		if (!tok_next(&st, false)) return false;
		if (st.tok.id != i) return perr("expected token id. needed %s", tokname);
		if (st.tok.src.s != st.src.s + tok_locs[i]) return perr("unexpected src location %s. got %d but needed %d", tokname, st.tok.src.s - st.src.s, tok_locs[i]);
		if (st.tok.src.len != tok_lens[i]) return perr("unexpected src len %s. got %d but needed %d", tokname, st.tok.src.len, tok_lens[i]);
	}

	return true;
}

