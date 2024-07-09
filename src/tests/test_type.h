#include "test_common.h"

#define setup_test_type(NAME, EXPECT_ERROR, S, ...)	\
static bool NAME(void) {				\
	cnms_t st = {					\
		.src = make_strview(S),			\
		.err = {				\
			.fn = error_handler,		\
			.ok = true,			\
		},					\
	};						\
	type_t expected[] = { __VA_ARGS__ };		\
	type_t buf[32];					\
							\
	tyref_t ref = { .buf = buf, .len = 0 };		\
	tok_init(&st.tok, st.src);			\
	tok_next(&st, false);				\
	if (EXPECT_ERROR) {				\
		if (type_parse(&st, &ref, arrlen(buf), false)) return perr("expected compile error");	\
	} else {					\
		if (!type_parse(&st, &ref, arrlen(buf), false)) return false;				\
		if (arrlen(expected) != ref.len) return perr("type lengths don't match");		\
		if (memcmp(buf, expected, sizeof(expected))) return perr("types don't match");		\
	}						\
							\
	return true;					\
}

setup_test_type(test_type1, false, "i8", { .inf.cls = ty_i8 })
setup_test_type(test_type2, false, "&i8", { .inf.cls = ty_ref }, { .inf.cls = ty_i8 })
setup_test_type(test_type3, false, "&[]i8", { .inf.cls = ty_slice }, { .inf.cls = ty_i8 })
setup_test_type(test_type4, false, "&[]any", { .inf.cls = ty_anyslice })
setup_test_type(test_type5, false, "i16", { .inf.cls = ty_i16 })
setup_test_type(test_type6, false, "i32", { .inf.cls = ty_i32 })
setup_test_type(test_type7, false, "i64", { .inf.cls = ty_i64 })
setup_test_type(test_type8, false, "u8", { .inf.cls = ty_u8 })
setup_test_type(test_type9, false, "u16", { .inf.cls = ty_u16 })
setup_test_type(test_type10, false, "u32", { .inf.cls = ty_u32 })
setup_test_type(test_type11, false, "u64", { .inf.cls = ty_u64 })
setup_test_type(test_type12, false, "f32", { .inf.cls = ty_f32 })
setup_test_type(test_type13, false, "f64", { .inf.cls = ty_f64 })
setup_test_type(test_type14, false, "&any", { .inf.cls = ty_anyref })
setup_test_type(test_type15, false, "&&any", { .inf.cls = ty_ref }, { .inf.cls = ty_anyref })
setup_test_type(test_type16, false, "const f64", { .inf.cls = ty_f64, .inf.isconst = true })
setup_test_type(test_type17, false, "&[] & const any", { .inf.cls = ty_slice }, { .inf.cls = ty_anyref, .inf.isconst = true })
setup_test_type(test_type18, false, "(i32) void", { .inf.cls = ty_pfn }, { .id = 1 },
	{ .inf.cls = ty_i32 }, { .inf.cls = ty_void })
setup_test_type(test_type19, false, "(i32, &u8) void", { .inf.cls = ty_pfn }, { .id = 2 },
	{ .inf.cls = ty_i32 }, { .inf.cls = ty_ref }, { .inf.cls = ty_u8 }, { .inf.cls = ty_void })
setup_test_type(test_type20, false, "(i32, &u8, &any) void", { .inf.cls = ty_pfn }, { .id = 3 },
	{ .inf.cls = ty_i32 }, { .inf.cls = ty_ref }, { .inf.cls = ty_u8 },
	{ .inf.cls = ty_anyref }, { .inf.cls = ty_void })
setup_test_type(test_type21, true, "any", { .inf.cls = ty_void })
setup_test_type(test_type22, true, "void", { .inf.cls = ty_void })

static bool init_typedef_test(cnms_t *const st, const char *const msg) {
	*st = (cnms_t){
		.src = { .s = msg, .len = strlen(msg) },
		.err = {
			.fn = error_handler,
			.ok = true,
		},
	};
	
	tok_init(&st->tok, st->src);
	tok_next(st, false);
	return typedef_parse(st);
}

