#include <ctype.h>

#include "lexer.h"

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

tok_t *tok_next(tok_t *const self, const strview_t src, const bool skip_newln) {
	const char *ptr = self->src.s + self->src.len;
	
	ptr = whitespace(src, ptr, skip_newln);
	if (ptr >= src.s + src.len) {
		self->id = tok_eof;
		return self;
	}

	self->src = (strview_t){ .s = ptr, .len = 1 };

	switch (*ptr++) {
	case 'a': case 'A': case 'b': case 'B': case 'c': case 'C': case 'd': case 'D':
	case 'e': case 'E': case 'f': case 'F': case 'g': case 'G': case 'h': case 'H':
	case 'i': case 'I': case 'j': case 'J': case 'k': case 'K': case 'l': case 'L':
	case 'm': case 'M': case 'n': case 'N': case 'o': case 'O': case 'p': case 'P':
	case 'q': case 'Q': case 'r': case 'R': case 's': case 'S': case 't': case 'T':
	case 'u': case 'U': case 'v': case 'V': case 'w': case 'W': case 'y': case 'Y':
	case 'x': case 'X': case 'z': case 'Z': case '_':
		self->id = tok_ident;
		while ((isalnum(*ptr) || *ptr == '_')
			&& ptr < src.s + src.len) ptr++, self->src.len++;
		break;
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		self->id = tok_int;
		while ((isdigit(*ptr) || *ptr == '.')
			&& ptr < src.s + src.len) {
			if (*ptr == '.') self->id = tok_fp;
			ptr++, self->src.len++;
		}
		break;
	case '"':
	case '\'':
		self->id = self->src.s[0] == '"' ? tok_str : tok_char;
		while (*ptr != self->src.s[0] &&
			ptr < src.s + src.len) {
			if (*ptr == '\'' && ptr + 1 < src.s + src.len) {
				ptr++, self->src.len++;
			}
			ptr++, self->src.len++;
		}
		self->src.s++;
		self->src.len -= 2;
		break;
	case '<':
		if (*ptr == '<') {
			self->src.len = 2;
			self->id = tok_bsl;
		} else if (*ptr == '=') {
			self->src.len = 2;
			self->id = tok_le;
		} else {
			self->id = tok_lt;
		}
		break;
	case '>':
		if (*ptr == '>') {
			self->src.len = 2;
			self->id = tok_bsr;
		} else if (*ptr == '=') {
			self->src.len = 2;
			self->id = tok_ge;
		} else {
			self->id = tok_gt;
		}
		break;
	case '&':
		if (*ptr == '&') {
			self->src.len = 2;
			self->id = tok_and;
		} else {
			self->id = tok_band;
		}
		break;
	case '|':
		if (*ptr == '|') {
			self->src.len = 2;
			self->id = tok_or;
		} else {
			self->id = tok_bor;
		}
		break;
	case '!':
		if (*ptr == '=') {
			self->src.len = 2;
			self->id = tok_neq;
		} else {
			self->id = tok_not;
		}
		break;
	case '=':
		if (*ptr == '=') {
			self->src.len = 2;
			self->id = tok_eqeq;
		} else {
			self->id = tok_eq;
		}
		break;
	case '\0': self->id = tok_eof; break;
	case '\n': self->id = tok_newln; break;
	case '.': self->id = tok_dot; break;
	case ',': self->id = tok_comma; break;
	case '^': self->id = tok_bxor; break;
	case '~': self->id = tok_bnot; break;
	case '+': self->id = tok_plus; break;
	case '-': self->id = tok_dash; break;
	case '/': self->id = tok_div; break;
	case '*': self->id = tok_star; break;
	case '%': self->id = tok_mod; break;
	case '(': self->id = tok_lpar; break;
	case ')': self->id = tok_rpar; break;
	case '[': self->id = tok_lbrk; break;
	case ']': self->id = tok_rbrk; break;
	case '{': self->id = tok_lbra; break;
	case '}': self->id = tok_rbra; break;
	default:
		return NULL;
	}

	return self;
}

