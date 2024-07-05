#ifndef _type_h_
#define _type_h_

#include <assert.h>

#include "common.h"
#include "lexer.h"

typedef enum {
	ty_void,

	ty_i8,		ty_u8,		ty_i16,		ty_u16,
	ty_i32,		ty_u32,		ty_i64,		ty_u64,
	ty_f32,		ty_f64,

	ty_ref,		ty_slice,	ty_pfn,
	ty_anyref,	ty_anyslice,
	ty_arr,

	ty_user,
} tyclass_t;

static_assert(ty_anyslice < (1 << 4), "POD types must fall under 4 bits");

typedef union {
	struct {
		tyclass_t cls;
		bool isconst;
	} inf;
	size_t		id;
} type_t;

typedef struct {
	type_t		*buf;
	size_t		len;
} tyref_t;

bool type_parse(cnms_t *st, tyref_t *ref, size_t size, bool accept_void);

#endif