static bool test_typedef1(void) {
	cnms_t st;
	if (!init_typedef_test(&st,
		"type Foo &i32")) return false;
	if (st.type.len != 2) return perr("expected matching type buf lengths");
	if (st.userty.len != 1 || st.userty.buf[0].ty != userty_typedef ||
		!strview_eq(st.userty.buf[0].name, make_strview("Foo"))) {
		return perr("expected typename of Foo");
	}
	if (st.tyref.len != 0 || !type_eq(&st, st.userty.buf[0].t, (tyref_t){
		.buf = (type_t[]){ (type_t){ .inf.cls = ty_ref },
				(type_t){ .inf.cls = ty_i32 }, },
		.len = 2,
	}, true)) {
		return perr("expected matching type refs");
	}

	return true;
}

static bool test_typedef2(void) {
	cnms_t st;
	if (!init_typedef_test(&st,
		"type Foo i8\n"
		"\tFooA\n"
		"\t\tx i32\n"
		"\t\ty i32\n"
		"\tFooB\n"
		"\tFooC\n")) return false;
	if (st.type.len != 3) return perr("expected matching type buf lengths");
	if (st.userty.len != 4 || st.userty.buf[0].ty != userty_enum ||
		!strview_eq(st.userty.buf[0].name, make_strview("Foo"))) {
		return perr("expected typename of Foo");
	}
	if (st.tyref.len != 2) return perr("expected matching type refs");
	if (!type_eq(&st, st.userty.buf[0].e.idty, (tyref_t){
		.buf = (type_t[]){ (type_t){ .inf.cls = ty_i8 } },
		.len = 1,
	}, true)) return perr("expected matching types");
	if (!type_eq(&st, st.userty.buf[1].s.types[0], (tyref_t){
		.buf = (type_t[]){ (type_t){ .inf.cls = ty_i32 } },
		.len = 1,
	}, true)) return perr("expected matching types");
	if (!type_eq(&st, st.userty.buf[1].s.types[1], (tyref_t){
		.buf = (type_t[]){ (type_t){ .inf.cls = ty_i32 } },
		.len = 1,
	}, true)) return perr("expected matching types");

	return true;
}
static bool test_typedef3(void) {
	cnms_t st;
	if (!init_typedef_test(&st,
		"type Foo\n"
		"\tFooA\n"
		"\t\tx i32\n"
		"\t\ty i32\n"
		"\tFooB\n"
		"\tFooC\n"
		"\t\trad f64\n")) return false;
	if (st.type.len != 4) return perr("expected matching type buf lengths");
	if (st.userty.len != 4 || st.userty.buf[0].ty != userty_enum ||
		!strview_eq(st.userty.buf[0].name, make_strview("Foo"))) {
		return perr("expected typename of Foo");
	}
	if (st.tyref.len != 3) return perr("expected matching type refs");
	if (!type_eq(&st, st.userty.buf[1].s.types[0], (tyref_t){
		.buf = (type_t[]){ (type_t){ .inf.cls = ty_i32 } },
		.len = 1,
	}, true)) return perr("expected matching types");
	if (!type_eq(&st, st.userty.buf[1].s.types[1], (tyref_t){
		.buf = (type_t[]){ (type_t){ .inf.cls = ty_i32 } },
		.len = 1,
	}, true)) return perr("expected matching types");
	if (!type_eq(&st, st.userty.buf[3].s.types[0], (tyref_t){
		.buf = (type_t[]){ (type_t){ .inf.cls = ty_f64 } },
		.len = 1,
	}, true)) return perr("expected matching types");

	return true;
}
static bool test_typedef4(void) {
	cnms_t st;
	if (!init_typedef_test(&st,
		"type Foo\n"
		"\tx i32\n"
		"\ty i32\n")) return false;
	if (st.type.len != 2) return perr("expected matching type buf lengths");
	if (st.userty.len != 1 || st.userty.buf[0].ty != userty_struct ||
		!strview_eq(st.userty.buf[0].name, make_strview("Foo"))) {
		return perr("expected typename of Foo");
	}
	if (st.tyref.len != 2) return perr("expected matching type refs");
	if (st.userty.buf[0].s.nmembers != 2) return perr("nmembers not correct!");
	if (!type_eq(&st, st.userty.buf[0].s.types[0], (tyref_t){
		.buf = (type_t[]){ (type_t){ .inf.cls = ty_i32 } },
		.len = 1,
	}, true)) return perr("expected matching types");
	if (!type_eq(&st, st.userty.buf[0].s.types[1], (tyref_t){
		.buf = (type_t[]){ (type_t){ .inf.cls = ty_i32 } },
		.len = 1,
	}, true)) return perr("expected matching types");
	if (!strview_eq(st.userty.buf[0].s.names[0], make_strview("x"))) {
		return perr("expected matching names");
	}
	if (!strview_eq(st.userty.buf[0].s.names[1], make_strview("y"))) {
		return perr("expected matching names");
	}

	return true;
}


