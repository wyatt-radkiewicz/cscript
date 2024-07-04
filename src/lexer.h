#ifndef _lexer_h_
#define _lexer_h_

#include "common.h"

typedef enum {
	tok_uninit,
	tok_eof,	tok_ident,	tok_int,	tok_str,
	tok_char,	tok_newln,	tok_fp,

	tok_dot,
	tok_comma,
	tok_and,	tok_or,		tok_not,	tok_bxor,
	tok_bnot,	tok_band,	tok_bor,	tok_bsl,
	tok_bsr,
	tok_eq,		tok_eqeq,	tok_neq,	tok_le,
	tok_lt,		tok_ge,		tok_gt,
	tok_plus,	tok_dash,	tok_div,	tok_star,
	tok_mod,
	tok_lpar,	tok_rpar,
	tok_lbrk,	tok_rbrk,
	tok_lbra,	tok_rbra,

	tok_max,
} tokid_t;

typedef struct {
	tokid_t id;
	strview_t src;
} tok_t;

void tok_init(tok_t *self, strview_t src);

tok_t *tok_next(tok_t *self, strview_t src, bool skip_newln);

#endif

