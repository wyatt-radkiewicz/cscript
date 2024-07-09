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
bool err_tyref_max(cnms_t *const st) {
	return cnms_error(st, "tyref_max: hit maximum number of types used in source file");
}
bool err_str_max(cnms_t *const st) {
	return cnms_error(st, "str_max: hit maximum number of string views used in source file");
}
bool err_offs_max(cnms_t *const st) {
	return cnms_error(st, "offs_max: hit maximum number of offset caches used in source file");
}
bool err_unknown_token(cnms_t *const st) {
	return cnms_error(st, "unknown_token: source code has unknown token");
}
bool err_any_type(cnms_t *const st) {
	return cnms_error(st, "any_type: you can only use the any type when under indirection");
}
bool err_slice_bracket(cnms_t *const st) {
	return cnms_error(st, "slice_bracket: expected ']'. slices can not have compile time size.");
}
bool err_no_void(cnms_t *const st) {
	return cnms_error(st, "no_void: can not have void type or undefined type here.");
}
bool err_userty_max(cnms_t *const st) {
	return cnms_error(st, "userty_max: hit maximum number of declarable user defined types.");
}
bool err_no_inddn(cnms_t *st, const char *context) {
	char tmp[512];
	strcpy(tmp, "no_inddn: expected identation level to decrease once because ");
	strcat(tmp, context);
	return cnms_error(st, tmp);
}
bool err_expect_ident(cnms_t *const st, const char *const situation) {
	char tmp[512];
	strcpy(tmp, "expect_ident: expected identifier ");
	strcat(tmp, situation);
	return cnms_error(st, tmp);
}
bool err_unknown_ident(cnms_t *const st, const strview_t ident) {
	char tmp[512];
	const strview_t msg = make_strview("unknown_ident: unknown identifier");
	strcpy(tmp, msg.s);
	if (!ident.s || !ident.len || msg.len + ident.len >= arrlen(tmp)) {
		cnms_error(st, tmp);
		return false;
	}

	memcpy(tmp + msg.len, ident.s, ident.len);
	tmp[msg.len + ident.len] = '\0';
	cnms_error(st, tmp);
	return false;
}
bool err_type_size_unknown(cnms_t *const st, const tyref_t ty) {
	char tmp[512];
	const strview_t msg1 = make_strview("type_size_unknown: can not use type ");
	const strview_t msg2 = make_strview(" with unknown or 0 size in this instance");
	strcpy(tmp, msg1.s);
	if (ty.buf[0].inf.cls >= ty_user) {
		const strview_t name = st->userty.buf[ty.buf[0].inf.cls - ty_user].name;
		if (msg1.len + name.len + msg2.len >= arrlen(tmp)) {
			strcat(tmp, msg2.s);
			cnms_error(st, tmp);
			return false;
		}
		memcpy(tmp + msg1.len, name.s, name.len);
		tmp[msg1.len + name.len] = '\0';
		strcat(tmp, msg2.s);
	} else if (ty.buf[0].inf.cls == ty_void) {
		strcat(tmp, "void");
		strcat(tmp, msg2.s);
	} else {
		strcat(tmp, "[error]");
		strcat(tmp, msg2.s);
	}

	cnms_error(st, tmp);
	return false;
}