#define setup_test_typeeq(NAME, ST, T)					\
static bool NAME(void) {						\
	cnms_t st = ST;							\
	if (!type_eq(&st,						\
		(tyref_t){ .buf = T, .len = arrlen(T)},			\
		(tyref_t){ .buf = T, .len = arrlen(T)},			\
		true)) return perr("expected types to be equal!");	\
	return true;							\
}

#define setup_test_typeneq(NAME, ST, LHS, RHS)				\
static bool NAME(void) {						\
	cnms_t st = ST;							\
	if (type_eq(&st,						\
		(tyref_t){ .buf = LHS, .len = arrlen(LHS)},		\
		(tyref_t){ .buf = RHS, .len = arrlen(RHS)},		\
		true)) return perr("expected types to be not equal!");	\
	return true;							\
}

setup_test_typeeq(test_typeeq1,
	((cnms_t){0}),
	((type_t[]){ (type_t){ .inf.cls = ty_i32 } }))
setup_test_typeneq(test_typeneq1,
	((cnms_t){0}),
	((type_t[]){ (type_t){ .inf.cls = ty_i32 } }),
	((type_t[]){ (type_t){ .inf.cls = ty_f32 } }))
setup_test_typeneq(test_typeneq2,
	((cnms_t){0}),
	((type_t[]){ (type_t){ .inf.cls = ty_i32, .inf.isconst = true } }),
	((type_t[]){ (type_t){ .inf.cls = ty_i32 } }))
setup_test_typeeq(test_typeeq2,
	((cnms_t){0}),
	((type_t[]){	(type_t){ .inf.cls = ty_pfn },
			(type_t){ .id = 2 },
			(type_t){ .inf.cls = ty_i32 },
			(type_t){ .inf.cls = ty_i32 },
			(type_t){ .inf.cls = ty_void }}))
setup_test_typeneq(test_typeneq3,
	((cnms_t){0}),
	((type_t[]){	(type_t){ .inf.cls = ty_pfn },
			(type_t){ .id = 2 },
			(type_t){ .inf.cls = ty_i32 },
			(type_t){ .inf.cls = ty_i32 },
			(type_t){ .inf.cls = ty_void }}),
	((type_t[]){	(type_t){ .inf.cls = ty_pfn },
			(type_t){ .id = 3 },
			(type_t){ .inf.cls = ty_i32 },
			(type_t){ .inf.cls = ty_i64 },
			(type_t){ .inf.cls = ty_void }}))
setup_test_typeneq(test_typeneq4,
	((cnms_t){0}),
	((type_t[]){	(type_t){ .inf.cls = ty_arr },
			(type_t){ .id = 2 },
			(type_t){ .inf.cls = ty_i32 } }),
	((type_t[]){	(type_t){ .inf.cls = ty_arr },
			(type_t){ .id = 3 },
			(type_t){ .inf.cls = ty_i32 } }))

#undef setup_test_typeeq
#undef setup_test_typeneq
#undef setup_test_type

