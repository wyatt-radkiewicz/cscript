#ifndef _type_h_
#define _type_h_

#include "common.h"

typedef enum {
	ty_void,

	ty_bool,	ty_char,
	ty_i8,		ty_u8,		ty_i16,		ty_u16,
	ty_i32,		ty_u32,		ty_i64,		ty_u64,
	ty_f32,		ty_f64,

	ty_ref,		ty_slice,
	ty_pfn,		ty_user,	ty_any,
} tyclass_t;

typedef union {
	tyclass_t	cls;
	size_t		id;
} type_t;

typedef struct {
	type_t		*buf;
	size_t		len;
} tyref_t;



#endif

