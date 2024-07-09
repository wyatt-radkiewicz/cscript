#include <stdalign.h>

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
			return cnms_error(st, "expected a pointer to function parameter type");
		}
		nparams->id++;
		if (st->tok.id == tok_comma) tok_next(st, false);
	}
	tok_next(st, false);
	if (!type_parse_inner(st, ref, size, true)) {
		return cnms_error(st, "expected a pointer to function return type");
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
	for (int i = 0; i < st->userty.len; i++) {
		if (!strview_eq(st->tok.src, st->userty.buf[i].name)) continue;
		const userty_t *const user = st->userty.buf + i;

		if (user->ty == userty_fdecl &&
			!last_is_indr && !accept_void) {
			return err_no_void(st);
		} else if (user->ty != userty_typedef) {
			ref->buf[ref->len++].inf.cls = ty_user + i;
			return true;
		}

		// This is a typedef so expand it
		if (ref->len + user->t.len > size) return err_tybuf_max(st);
		memcpy(ref->buf + ref->len, user->t.buf, sizeof(*ref->buf) * user->t.len);
		ref->len += user->t.len;
		return true;
	}

	return err_unknown_ident(st, st->tok.src);
ret:
	tok_next(st, false);
	return true;
}

bool type_eq(const cnms_t *st, const tyref_t lhs, const tyref_t rhs, bool test_quals) {
	if (lhs.len != rhs.len) return false;
	
	for (int i = 0;;i++) {
		if (lhs.buf[i].inf.cls != rhs.buf[i].inf.cls) return false;
		if (test_quals && lhs.buf[i].inf.isconst
			!= rhs.buf[i].inf.isconst) return false;

		switch (lhs.buf[i].inf.cls) {
		case ty_ref: case ty_slice:
		case ty_anyref: case ty_anyslice:
			break;

		case ty_pfn:
		case ty_arr:
			i++;
			if (lhs.buf[i].id != rhs.buf[i].id) return false;
			break;

		default:
			return true;
		}
	}
}

tyinf_t type_info(const cnms_t *st, const tyref_t ty) {
	switch (ty.buf[0].inf.cls) {
	case ty_void: return (tyinf_t){ .size = 0, .align = 1 };
	case ty_i8: return (tyinf_t){ .size = sizeof(int8_t), .align = alignof(int8_t) };
	case ty_u8: return (tyinf_t){ .size = sizeof(uint8_t), .align = alignof(uint8_t) };
	case ty_i16: return (tyinf_t){ .size = sizeof(int16_t), .align = alignof(int16_t) };
	case ty_u16: return (tyinf_t){ .size = sizeof(uint16_t), .align = alignof(uint16_t) };
	case ty_i32: return (tyinf_t){ .size = sizeof(int32_t), .align = alignof(int32_t) };
	case ty_u32: return (tyinf_t){ .size = sizeof(uint32_t), .align = alignof(uint32_t) };
	case ty_i64: return (tyinf_t){ .size = sizeof(int64_t), .align = alignof(int64_t) };
	case ty_u64: return (tyinf_t){ .size = sizeof(uint64_t), .align = alignof(uint64_t) };
	case ty_f32: return (tyinf_t){ .size = sizeof(float), .align = alignof(float) };
	case ty_f64: return (tyinf_t){ .size = sizeof(double), .align = alignof(double) };
	case ty_ref: return (tyinf_t){ .size = sizeof(void *), .align = alignof(void *) };
	case ty_slice: return (tyinf_t){ .size = sizeof(vmslice_t), .align = alignof(vmslice_t) };
	case ty_pfn: return (tyinf_t){ .size = sizeof(vmpfn_t), .align = alignof(vmpfn_t) };
	case ty_anyref: return (tyinf_t){ .size = sizeof(vmanyref_t), .align = alignof(vmanyref_t) };
	case ty_anyslice: return (tyinf_t){ .size = sizeof(vmanyslice_t), .align = alignof(vmanyslice_t) };
	case ty_arr: {
		tyinf_t inf = type_info(st, (tyref_t){ .buf = ty.buf + 2, .len = ty.len - 2 });
		inf.size *= ty.buf[1].id;
		return inf;
	}
	default: {
		// user types
		const userty_t *const user = st->userty.buf + ty.buf[0].inf.cls - ty_user;
		switch (user->ty) {
		case userty_struct: return user->s.inf;
		case userty_enum: return user->e.inf;
		case userty_fdecl: return (tyinf_t){ .size = 0, .align = 1 };
		case userty_typedef: assert(0 && "unexpanded typedef found");
		}
	}
	}
}

static bool struct_parse(cnms_t *const st, userty_t *const ty, strview_t name) {
	ty->ty = userty_struct;
	ty->s = (struct_t){
		.nmembers = 0,
		.enumid = -1,
		.names = st->str.buf + st->str.len,
		.types = st->tyref.buf + st->tyref.len,
		.offs = st->offs.buf + st->offs.len,
		.inf = {0},
	};

	do {
		if (st->tyref.len == arrlen(st->tyref.buf)) return err_tyref_max(st);
		if (st->str.len == arrlen(st->str.buf)) return err_str_max(st);
		if (st->offs.len == arrlen(st->offs.buf)) return err_offs_max(st);
		
		tyref_t *const memberty = &ty->s.types[ty->s.nmembers];
		*memberty = (tyref_t){
			.buf = st->type.buf + st->type.len,
		};
		if (!type_parse(st, memberty, arrlen(st->type.buf) - st->type.len, false)) return false;
		st->type.len += memberty->len;
		ty->s.names[ty->s.nmembers] = name;

		const tyinf_t inf = type_info(st, *memberty);
		ty->s.inf.size = align_up(ty->s.inf.size, inf.align);
		ty->s.offs[ty->s.nmembers] = ty->s.inf.size;
		ty->s.inf.size += inf.size;
		if (inf.align > ty->s.inf.align) ty->s.inf.align = inf.align;

		st->tyref.len++, st->str.len++, st->offs.len++;
		ty->s.nmembers++;

		// Look for next member name
		if (!tok_isnewln(st->tok)) {
			return cnms_error(st, "expected newline after structure member.");
		}

		if (tok_next(st, false)->id != tok_ident) break;
		name = st->tok.src;
		tok_next(st, false);
	} while (true);

	if (!tok_isinddn(st->tok)) return err_no_inddn(st, "struct def");
	tok_next(st, false);

	ty->s.inf.size = align_up(ty->s.inf.size, ty->s.inf.align);
	return true;
}

