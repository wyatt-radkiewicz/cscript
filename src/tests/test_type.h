#include "test_common.h"

#define setup_test(NAME, EXPECT_ERROR, S, ...)		\
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

setup_test(test_type1, false, "i8", { .inf.cls = ty_i8 })
setup_test(test_type2, false, "&i8", { .inf.cls = ty_ref }, { .inf.cls = ty_i8 })
setup_test(test_type3, false, "&[]i8", { .inf.cls = ty_slice }, { .inf.cls = ty_i8 })
setup_test(test_type4, false, "&[]any", { .inf.cls = ty_anyslice })
setup_test(test_type5, false, "i16", { .inf.cls = ty_i16 })
setup_test(test_type6, false, "i32", { .inf.cls = ty_i32 })
setup_test(test_type7, false, "i64", { .inf.cls = ty_i64 })
setup_test(test_type8, false, "u8", { .inf.cls = ty_u8 })
setup_test(test_type9, false, "u16", { .inf.cls = ty_u16 })
setup_test(test_type10, false, "u32", { .inf.cls = ty_u32 })
setup_test(test_type11, false, "u64", { .inf.cls = ty_u64 })
setup_test(test_type12, false, "f32", { .inf.cls = ty_f32 })
setup_test(test_type13, false, "f64", { .inf.cls = ty_f64 })
setup_test(test_type14, false, "&any", { .inf.cls = ty_anyref })
setup_test(test_type15, false, "&&any", { .inf.cls = ty_ref }, { .inf.cls = ty_anyref })
setup_test(test_type16, false, "const f64", { .inf.cls = ty_f64, .inf.isconst = true })
setup_test(test_type17, false, "&[] & const any", { .inf.cls = ty_slice }, { .inf.cls = ty_anyref, .inf.isconst = true })
setup_test(test_type18, false, "(i32) void", { .inf.cls = ty_pfn }, { .id = 1 },
	{ .inf.cls = ty_i32 }, { .inf.cls = ty_void })
setup_test(test_type19, false, "(i32, &u8) void", { .inf.cls = ty_pfn }, { .id = 2 },
	{ .inf.cls = ty_i32 }, { .inf.cls = ty_ref }, { .inf.cls = ty_u8 }, { .inf.cls = ty_void })
setup_test(test_type20, false, "(i32, &u8, &any) void", { .inf.cls = ty_pfn }, { .id = 3 },
	{ .inf.cls = ty_i32 }, { .inf.cls = ty_ref }, { .inf.cls = ty_u8 },
	{ .inf.cls = ty_anyref }, { .inf.cls = ty_void })
setup_test(test_type21, true, "any", { .inf.cls = ty_void })
setup_test(test_type22, true, "void", { .inf.cls = ty_void })

#undef setup_test

