#ifndef _lexer_h_
#define _lexer_h_

#include "common.h"

#define tokid_xmacro \
	X(tok_uninit) \
	X(tok_indup)	X(tok_inddn) \
	X(tok_eof)	X(tok_ident)	X(tok_int)	X(tok_str) \
	X(tok_char)	X(tok_newln)	X(tok_fp) \
	\
	X(tok_dot) \
	X(tok_comma) \
	X(tok_and)	X(tok_or)	X(tok_not)	X(tok_bxor) \
	X(tok_bnot)	X(tok_band)	X(tok_bor)	X(tok_bsl) \
	X(tok_bsr) \
	X(tok_eq)	X(tok_eqeq)	X(tok_neq)	X(tok_le) \
	X(tok_lt)	X(tok_ge)	X(tok_gt) \
	X(tok_plus)	X(tok_dash)	X(tok_div)	X(tok_star) \
	X(tok_mod) \
	X(tok_lpar)	X(tok_rpar) \
	X(tok_lbrk)	X(tok_rbrk) \
	X(tok_lbra)	X(tok_rbra)

#define X(NAME) NAME,
typedef enum {
	tokid_xmacro
	tok_max,
} tokid_t;
#undef X

typedef struct {
	tokid_t id;
	strview_t src;
} tok_t;

void tok_init(tok_t *self, strview_t src);

tok_t *tok_next(cnms_t *st, bool skip_newln);

static inline bool tok_isnewln(const tok_t tok) {
	return tok.id == tok_newln || tok.id == tok_eof;
}
static inline bool tok_isinddn(const tok_t tok) {
	return tok.id == tok_inddn || tok.id == tok_eof;
}

#ifndef NDEBUG
const char *tokid_tostr(tokid_t id);
#endif

#endif