static bool enum_parse(cnms_t *const st, userty_t *const ty, strview_t name, const tyref_t idty) {
	ty->ty = userty_enum;
	ty->e = (enum_t){
		.idty = idty,
		.nvariants = 0,
		.inf = {
			.size = 0,
			.align = 1,
		},
		.dataoffs = 0,
	};

	do {
		if (st->userty.len + 1 + ty->e.nvariants == arrlen(st->userty.buf)) return err_userty_max(st);
		
		userty_t *const v = st->userty.buf + st->userty.len + 1 + ty->e.nvariants;
		*v = (userty_t){
			.ty = userty_struct,
			.name = name,
		};
		ty->e.nvariants++;

		// Look for next member name
		if (tok_next(st, false)->id == tok_indup) {
			// This variant has type info associated with it
			if (tok_next(st, false)->id != tok_ident) {
				return err_expect_ident(st, "for type member or variant name");
			}
			const strview_t first_name = st->tok.src;
			tok_next(st, false);
			if (!struct_parse(st, v, first_name)) return false;
			if (v->s.inf.size > ty->e.inf.size) ty->e.inf.size = v->s.inf.size;
			if (v->s.inf.align > ty->e.inf.align) ty->e.inf.align = v->s.inf.align;

			v->s.enumid = ty - st->userty.buf;
			if (st->tok.id != tok_ident) break;
		} else if (st->tok.id == tok_ident) {
			v->s.enumid = ty - st->userty.buf;
		} else if (st->tok.id != tok_ident) {
			break;
		}

		name = st->tok.src;
		if (!tok_isnewln(*tok_next(st, false))) {
			return cnms_error(st, "expected newline after enum variant.");
		}
	} while (true);

	if (!tok_isinddn(st->tok)) return err_no_inddn(st, "enum def");
	tok_next(st, false);

	// Add id int to front
	const tyinf_t idinf = type_info(st, idty);
	ty->e.inf.size = align_up(ty->e.inf.size, ty->e.inf.align);
	ty->e.dataoffs = align_up(idinf.size, ty->e.inf.align);
	ty->e.inf.size += ty->e.dataoffs;

	return true;
}

// Current token points to indent before first variant name/member name
static bool structenum_parse(cnms_t *const st, userty_t *const ty) {
	if (tok_next(st, false)->id != tok_ident) return err_expect_ident(st, "for type member or variant name");
	const strview_t first_name = st->tok.src;
	
	if (tok_next(st, false)->id == tok_newln) {
		if (st->type.len == arrlen(st->type.buf)) return err_tybuf_max(st);
		const tyref_t idty = {
			.buf = st->type.buf + st->type.len++,
			.len = 1,
		};
		idty.buf[0] = (type_t){ .inf.cls = ty_i32 };

		const bool ret = enum_parse(st, ty, first_name, idty);
		if (!ret) return false;
		st->userty.len += st->userty.buf[st->userty.len].e.nvariants + 1;
		return ret;
	} else {
		const bool ret = struct_parse(st, ty, first_name);
		st->userty.len++;
		return ret;
	}
}

bool typedef_parse(cnms_t *const st) {
	if (st->userty.len == arrlen(st->userty.buf)) return err_userty_max(st);
	userty_t *const ty = st->userty.buf + st->userty.len;

	if (tok_next(st, false)->id != tok_ident) return err_expect_ident(st, "for type name");
	ty->name = st->tok.src;

	if (tok_isnewln(*tok_next(st, false))) {
		if (tok_next(st, true)->id != tok_indup) {
			// Add a forward declaration
			ty->ty = userty_fdecl;
			return true;
		}

		// Create either struct or enum
		return structenum_parse(st, ty);
	}

	// Create a typedef
	ty->ty = userty_typedef;
	ty->t.buf = st->type.buf + st->type.len;
	if (!type_parse(st, &ty->t, arrlen(st->type.buf) - st->type.len, true)) return false;
	st->type.len += ty->t.len;

	if (!tok_isnewln(st->tok)) {
		return cnms_error(st, "expected newline after type definition");
	}

	// Parse enum with custom id type
	if (tok_next(st, false)->id == tok_indup) {
		if (ty->t.buf[0].inf.cls < ty_i8 || ty->t.buf[0].inf.cls > ty_f64) {
			return cnms_error(st, "variant id type must be of an integer type");
		}
		if (tok_next(st, false)->id != tok_ident) return err_expect_ident(st, "for variant name");
		const strview_t variant_name = st->tok.src;
		if (!tok_isnewln(*tok_next(st, false))) return cnms_error(st, "variants in enums must be either"
				" empty or structs");
		const bool ret = enum_parse(st, ty, variant_name, ty->t);
		if (!ret) return false;
		st->userty.len += st->userty.buf[st->userty.len].e.nvariants + 1;
		return ret;
	}
	
	st->userty.len++;
	return true;
}

