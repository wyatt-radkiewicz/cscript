#ifndef _lexer_h_
#define _lexer_h_

#include "common.h"

#define tokty_def \
	/* Keywords */ \
	enumdef(tok_auto) \
	enumdef(tok_bool) \
	enumdef(tok_break) \
	enumdef(tok_case) \
	enumdef(tok_char) \
	enumdef(tok_const) \
	enumdef(tok_continue) \
	enumdef(tok_default) \
	enumdef(tok_do) \
	enumdef(tok_double) \
	enumdef(tok_else) \
	enumdef(tok_enum) \
	enumdef(tok_extern) \
	enumdef(tok_false) \
	enumdef(tok_float) \
	enumdef(tok_for) \
	enumdef(tok_goto) \
	enumdef(tok_if) \
	enumdef(tok_int) \
	enumdef(tok_long) \
	enumdef(tok_nullptr) \
	enumdef(tok_restrict) \
	enumdef(tok_return) \
	enumdef(tok_short) \
	enumdef(tok_signed) \
	enumdef(tok_sizeof) \
	enumdef(tok_static) \
	enumdef(tok_static_assert) \
	enumdef(tok_struct) \
	enumdef(tok_switch) \
	enumdef(tok_true) \
	enumdef(tok_typedef) \
	enumdef(tok_typeof) \
	enumdef(tok_union) \
	enumdef(tok_unsigned) \
	enumdef(tok_void) \
	enumdef(tok_volatile) \
	enumdef(tok_while) \
	\
	enumdef(tok_max_keywords) \
	\
	/* Tokens (first and second chars) */ \
	enumdef(tok_eof) \
	enumdef(tok_plus) \
	enumdef(tok_pluseq) \
	enumdef(tok_minus) \
	enumdef(tok_minuseq) \
	enumdef(tok_star) \
	enumdef(tok_dot) \
	enumdef(tok_eq) \
	enumdef(tok_eqeq) \
	enumdef(tok_bitand) \
	enumdef(tok_and) \
	enumdef(tok_bitor) \
	enumdef(tok_or) \
	enumdef(tok_bitnot) \
	enumdef(tok_not) \
	enumdef(tok_noteq) \
	enumdef(tok_xor) \
	enumdef(tok_lparen) \
	enumdef(tok_rparen) \
	enumdef(tok_lbrack) \
	enumdef(tok_rbrack) \
	enumdef(tok_lbrace) \
	enumdef(tok_rbrace) \
	enumdef(tok_gt) \
	enumdef(tok_ge) \
	enumdef(tok_shr) \
	enumdef(tok_lt) \
	enumdef(tok_le) \
	enumdef(tok_shl) \
	enumdef(tok_comma) \
	enumdef(tok_char_lit) \
	enumdef(tok_semicol) \
	enumdef(tok_slash) \
	enumdef(tok_percent) \
	\
	/* Misc tokens */ \
	enumdef(tok_string) \
	enumdef(tok_integer) \
	enumdef(tok_floating) \
	enumdef(tok_ident) \
	enumdef(tok_error) \
	enumdef(tok_undefined)

// First letter of each token is here
typedef enum tokty {
#define enumdef(e) e,
tokty_def
#undef enumdef
} tokty_t;

typedef struct tok {
	tokty_t ty;
	const char *lit;
	size_t len, line, chr;
	err_t err;
} tok_t;

typedef struct keyword {
	const char *str;
	size_t len;
	uint32_t hash;
	unsigned int psl;
	tokty_t ty;
} keyword_t;

struct lexer {
	size_t line, chr, head_line, head_chr;
	const char *head;

	union {
		struct {
			tok_t prev[3], curr, peek[3];
		};
		tok_t toks[7];
	};
};

#define make_errtok(state, _ty, _msg) ((tok_t){ \
	.ty = tok_error, \
	.lit = NULL, \
	.len = 0, \
	.line = (state)->line, \
	.chr = (state)->chr, \
	.err.ty = _ty, \
	.err.msg = _msg, \
})

// No entropy on top bits. Not needed since in the implementation, the
// hashmap for keywords is less than 256.
inline static uint32_t ident_hash(const char *str, const size_t len) {
	return ((str[0] * 0x92) ^ str[1]) ^ (str[len - 1] * 0x13 - str[len - 2]);
}

lexer_t lexer_init(const char *script);
bool lexer_next(lexer_t *const state);

const char *tokty_to_str(const tokty_t ty);
void dbg_tok_print(const tok_t tok);

#endif

