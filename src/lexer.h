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
	enumdef(tok_plusplus) \
	enumdef(tok_pluseq) \
	enumdef(tok_minus) \
	enumdef(tok_minusminus) \
	enumdef(tok_minuseq) \
	enumdef(tok_star) \
	enumdef(tok_stareq) \
	enumdef(tok_dot) \
	enumdef(tok_arrow) \
	enumdef(tok_eq) \
	enumdef(tok_eqeq) \
	enumdef(tok_bitand) \
	enumdef(tok_bitandeq) \
	enumdef(tok_and) \
	enumdef(tok_bitor) \
	enumdef(tok_bitoreq) \
	enumdef(tok_or) \
	enumdef(tok_bitnot) \
	enumdef(tok_not) \
	enumdef(tok_noteq) \
	enumdef(tok_xor) \
	enumdef(tok_xoreq) \
	enumdef(tok_lparen) \
	enumdef(tok_rparen) \
	enumdef(tok_lbrack) \
	enumdef(tok_rbrack) \
	enumdef(tok_lbrace) \
	enumdef(tok_rbrace) \
	enumdef(tok_gt) \
	enumdef(tok_ge) \
	enumdef(tok_shr) \
	enumdef(tok_shreq) \
	enumdef(tok_lt) \
	enumdef(tok_le) \
	enumdef(tok_shl) \
	enumdef(tok_shleq) \
	enumdef(tok_comma) \
	enumdef(tok_char_lit) \
	enumdef(tok_semicol) \
	enumdef(tok_slash) \
	enumdef(tok_slasheq) \
	enumdef(tok_percent) \
	enumdef(tok_percenteq) \
	enumdef(tok_qmark) \
	enumdef(tok_colon) \
	enumdef(tok_elipses) \
	enumdef(tok_hash) \
	enumdef(tok_doublehash) \
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

typedef struct macro_param {
	const char *name;
	int len;
} macro_param_t;

#define MAX_MACRO_PARAMS 32

typedef struct macro {
	const char *name, *start;
	int namelen, len;
	size_t start_line, start_chr;
	macro_param_t params[MAX_MACRO_PARAMS];
	int nparams;
} macro_t;

typedef struct lexerlvl {
	size_t line, chr, head_line, head_chr;
	const char *head, *end;
	int macro_exp;
	macro_param_t params[MAX_MACRO_PARAMS];
	int nparams;
} lexerlvl_t;

typedef const char *(*lexer_include_pfn)(const char *path);

struct lexer {
	tok_t prev, curr, peek;
	lexerlvl_t lvls[16];
	int lvl;

	lexer_include_pfn incfn;
	char concat_buf[512]; // Buffer for concatinated stuff
	int concat_sz;
	macro_t macros[256];
	ident_map_t map;
	ident_ent_t ents[255];
};

#define make_errtok(state, lvl, _ty, _msg) ((tok_t){ \
	.ty = tok_error, \
	.lit = NULL, \
	.len = 0, \
	.line = (lvl)->line, \
	.chr = (lvl)->chr, \
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

