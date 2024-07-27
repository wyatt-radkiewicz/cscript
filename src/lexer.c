#include <ctype.h>

#include "common.h"

static void whitespace(cnm_t *cnm) {
	for (; isspace(cnm->tok.src.str[0]); ++cnm->tok.src.str);
}

static const tok_t *lexident(cnm_t *cnm) {
	cnm->tok.ty = TOK_IDENT;
	for (; isalnum(cnm->tok.src.str[cnm->tok.src.len])
		|| cnm->tok.src.str[cnm->tok.src.len] == '_'
		; ++cnm->tok.src.len);
	return &cnm->tok;
}

static const tok_t *lexnum(cnm_t *cnm) {
	cnm->tok.ty = TOK_NUM;
	for (; isalnum(cnm->tok.src.str[cnm->tok.src.len])
		|| cnm->tok.src.str[cnm->tok.src.len] == '.'
		; ++cnm->tok.src.len);
	return &cnm->tok;
}

static const tok_t *lexstrchr(cnm_t *cnm) {
	cnm->tok.ty = cnm->tok.src.str[0] == '\'' ? TOK_CHR : TOK_STR;
	for (; cnm->tok.src.str[cnm->tok.src.len] != cnm->tok.src.str[0]
		; ++cnm->tok.src.len) {
		if (cnm->tok.src.str[cnm->tok.src.len] == '\0') {
			errinf_t errinf = {
				.area = {
					.str = cnm->tok.src.str,
					.len = cnm->tok.src.len,
				},
				.critical = true,
			};
			if (cnm->tok.ty == TOK_CHR)
				strcpy(errinf.comment, "missing corresponding \'");
			else
				strcpy(errinf.comment, "missing corresponding \"");

			doerr(cnm, 2, cnm->tok.ty == TOK_CHR ?
				"unexpected eof, when parsing character token"
				: "unexpected eof, when parsing string token",
				true, &errinf, 1);
			cnm->tok.ty = TOK_EOF;
			return &cnm->tok;
		}
	}
	cnm->tok.src.len++;
	return &cnm->tok;
}

static const tok_t *lexunknown(cnm_t *cnm) {
	doerr(cnm, 1, "unexpected character while parsing token, skipping token...",
	false, &(errinf_t){
		.area = {
			.str = cnm->tok.src.str,
			.len = 1,
		},
		.comment = "no token starts with this character",
		.critical = false,
	}, 1);

	for (; !isspace(cnm->tok.src.str[0]) && cnm->tok.src.str[0] != '\0';
		cnm->tok.src.str++);
	
	return tnext(cnm);
}

const tok_t *tnext(cnm_t *cnm) {
	if (cnm->tok.ty == TOK_EOF) return &cnm->tok;
	cnm->tok.src.str += cnm->tok.src.len;
	cnm->tok.src.len = 1;
	whitespace(cnm);

	switch (cnm->tok.src.str[0]) {
	case '\'': case '\"':
		return lexstrchr(cnm);
	default:
		if (isalpha(cnm->tok.src.str[0]) || cnm->tok.src.str[0] == '_')
			return lexident(cnm);
		else if (isdigit(cnm->tok.src.str[0]))
			return lexnum(cnm);
		else
			return lexunknown(cnm);
#define TOK(name)
#define SINGLE_TOK(c1, name) \
case c1: \
	cnm->tok.ty = TOK_##name; \
	return &cnm->tok;
#define DOUBLE_TOK(c1, name1, c2, name2) \
case c1: \
	cnm->tok.ty = TOK_##name1; \
	if (cnm->tok.src.str[1] == c2) { \
		cnm->tok.ty = TOK_##name2; \
		cnm->tok.src.len = 2; \
	} \
	return &cnm->tok;
#define TRIPLE_TOK(c1, name1, c2, name2, c3, name3) \
case c1: \
	cnm->tok.ty = TOK_##name1; \
	if (cnm->tok.src.str[1] == c2) { \
		cnm->tok.ty = TOK_##name2; \
		cnm->tok.src.len = 2; \
	} else if (cnm->tok.src.str[1] == c3) { \
		cnm->tok.ty = TOK_##name3; \
		cnm->tok.src.len = 2; \
	} \
	return &cnm->tok;
#include "tokens.h"
#undef TOK
#undef SINGLE_TOK
#undef DOUBLE_TOK
#undef TRIPLE_TOK
	}
}

