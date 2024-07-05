#include "state.h"
#include "type.h"

static bool type_parse_inner(cnms_t *const st, tyref_t *const ref, const size_t size, const bool accept_void);

static bool pfn_parse(cnms_t *const st, tyref_t *const ref, const size_t size) {
	ref->buf[ref->len++].inf.cls = ty_pfn;
	if (ref->len + 1 >= size) return err_tybuf_max(st);
	type_t *nparams = &ref->buf[ref->len++];
	nparams->id = 0;

	tok_next(st, false);
	while (st->tok.id != tok_rpar) {
		if (!type_parse_inner(st, ref, size, false)) {
			cnms_error(st, "expected a pointer to function parameter type");
			return false;
		}
		nparams->id++;
		if (st->tok.id == tok_comma) tok_next(st, false);
	}
	tok_next(st, false);
	if (!type_parse_inner(st, ref, size, true)) {
		cnms_error(st, "expected a pointer to function return type");
		return false;
	}

	return true;
}

bool type_parse(cnms_t *const st, tyref_t *const ref, const size_t size, const bool accept_void) {
	ref->len = 0;

	return type_parse_inner(st, ref, size, accept_void);
}

static bool type_parse_inner(cnms_t *const st, tyref_t *const ref, const size_t size, const bool accept_void) {
	if (ref->len == size) return err_tybuf_max(st);
	ref->buf[ref->len] = (type_t){ .inf.cls = ty_void };

	// Parse refrences, consts and slices
	bool last_is_indr = false;
	while (true) {
		if (st->tok.id == tok_band || st->tok.id == tok_and) {
			if (st->tok.id == tok_and) {
				ref->buf[ref->len++].inf.cls = ty_ref;
				if (ref->len == size) return err_tybuf_max(st);
				ref->buf[ref->len] = (type_t){0};
			}

			if (tok_next(st, false)->id == tok_lbrk) {
				if (tok_next(st, false)->id != tok_rbrk) return err_slice_bracket(st);
				ref->buf[ref->len++].inf.cls = ty_slice;
				tok_next(st, false);
			} else {
				ref->buf[ref->len++].inf.cls = ty_ref;
			}
			if (ref->len == size) return err_tybuf_max(st);
			ref->buf[ref->len] = (type_t){ .inf.cls = ty_void };
			last_is_indr = true;
		} else if (st->tok.id == tok_lbrk) {
			last_is_indr = false;
			assert(0 && "implement arrays");
		} else if (st->tok.id == tok_ident && strview_eq(st->tok.src, make_strview("const"))) {
			ref->buf[ref->len].inf.isconst = true;
			tok_next(st, false);
		} else {
			break;
		}
	}

	// Parse pointer to function
	if (st->tok.id == tok_lpar) return pfn_parse(st, ref, size);

	if (st->tok.id != tok_ident) return err_unknown_ident(st, st->tok.src);

	// Try POD types
	switch (st->tok.src.s[0]) {
	case 'i':
	case 'u': {
		bool u = st->tok.src.s[0] == 'u';
		if (st->tok.src.len == 2 && st->tok.src.s[1] == '8') {
			ref->buf[ref->len++].inf.cls = u ? ty_u8 : ty_i8;
			goto ret;
		} else if (st->tok.src.len == 3 && st->tok.src.s[1] == '1'
			&& st->tok.src.s[2] == '6') {
			ref->buf[ref->len++].inf.cls = u ? ty_u16 : ty_i16;
			goto ret;
		} else if (st->tok.src.len == 3 && st->tok.src.s[1] == '3'
			&& st->tok.src.s[2] == '2') {
			ref->buf[ref->len++].inf.cls = u ? ty_u32 : ty_i32;
			goto ret;
		} else if (st->tok.src.len == 3 && st->tok.src.s[1] == '6'
			&& st->tok.src.s[2] == '4') {
			ref->buf[ref->len++].inf.cls = u ? ty_u64 : ty_i64;
			goto ret;
		}
		break;
	}
	case 'f':
		if (st->tok.src.len == 3 && st->tok.src.s[1] == '3'
			&& st->tok.src.s[2] == '2') {
			ref->buf[ref->len++].inf.cls = ty_f32;
			goto ret;
		} else if (st->tok.src.len == 3 && st->tok.src.s[1] == '6'
			&& st->tok.src.s[2] == '4') {
			ref->buf[ref->len++].inf.cls = ty_f64;
			goto ret;
		}
		break;
	case 'a':
		if (st->tok.src.len == 3 && st->tok.src.s[1] == 'n'
			&& st->tok.src.s[2] == 'y') {
			if (!last_is_indr) return err_any_type(st);
			ref->buf[ref->len - 1].inf.isconst = ref->buf[ref->len].inf.isconst;
			if (ref->buf[ref->len - 1].inf.cls == ty_ref) {
				ref->buf[ref->len - 1].inf.cls = ty_anyref;
			} else {
				ref->buf[ref->len - 1].inf.cls = ty_anyslice;
			}
			goto ret;
		}
	case 'v':
		if (st->tok.src.len == 4 && st->tok.src.s[1] == 'o'
			&& st->tok.src.s[2] == 'i' && st->tok.src.s[3] == 'd') {
			if (!accept_void) return err_no_void(st);
			ref->buf[ref->len++].inf.cls = ty_void;
			goto ret;
		}
	}

	// Try user made types

	return err_unknown_ident(st, st->tok.src);
ret:
	tok_next(st, false);
	return true;
}

