#include <ctype.h>

#include "lexer.h"
#include "state.h"

static const char *whitespace(const strview_t src, const char *ptr, const bool skip_newln) {
	while (ptr < src.s + src.len
		&& (*ptr == ' '
			|| *ptr == '\t'
			|| (skip_newln && *ptr == '\n'))) ptr++;
	return ptr;
}

void tok_init(tok_t *const self, const strview_t src) {
	*self = (tok_t){
		.id = tok_uninit,
		.src = {
			.s = src.s,
			.len = 0,
		},
	};
}

tok_t *tok_next(cnms_t *const st, const bool skip_newln) {
	// Find where the end of this token is (depends on token type)
	const char *ptr = st->tok.src.s;
	switch (st->tok.id) {
	case tok_eof: break;
	case tok_str: case tok_char:
		ptr++;
	default:
		ptr += st->tok.src.len;
		break;
	}
	
	// Skip whitespace and set new token location
	ptr = whitespace(st->src, ptr, skip_newln);
	st->tok = (tok_t){
		.id = tok_eof,
		.src = {
			.s = ptr,
			.len = 1,
		},
	};
	if (ptr >= st->src.s + st->src.len) return &st->tok;

	// Get token type and length
	switch (*ptr++) {
	case 'a': case 'A': case 'b': case 'B': case 'c': case 'C': case 'd': case 'D':
	case 'e': case 'E': case 'f': case 'F': case 'g': case 'G': case 'h': case 'H':
	case 'i': case 'I': case 'j': case 'J': case 'k': case 'K': case 'l': case 'L':
	case 'm': case 'M': case 'n': case 'N': case 'o': case 'O': case 'p': case 'P':
	case 'q': case 'Q': case 'r': case 'R': case 's': case 'S': case 't': case 'T':
	case 'u': case 'U': case 'v': case 'V': case 'w': case 'W': case 'y': case 'Y':
	case 'x': case 'X': case 'z': case 'Z': case '_':
		st->tok.id = tok_ident;
		while ((isalnum(*ptr) || *ptr == '_')
			&& ptr < st->src.s + st->src.len) ptr++, st->tok.src.len++;
		break;
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		st->tok.id = tok_int;
		while ((isdigit(*ptr) || *ptr == '.')
			&& ptr < st->src.s + st->src.len) {
			if (*ptr == '.') st->tok.id = tok_fp;
			ptr++, st->tok.src.len++;
		}
		break;
	case '"':
	case '\'':
		st->tok.id = st->tok.src.s[0] == '"' ? tok_str : tok_char;
		while (*ptr != st->tok.src.s[0] &&
			ptr < st->src.s + st->src.len) {
			if (*ptr == '\'' && ptr + 1 < st->src.s + st->src.len) {
				ptr++, st->tok.src.len++;
			}
			ptr++, st->tok.src.len++;
		}
		st->tok.src.s++;
		st->tok.src.len--;
		break;
	case '<':
		if (*ptr == '<') {
			st->tok.src.len = 2;
			st->tok.id = tok_bsl;
		} else if (*ptr == '=') {
			st->tok.src.len = 2;
			st->tok.id = tok_le;
		} else {
			st->tok.id = tok_lt;
		}
		break;
	case '>':
		if (*ptr == '>') {
			st->tok.src.len = 2;
			st->tok.id = tok_bsr;
		} else if (*ptr == '=') {
			st->tok.src.len = 2;
			st->tok.id = tok_ge;
		} else {
			st->tok.id = tok_gt;
		}
		break;
	case '&':
		if (*ptr == '&') {
			st->tok.src.len = 2;
			st->tok.id = tok_and;
		} else {
			st->tok.id = tok_band;
		}
		break;
	case '|':
		if (*ptr == '|') {
			st->tok.src.len = 2;
			st->tok.id = tok_or;
		} else {
			st->tok.id = tok_bor;
		}
		break;
	case '!':
		if (*ptr == '=') {
			st->tok.src.len = 2;
			st->tok.id = tok_neq;
		} else {
			st->tok.id = tok_not;
		}
		break;
	case '=':
		if (*ptr == '=') {
			st->tok.src.len = 2;
			st->tok.id = tok_eqeq;
		} else {
			st->tok.id = tok_eq;
		}
		break;
	case '.': st->tok.id = tok_dot; break;
	case ',': st->tok.id = tok_comma; break;
	case '^': st->tok.id = tok_bxor; break;
	case '~': st->tok.id = tok_bnot; break;
	case '+': st->tok.id = tok_plus; break;
	case '-': st->tok.id = tok_dash; break;
	case '/': st->tok.id = tok_div; break;
	case '*': st->tok.id = tok_star; break;
	case '%': st->tok.id = tok_mod; break;
	case '(': st->tok.id = tok_lpar; break;
	case ')': st->tok.id = tok_rpar; break;
	case '[': st->tok.id = tok_lbrk; break;
	case ']': st->tok.id = tok_rbrk; break;
	case '{': st->tok.id = tok_lbra; break;
	case '}': st->tok.id = tok_rbra; break;
	case '\n': st->tok.id = tok_newln; break;
	case '\0': break;
	default:
		err_unknown_token(st);
		return NULL;
	}

	return &st->tok;
}

#ifndef NDEBUG
#define X(NAME) case NAME: return #NAME;
const char *tokid_tostr(const tokid_t id) {
	switch (id) {
	tokid_xmacro
	default: return NULL;
	}
}
#undef X
#endif

