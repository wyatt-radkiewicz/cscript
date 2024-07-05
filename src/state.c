#include "state.h"

bool cnms_error(cnms_t *const st, const char *const msg) {
	st->err.ok = false;
	if (!st->err.fn) return false;

	int line = 1;
	for (const char *ptr = st->src.s; ptr < st->tok.src.s; ptr++) {
		line += *ptr == '\n';
	}

	st->err.fn(st->err.user, line, msg);
	return false;
}

bool err_tybuf_max(cnms_t *const st) {
	return cnms_error(st, "tybuf_max: hit buffer limit on type information");
}
bool err_unknown_token(cnms_t *const st) {
	return cnms_error(st, "unknown_token: source code has unknown token");
}
bool err_any_type(cnms_t *st) {
	return cnms_error(st, "any_type: you can only use the any type when under indirection");
}
bool err_slice_bracket(cnms_t *st) {
	return cnms_error(st, "slice_bracket: expected ']'. slices can not have compile time size.");
}
bool err_no_void(cnms_t *st) {
	return cnms_error(st, "no_void: can not have void type here.");
}
bool err_unknown_ident(cnms_t *st, strview_t ident) {
	char tmp[512];
	const char *const msg = "unknown_ident: unknown identifier";
	const size_t msglen = strlen(msg);
	strcpy(tmp, msg);
	if (!ident.s || !ident.len || msglen + ident.len >= arrlen(tmp)) {
		cnms_error(st, tmp);
		return false;
	}

	memcpy(tmp + msglen, ident.s, ident.len);
	tmp[msglen + ident.len] = '\0';
	cnms_error(st, tmp);
	return false;
}

