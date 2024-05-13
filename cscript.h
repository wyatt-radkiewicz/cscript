// 
//
//
// CScript header - @eklipsed
// Make sure to include the implementation somewhere. Use:
// #define CS_IMPLEMENTATION
// #include "cscript.h"
//
// How to use:
//
//
//
#ifndef _cscript_h_
#define _cscript_h_

#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <tgmath.h>

#define ARRLEN(A) (sizeof(A)/sizeof((A)[0]))
#ifndef CS_PANIC
# ifdef __GNUC__
#  define CS_PANIC(MSG) (__builtin_unreachable())
# elif defined(_MSC_VER)
#  define CS_PANIC(MSG) (__assume(false))
# else
#  define CS_PANIC(MSG)
# endif
#endif

#define CS_MAX_TEMPLATES 4

typedef struct cs_strview {
	const char	*str;
	size_t		len;
} cs_strview_t;

// Create a string view from a literal
#define CS_STRVIEW(LIT) ((cs_strview_t){ .str = (LIT), .len = sizeof(LIT) - 1 })
#define CS_MIN(A, B) ((A) < (B) ? (A) : (B))
#define CS_MAX(A, B) ((A) > (B) ? (A) : (B))

static inline bool cs_strview_eq(const cs_strview_t a, const cs_strview_t b) {
	return a.len == b.len && !memcmp(a.str, b.str, a.len);
}
static inline bool cs_isalpha(char c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
static inline bool cs_isdigit(char c) {
	return c >= '0' && c <= '9';
}
static inline bool cs_isalnum(char c) {
	return cs_isalpha(c) || (c >= '0' && c <= '9');
}
static inline char cs_tolower(char c) {
	return c - (c >= 'A' && c <= 'Z') * 32;
}
// Returns the size of the character in bytes
static inline int cs_utf8next(const uint8_t *ptr, int *c) {
	if ((*ptr & 0x80) == 0x00) {
		*c = *ptr;
		return 1;
	} else if ((*ptr & 0xE0) == 0xC0) {
		*c = (*ptr++ & 0x1F) << 6;
		*c |= *ptr & 0x3F;
		return 2;
	} else if ((*ptr & 0xF0) == 0xE0) {
		*c = (*ptr++ & 0x0F) << 12;
		*c |= (*ptr++ & 0x3F) << 6;
		*c |= *ptr & 0x3F;
		return 3;
	} else if ((*ptr & 0xF8) == 0xF0) {
		*c = (*ptr++ & 0x07) << 18;
		*c |= (*ptr++ & 0x3F) << 12;
		*c |= (*ptr++ & 0x3F) << 6;
		*c |= *ptr & 0x3F;
		return 4;
	} else { // ?????
		*c = *ptr;
		return 1;
	}
}
static inline int cs_utf8out(uint8_t *ptr, int c) {
	if (c <= 0x7F) {
		*ptr = c;
		return 1;
	} else if (c <= 0x7FF) {
		*ptr++ = 0xC0 | (c >> 6 & 0x1F);
		*ptr = 0x80 | (c & 0x3F);
		return 2;
	} else if (c <= 0xFFFF) {
		*ptr++ = 0xC0 | (c >> 12 & 0x1F);
		*ptr++ = 0x80 | (c >> 6 & 0x3F);
		*ptr = 0x80 | (c & 0x3F);
		return 3;
	} else if (c <= 0x10FFFF) {
		*ptr++ = 0xC0 | (c >> 18 & 0x1F);
		*ptr++ = 0x80 | (c >> 12 & 0x3F);
		*ptr++ = 0x80 | (c >> 6 & 0x3F);
		*ptr = 0x80 | (c & 0x3F);
		return 4;
	} else {
		*ptr = 0;
		return 1;
	}
}

// PC		program counter
// 0: SP	stack pointer
// 1-7:		general purpose
enum cs_optype {
	CS_OP_BEQ,	// 0: addr, 1: type, 2: offs
	CS_OP_BNE,
	CS_OP_BGT,
	CS_OP_BLT,
	CS_OP_BGE,
	CS_OP_BLE,

	CS_OP_ADD,	// 0: to, 1: left, 2: right
	CS_OP_SUB,
	CS_OP_MUL,
	CS_OP_DIV,
	CS_OP_MOD,
	CS_OP_AND,
	CS_OP_OR,
	CS_OP_XOR,
	CS_OP_SLL,
	CS_OP_SRL,
	CS_OP_SRA,

	CS_OP_MEMCPY,	// 0: toaddr, 1: fromaddr, 2: size
	CS_OP_TRANS,	// 0: toreg, 1: fromreg
	CS_OP_LOAD,	// 0: mem, 1: reg, 2: <size>
	CS_OP_STORE,
	CS_OP_DATA,	// 0: what reg to store data pointer to
	CS_OP_PUSH,	// 0: reg, 1: <size>
	CS_OP_EXT,
	CS_OP_RED,
	CS_OP_F2I,	// 0: reg (always does max size, 64/32)
	CS_OP_I2F,

	CS_OP_NEG,	// 0: reg, 1: <size>
	CS_OP_NOT,	// 0: reg
	CS_OP_CALL,
	CS_OP_JMP,
	CS_OP_RET,
};

typedef struct cs_op {
	// If true, load constant from code segment (relative to end)
	uint16_t	load_imm		: 1;

	union {
		int16_t			imm	: 15;
		struct {
			uint16_t	ty	: 5;
			uint16_t	op0	: 3;
			uint16_t	op1	: 3;
			uint16_t	op2	: 3;
		};
	};
} cs_op_t;

// Function prototype for external CScript functions
typedef void(*cs_ext_func_t)(void *vm, void *user);
// Called when CScipt encounters an error. 
typedef void(*cs_error_writer_callback_t)(void *user, const char *msg);
// Called when an internal function is being compiled.
typedef void(*cs_symbol_callback_t)(void *user, cs_strview_t func, int loc);
// Called when an external function signature is found.
// You are expected to return a non-null pointer. If it is null,
// then the function will call CS_PANIC
typedef void *(*cs_ext_symbol_callback_t)(void *user, cs_strview_t func);

enum cs_type_class {
	// Here even though you can't explicitly create void types in c-script
	CS_TYPE_VOID,
	CS_TYPE_ERROR,

	// POD types (sentinal type) (ordered in conversion ranking)
	CS_TYPE_BOOL,
	CS_TYPE_CHAR,
	CS_TYPE_I8,
	CS_TYPE_U8,
	CS_TYPE_I16,
	CS_TYPE_U16,
	CS_TYPE_I32,
#ifdef CS_32BIT
	CS_TYPE_ISIZE,
#endif
	CS_TYPE_U32,
#ifdef CS_32BIT
	CS_TYPE_USIZE,
#endif
	CS_TYPE_I64,
#ifndef CS_32BIT
	CS_TYPE_ISIZE,
#endif
	CS_TYPE_U64,
#ifndef CS_32BIT
	CS_TYPE_USIZE,
#endif
	CS_TYPE_F32,
	CS_TYPE_F64,

	CS_TYPE_REF,		// Refrence to something
	CS_TYPE_PTR,		// Since you can only own an entire array,
				// there is no "pointer slice"
	CS_TYPE_ARRPTR,		// Pointer to owned array.
	CS_TYPE_SLICE,		// Refrence to array

	CS_TYPE_PFN,		// Pointer to func type (sentinal type, next level)
				// PFNs are a bool and pointer/offset pair
	CS_TYPE_STRUCT,		// Structs (sentinal type, next level)
	CS_TYPE_ENUM,		// Enums (sentinal type, next level)
	CS_TYPE_ARR,		// Array of size (next level * 2)
	CS_TYPE_TMPL,		// Template	

	CS_TYPE_MUTMASK	= 0x80,	// Is mutable flag
};

#define CS_TYPE_PFNMASK	0x80	// Used in ID byte to specify whether or not
				// The PFN id is a usertype or func ID
#define CS_USERTYPE_STRUCT 0
#define CS_USERTYPE_ENUM 1
#define CS_USERTYPE_PFN 2
#define CS_USERTYPE_TYPEDEF 3

typedef struct cs_vmpfn {
	uint32_t	ext	: 1;	// Is external?

	// Either offset from code start to internal function, or
	// offset from data segment to the pointer of the function
	uint32_t	offs	: 31;
} cs_vmpfn_t;
typedef struct cs_vmslice {
	void		*ptr;
	size_t		len;
} cs_vmslice_t;

typedef struct cs_type {
	uint8_t		levels[8];
} cs_type_t;
typedef struct cs_named_type {
	cs_type_t	type;
	cs_strview_t	name;
} cs_named_type_t;

typedef struct cs_struct {
	cs_strview_t	templates[CS_MAX_TEMPLATES];
	size_t		size;			// Final size
	size_t		alignment;		// Final alignment
	cs_named_type_t	*members;		// Final type (no CS_TYPE_TMPL)
	cs_named_type_t	*member_templates;	// Members with CS_TYPE_TMPL
	int		nmembers;
} cs_struct_t;
typedef struct cs_enum {
	cs_strview_t	templates[CS_MAX_TEMPLATES];
	size_t		size;			// Final size
	size_t		alignment;		// Final alignment

	// Variants can't have templates of their own so they inherit the enum's
	// templates
	int		nvariants;
} cs_enum_t;
typedef struct cs_fnsig {
	cs_named_type_t	*params;
	int		nparams;
	cs_type_t	ret;
} cs_fnsig_t;
typedef struct cs_fn {
	cs_strview_t	name;
	cs_strview_t	templates[CS_MAX_TEMPLATES];
	const char	*def;	// Statements (may be NULL for pfns/templated)
	int		loc;	// Compiled location in code
	// Where the stack is (based on params) at the start
	int		stack_start;
	cs_fnsig_t	sig;
} cs_fn_t;
typedef union cs_usertype {
	cs_strview_t	name;
	int		ty;
	cs_struct_t	s;
	cs_enum_t	e;
	cs_fnsig_t	p;
	cs_type_t	t;
} cs_usertype_t;

enum cs_valloc {
	CS_LOC_ERR,		// Error in evaluating the expression
	CS_LOC_EOF,		// End of expression when evaluating	

	CS_LOC_REG,		// Register
	CS_LOC_DATA,		// Found in data segment
	CS_LOC_STACK,		// Stack (sp relative)
	CS_LOC_INDIRECT	= 0x80,	// Derefrence pointer found at this location
};
typedef struct cs_val {
	unsigned char	loc;	// Location type
	short		scope;	// -1 (not lvalue), 0-SHORT_MAX (scope ID)
	int		offs;	// Location offset
	cs_type_t	type;
} cs_val_t;

typedef struct cs_state {
	cs_strview_t	scopename[128];
	cs_val_t	scope[128];	// Scope array
	int		nscopes;

	int		vstack;		// Virtual stack pointer
	
	cs_usertype_t	usertypes[512];	// Array of usertypes
	int		nusertypes;
	cs_fn_t		funcs[64];
	int		nfuncs;
	cs_type_t	templates[CS_MAX_TEMPLATES];	// Final template types

	cs_named_type_t	typebuffer[256];
	int		ntypes;		// Length of typebuffer
	
	const char	*srcbegin;
	const char	*src;		// Where we are in the source code
	
	uint8_t		*outbegin, *outend;
	uint8_t		*code, *data;	// Data grows up, data grows down
	
	cs_error_writer_callback_t	error_writer;
	void				*error_writer_user;
} cs_state_t;

//
// Source code literal parsing
//
static uint64_t cs__parse_number_hex(cs_strview_t num) {
	uint64_t x = 0, pow = 1;
	const char *ptr = num.str + num.len - 1;
	while (ptr >= num.str) {
		if (cs_isdigit(*ptr)) x = (*ptr - '0') * pow;
		else x = ((*ptr - 'a') + 10) * pow;
		pow *= 16;
		ptr--;
	}
	return x;
}
static uint64_t cs__parse_number_int(cs_strview_t num) {
	uint64_t x = 0, pow = 1;
	const char *ptr = num.str + num.len - 1;
	while (ptr >= num.str) {
		x = (*ptr - '0') * pow;
		pow *= 10;
		ptr--;
	}
	return x;
}
static double cs__parse_number_floating(cs_strview_t num) {
	bool has_decimal = true;
	const char *dot = num.str;
	while (*dot != '.' && dot <= num.str + num.len) dot++;
	if (dot == num.str + num.len) {
		has_decimal = false;
		dot--;
	}
	
	double x = 0.0, pow = 1.0;
	const char *ptr = dot;
	while (ptr >= num.str) {
		x += (double)(*ptr - '0') * pow;
		pow *= 10.0;
		ptr--;
	}

	if (!has_decimal) return x;
	ptr = dot + 1;
	pow = 0.1;
	while (ptr < num.str + num.len) {
		x += (double)(*ptr - '0') * pow;
		pow *= 0.1;
		ptr++;
	}

	return x;
}

// Parse a number. Will set the type that it finds
// It might return a cs_type that is smaller than the outputted variable
// Signed variables will always use i64, unsigned will always use u64, and
// float and double both use f64.
// Returns CS_TYPE_ERROR if it's not a number.
static cs_type_t cs_parse_number(cs_state_t *state, uint64_t *u64,
				int64_t *i64, double *f64) {
	cs_type_t type = (cs_type_t){ .levels[0] = CS_TYPE_ERROR };
	bool neg = false, floating = false, hex = false, is_f32 = false;

	// Get the number of characters in the number
	cs_strview_t num = (cs_strview_t){ .str = state->src };
	if (*state->src == '-') {
		neg = true;
		state->src++;
		num.str++;
	}
	if (!cs_isdigit(*state->src)) return type;
	while (true) {
		if (*state->src == '.') {
			if (floating) return type;
			floating = true;
			state->src++;
			num.len++;
			continue;
		} else if (*state->src == 'x') {
			if (hex) return type;
			hex = true;
			if (num.str[0] != '0' && num.len != 1) return type;
			num.len = 0;
			num.str += 2;
			continue;
		} else if (*state->src == 'f') {
			is_f32 = true;
			floating = true;
			state->src++;
			break;
		}
		if (!cs_isdigit(*state->src)
			&& !(hex
			&& cs_tolower(*state->src) >= 'a'
			&& cs_tolower(*state->src) <= 'f')) {
			break;
		}
		state->src++;
		num.len++;
	}
	if (hex && floating) return type;

	// Actually parse the number based on what is here
	if (hex) {
		// Hex numbers try to be unsigned automatically
		const uint64_t tmp = cs__parse_number_hex(num);
		if (neg) {
			if (tmp >= INT64_MAX-1) return type;
			*i64 = -(int64_t)tmp;
			if (-(int64_t)tmp >= INT32_MIN) type.levels[0] = CS_TYPE_I32;
			else type.levels[0] = CS_TYPE_I64;
		} else {
			*u64 = tmp;
			if (tmp <= UINT32_MAX) type.levels[0] = CS_TYPE_U32;
			else type.levels[0] = CS_TYPE_U64;
		}
	} else if (!floating) {
		// Int numbers try to be signed automatically
		uint64_t tmp = cs__parse_number_int(num);
		if (tmp >= INT64_MAX && neg) return type;
		if (neg && -(int64_t)tmp >= INT32_MIN
			|| !neg && tmp <= INT32_MAX) {
			type.levels[0] = CS_TYPE_I32;
			*i64 = neg ? -tmp : tmp;
		} else if (!neg && tmp <= UINT32_MAX) {
			type.levels[0] = CS_TYPE_U32;
			*u64 = tmp;
		} else if (neg && tmp <= INT64_MAX-1
			|| !neg && tmp <= INT64_MAX) {
			type.levels[0] = CS_TYPE_I64;
			*i64 = neg ? -tmp : tmp;
		} else {
			type.levels[0] = CS_TYPE_U64;
			*u64 = tmp;
		}
	} else {
		// Depends on is_f32
		*f64 = cs__parse_number_floating(num);
		type.levels[0] = is_f32 ? CS_TYPE_F32 : CS_TYPE_F64;
	}

	return type;
}

// Parse everything but the '' around a char
// c is the output char
// l is the number of bytes it would take to encode in utf8
static cs_type_t cs_parse_char(cs_state_t *state, int *c, int *l) {
	int len = cs_utf8next((uint8_t *)state->src, c);
	state->src += len;
	if (*c != '\\') return (cs_type_t){ .levels[0] = len == 1 ? CS_TYPE_CHAR : CS_TYPE_U32 };

	// Escape codes!!
	int i = 0, pow = 0;

	len = cs_utf8next((uint8_t *)state->src, c);
	state->src += len;
	switch (*c) {
	case 'a': *c = '\a'; return (cs_type_t){ .levels[0] = CS_TYPE_CHAR };
	case 'b': *c = '\b'; return (cs_type_t){ .levels[0] = CS_TYPE_CHAR };
	case 'e': *c = '\033'; return (cs_type_t){ .levels[0] = CS_TYPE_CHAR };
	case 'f': *c = '\f'; return (cs_type_t){ .levels[0] = CS_TYPE_CHAR };
	case 'n': *c = '\n'; return (cs_type_t){ .levels[0] = CS_TYPE_CHAR };
	case 'r': *c = '\r'; return (cs_type_t){ .levels[0] = CS_TYPE_CHAR };
	case 't': *c = '\t'; return (cs_type_t){ .levels[0] = CS_TYPE_CHAR };
	case 'v': *c = '\v'; return (cs_type_t){ .levels[0] = CS_TYPE_CHAR };
	case '\\': *c = '\\'; return (cs_type_t){ .levels[0] = CS_TYPE_CHAR };
	case '\'': *c = '\''; return (cs_type_t){ .levels[0] = CS_TYPE_CHAR };
	case '"': *c = '"'; return (cs_type_t){ .levels[0] = CS_TYPE_CHAR };
	case '?': *c = '?'; return (cs_type_t){ .levels[0] = CS_TYPE_CHAR };
	case 'x': *c = 0; i = 2; pow = 0x10; goto do_hexnum;
	case 'u': *c = 0; i = 4; pow = 0x1000; goto do_hexnum;
	case 'U': *c = 0; i = 8; pow = 0x10000000; goto do_hexnum;
	do_hexnum:
		for (; i > 0; i--, pow /= 16) {
			if (*state->src >= '0' && *state->src <= '9') {
				*c += (*state->src - '0') * pow;
			} else if (cs_tolower(*state->src) >= 'a'
				&& cs_tolower(*state->src) <= 'f') {
				*c += (*state->src - 'a' + 10) * pow;
			} else {
				return (cs_type_t){ .levels[0] = CS_TYPE_ERROR };
			}
			state->src++;
		}
	default:
		return (cs_type_t){ .levels[0] = len == 1 ? CS_TYPE_CHAR : CS_TYPE_U32 };
	}
}

// Returns length including the null-terminator
static cs_type_t cs_parse_string(cs_state_t *state, char *buf, size_t size, size_t *len) {
	if (*state->src != '\"') return (cs_type_t){ .levels[0] = CS_TYPE_ERROR };
	state->src++;

	*len = 0;
	while (*state->src != '\"') {
		int c, l;
		if (cs_parse_char(state, &c, &l).levels[0] == CS_TYPE_ERROR) {
			return (cs_type_t){ .levels[0] = CS_TYPE_ERROR };
		}

		// Write to buffer
		if (*len + l < size && buf) cs_utf8out((uint8_t *)buf, c);
		*len += l;
	}
	state->src++;
	return (cs_type_t){
		.levels[0] = CS_TYPE_ARR,
		.levels[1] = (*len) & 0xFF,
		.levels[2] = (*len >> 8) & 0xFF,
		.levels[3] = CS_TYPE_CHAR,
	};
}

// Gets where in the source code the current source pointer is
static void cs_srcpoint(const cs_state_t *state, int *line, int *col) {
	*line = 1;
	*col = 1;

	const char *src = state->srcbegin;
	while (src < state->src) {
		*col += 1;
		if (*src == '\n') {
			*line += 1;
			*col = 1;
		}
		src++;
	}
}

// Returns number of characters that would have been written
// (not including null-terminator)
static int cs_itoa(char *buf, size_t buflen, int x) {
	int ndigits = (x ? 0 : 1) + (x < 0 ? 1 : 0);
	for (int d = x; d; ndigits++, d /= 10);
	if (!buf || !buflen) return ndigits;

	int idx = 0;
	if (x < 0) buf[idx++] = '-';
	for (; x && idx + 1 < buflen; idx++, x /= 10) buf[idx] = '0' + x % 10;
	buf[idx] = '\0';
	return ndigits;
}

static void cs_error_with_context(const cs_state_t *state, const char *msg) {
	char	buf[16];
	int	line, col;
	cs_srcpoint(state, &line, &col);
	cs_itoa(buf, ARRLEN(buf), line);
	state->error_writer(state->error_writer_user, "[cscript err] at ");
	state->error_writer(state->error_writer_user, buf);
	cs_itoa(buf, ARRLEN(buf), col);
	state->error_writer(state->error_writer_user, ":");
	state->error_writer(state->error_writer_user, buf);
	state->error_writer(state->error_writer_user, "\t");
	state->error_writer(state->error_writer_user, msg);
	state->error_writer(state->error_writer_user, "\n");
}

static cs_strview_t cs_parse_word(cs_state_t *state) {
	if (!cs_isalpha(*state->src)) return (cs_strview_t){0};

	cs_strview_t word = { .str = state->src++, .len = 1 };
	while (cs_isalnum(*state->src)) {
		state->src++;
		word.len++;
	}
	return word;
}

// state's src pointer only updates if the two match
static bool cs_expect_keyword(cs_state_t *state, const cs_strview_t keyword) {
	const char *backup = state->src;
	if (cs_strview_eq(cs_parse_word(state), keyword)) return true;
	state->src = backup;
	return false;
}

static void cs_parse_whitespace(cs_state_t *state, bool include_newline) {
	while (*state->src == ' '
		|| *state->src == '\t'
		|| (include_newline && *state->src == '\n')) state->src++;
}

// Evaluate a type's alignment
static size_t cs_alignment(cs_state_t *state, const uint8_t *type) {
	switch (type[0] & ~CS_TYPE_MUTMASK) {
	case CS_TYPE_I8: case CS_TYPE_U8: case CS_TYPE_BOOL: case CS_TYPE_CHAR:
		return 1;
	case CS_TYPE_I16: case CS_TYPE_U16:
		return 2;
	case CS_TYPE_I32: case CS_TYPE_U32: case CS_TYPE_F32:
		return 4;
	case CS_TYPE_I64: case CS_TYPE_U64: case CS_TYPE_F64:
		return 8;
#ifdef CS_32BIT
	case CS_TYPE_ISIZE: return 4;
	case CS_TYPE_USIZE: return 4;
#else
	case CS_TYPE_ISIZE: return 8;
	case CS_TYPE_USIZE: return 8;
#endif
	case CS_TYPE_REF: case CS_TYPE_PTR:
		return alignof(void*);
	case CS_TYPE_ARRPTR: case CS_TYPE_SLICE:
		return alignof(cs_vmslice_t);
	case CS_TYPE_PFN:
		return alignof(cs_vmpfn_t);
	case CS_TYPE_STRUCT:
		return state->usertypes[type[1]].s.alignment;
	case CS_TYPE_ENUM:
		return state->usertypes[type[1]].e.alignment;
	case CS_TYPE_ARR:
		return cs_alignment(state, type + 3);
	default:
		CS_PANIC("Can not alignment size of invalid type.");
	}
}

// Evaluate a type's size
static size_t cs_size(cs_state_t *state, const uint8_t *type) {
	switch (type[0] & ~CS_TYPE_MUTMASK) {
	case CS_TYPE_I8: case CS_TYPE_U8: case CS_TYPE_BOOL: case CS_TYPE_CHAR:
		return 1;
	case CS_TYPE_I16: case CS_TYPE_U16:
		return 2;
	case CS_TYPE_I32: case CS_TYPE_U32: case CS_TYPE_F32:
		return 4;
	case CS_TYPE_I64: case CS_TYPE_U64: case CS_TYPE_F64:
		return 8;
#ifdef CS_32BIT
	case CS_TYPE_ISIZE: return 4;
	case CS_TYPE_USIZE: return 4;
#else
	case CS_TYPE_ISIZE: return 8;
	case CS_TYPE_USIZE: return 8;
#endif
	case CS_TYPE_REF: case CS_TYPE_PTR:
		return sizeof(void*);
	case CS_TYPE_ARRPTR: case CS_TYPE_SLICE:
		return sizeof(cs_vmslice_t);
	case CS_TYPE_PFN:
		return sizeof(cs_vmpfn_t);
	case CS_TYPE_STRUCT:
		return state->usertypes[type[1]].s.size;
	case CS_TYPE_ENUM:
		return state->usertypes[type[1]].e.size;
	case CS_TYPE_ARR:
		return (type[1] | (type[2] << 8)) * cs_size(state, type + 3);
	default:
		CS_PANIC("Can not find size of invalid type.");
	}
}

// Get the type from the source
static int cs__parse_type_getident(cs_state_t *state, cs_type_t *type, int *nlevels);
static cs_type_t cs_parse_type(cs_state_t *state, const bool implicit_const) {
	cs_type_t	type;
	int		nlevels = 0;

	// First qualifier may be implicitly mutable instead of const so based
	// on that check the first keyword for that
	if (implicit_const) {
		type.levels[nlevels] =
			cs_expect_keyword(state, CS_STRVIEW("mut"))
			? CS_TYPE_MUTMASK : 0;
	} else {
		type.levels[nlevels] =
			cs_expect_keyword(state, CS_STRVIEW("const"))
			? 0 : CS_TYPE_MUTMASK;
	}

	for (;;) {
		cs_parse_whitespace(state, false);
		
		switch (*state->src++) {
		case '&':
			if (*state->src == '[') {
				if (*++state->src != ']') {
					cs_error_with_context(state, "Unexpected array size in slice!");
					type.levels[0] = CS_TYPE_ERROR;
					return type;
				}
				state->src++;
				type.levels[nlevels++] |= CS_TYPE_SLICE;
			} else {
				type.levels[nlevels++] |= CS_TYPE_REF;
			}
			goto prepare_next;
		case '*':
			if (*state->src == '[') {
				if (*++state->src != ']') {
					cs_error_with_context(state, "Unexpected array size in ptr slice!");
					type.levels[0] = CS_TYPE_ERROR;
					return type;
				}
				state->src++;
				type.levels[nlevels++] |= CS_TYPE_ARRPTR;
			} else {
				type.levels[nlevels++] |= CS_TYPE_PTR;
			}
			goto prepare_next;
		case '[':
			CS_PANIC("Implement array size in type parsing!");

#define CHECK2(C1, C2, TYPE) \
	if (*state->src == C1 \
		&& state->src[1] == C2 \
		&& !cs_isalnum(state->src[2])) { \
		type.levels[nlevels++] |= TYPE; \
		goto hit_end; \
	}
		case 'u':
			if (*state->src == '8' && !cs_isalnum(state->src[1])) {
				type.levels[nlevels++] |= CS_TYPE_U8;
				goto hit_end;
			}
			CHECK2('1', '6', CS_TYPE_U16);
			CHECK2('3', '2', CS_TYPE_U32);
			CHECK2('6', '4', CS_TYPE_U64);
			if (cs_expect_keyword(state, CS_STRVIEW("size"))) {
				type.levels[nlevels++] |= CS_TYPE_USIZE;
				goto hit_end;
			}
			break;
		case 'i':
			if (*state->src == '8' && !cs_isalnum(state->src[1])) {
				type.levels[nlevels++] |= CS_TYPE_I8;
				goto hit_end;
			}
			CHECK2('1', '6', CS_TYPE_I16);
			CHECK2('3', '2', CS_TYPE_I32);
			CHECK2('6', '4', CS_TYPE_I64);
			if (cs_expect_keyword(state, CS_STRVIEW("size"))) {
				type.levels[nlevels++] |= CS_TYPE_ISIZE;
				goto hit_end;
			}
			break;
		case 'f':
			CHECK2('3', '2', CS_TYPE_F32);
			CHECK2('6', '4', CS_TYPE_F64);
#undef CHECK2
			break;
		case 'b':
			if (cs_expect_keyword(state, CS_STRVIEW("ool"))) {
				type.levels[nlevels++] |= CS_TYPE_BOOL;
				goto hit_end;
			}
			break;
		case 'c':
			if (cs_expect_keyword(state, CS_STRVIEW("har"))) {
				type.levels[nlevels++] |= CS_TYPE_CHAR;
				goto hit_end;
			}
			break;
		}

		// Search for an identifier
		state->src--;
		if (cs__parse_type_getident(state, &type, &nlevels)) {
			return type;
		}
prepare_next:
		if (nlevels >= ARRLEN(type.levels)) {
			cs_error_with_context(state, "Max type nesting hit.");
			type.levels[0] = CS_TYPE_ERROR;
			return type;
		}

		cs_parse_whitespace(state, false);
		type.levels[nlevels] =
			cs_expect_keyword(state, CS_STRVIEW("mut"))
			? CS_TYPE_MUTMASK : 0;
	}
hit_end:
	return type;
}

static int cs__parse_type_getident(cs_state_t *state, cs_type_t *type, int *nlevels) {
	cs_strview_t ident = cs_parse_word(state);
	
	for (int i = 0; i < state->nusertypes; i++) {
		const cs_usertype_t *ty = state->usertypes + i;
		if (!cs_strview_eq(ty->name, ident)) continue;
		switch (ty->ty) {
		case CS_USERTYPE_PFN:
			type->levels[(*nlevels)++] |= CS_TYPE_PFN;
			break;
		case CS_USERTYPE_ENUM:
			type->levels[(*nlevels)++] |= CS_TYPE_ENUM;
			break;
		case CS_USERTYPE_STRUCT:
			type->levels[(*nlevels)++] |= CS_TYPE_STRUCT;
			break;
		case CS_USERTYPE_TYPEDEF: {
			// Append the typedef levels
			for (int i = 0; i < ARRLEN(type->levels); i++) {
				type->levels[(*nlevels)++] = ty->t.levels[i];
				if (ty->t.levels[i] == CS_TYPE_VOID) break;
				if (*nlevels >= ARRLEN(type->levels)) {
					cs_error_with_context(state,
					"Max type nesting hit.");
					type->levels[0] = CS_TYPE_ERROR;
					return -1;
				}
			}
			return 0;
		} }

		if (*nlevels >= ARRLEN(type->levels)) {
			cs_error_with_context(state, "Max type nesting hit.");
			type->levels[0] = CS_TYPE_ERROR;
			return -1;
		}
		type->levels[(*nlevels)++] = i;
		return 0;
	}

	// TODO: Just assume that it is a template
	type->levels[(*nlevels)++] = CS_TYPE_TMPL;
	return 0;
}

//
// Finding type of a evaluation expression
//
static cs_val_t cs_eval(cs_state_t *state, bool onlytype);
static bool cs_is_arithmetic(const cs_type_t *type) {
	switch (type->levels[0] & ~CS_TYPE_MUTMASK) {
	case CS_TYPE_PFN: case CS_TYPE_TMPL: case CS_TYPE_ENUM:
	case CS_TYPE_STRUCT: case CS_TYPE_SLICE: case CS_TYPE_ARR:
	case CS_TYPE_VOID: case CS_TYPE_ERROR: return false;
	default: return true;
	}
}
static cs_type_t cs_arithmetic_promotion(const cs_type_t *type) {
	if (!cs_is_arithmetic(type)) return (cs_type_t){ .levels[0] = CS_TYPE_ERROR };
	switch (type->levels[0] & ~CS_TYPE_MUTMASK) {
	case CS_TYPE_I64: case CS_TYPE_U64: case CS_TYPE_F64:
	case CS_TYPE_F32: case CS_TYPE_U32:
		return *type;
	default:
		return (cs_type_t){ .levels[0] = CS_TYPE_I32 };
	}
}
static cs_type_t cs_arithmetic_conversion(const cs_type_t *a, const cs_type_t *b) {
	if (!cs_is_arithmetic(a) || !cs_is_arithmetic(b))
		return (cs_type_t){ .levels[0] = CS_TYPE_ERROR };
	const int alvl = a->levels[0] & ~CS_TYPE_MUTMASK;
	const int blvl = b->levels[0] & ~CS_TYPE_MUTMASK;

	// Make both const if one is
	const int mutmask = (a->levels[0] & CS_TYPE_MUTMASK)
		& (b->levels[0] & CS_TYPE_MUTMASK);
	return (cs_type_t){ .levels[0] = CS_MAX(alvl, blvl) | mutmask };
}
static bool cs_types_eq(const cs_state_t *state, const cs_type_t *a,
			const cs_type_t *b, bool ignore_quals);
static bool cs_fnsig_eq(const cs_state_t *state, const cs_fnsig_t *a,
			const cs_fnsig_t *b) {
	if (a->nparams != b->nparams) return false;
	if (!cs_types_eq(state, &a->ret, &b->ret, false)) return false;
	for (int i = 0; i < a->nparams; i++) {
		if (!cs_types_eq(state, &a->params[i].type, &b->params[i].type, false))
			return false;
	}
	return true;
}
static bool cs_types_eq(const cs_state_t *state, const cs_type_t *a,
			const cs_type_t *b, bool ignore_quals) {
	for (int i = 0; i < ARRLEN(a->levels); i++) {
		if (ignore_quals) {
			if ((a->levels[i] & ~CS_TYPE_MUTMASK)
				!= (b->levels[i] & ~CS_TYPE_MUTMASK))
				return false;
		} else {
			if (a->levels[i] != b->levels[i]) return false;
		}
		switch (a->levels[i] & ~CS_TYPE_MUTMASK) {
		case CS_TYPE_ARR: {
			// Check array lengths (must be same)
			const unsigned alen = a->levels[i+1] | a->levels[i+2] << 8;
			const unsigned blen = b->levels[i+1] | b->levels[i+2] << 8;
			if (alen == blen) return false;
			i += 2;	// Only inc twice since inc inc for loop
			continue;
		} case CS_TYPE_PFN: {
			if (a->levels[i+1] == b->levels[i+1]) {
				return true;
			} else if ((a->levels[i+1] & CS_TYPE_PFNMASK)
				== (b->levels[i+1] & CS_TYPE_PFNMASK)
				&& (a->levels[i+1] & ~CS_TYPE_PFNMASK)
				!= (b->levels[i+1] & ~CS_TYPE_PFNMASK)){
				// If id types are equal but id's are unequal
				// this automatically makes it non-matching
				return false;
			}

			// See if function signatures match
			const cs_fnsig_t *sigs[2];
			const uint8_t lvls[2] = { a->levels[i+1], b->levels[i+1] };
			for (int i = 0; i < 2; i++) {
				if (lvls[i] & CS_TYPE_PFNMASK) {
					sigs[i] = &state->funcs[a->levels[i+1] & ~CS_TYPE_PFNMASK].sig;
				} else {
					sigs[i] = &state->usertypes[a->levels[i+1]].p;
				}
			}
			return cs_fnsig_eq(state, sigs[0], sigs[1]);
		} case CS_TYPE_PTR: case CS_TYPE_REF:
		case CS_TYPE_SLICE: case CS_TYPE_ARRPTR: continue;
		}
		break;	// Hit sentinal
	}
	return true;
}
// Returns true if from can be cast to to's mutability qualifiers
// If types aren't the same it will also return false
static bool cs_cast_mutability(const cs_state_t *state, const cs_type_t *from,
				const cs_type_t *to) {
	if (!cs_types_eq(state, from, to, true)) return false;
	for (int i = 0; i < ARRLEN(from->levels); i++) {
		// Check mutability bit
		if ((to->levels[i] & CS_TYPE_MUTMASK)
			&& !(from->levels[i] & CS_TYPE_MUTMASK)) return false;
		
		// Skip things like ID's and array lengths
		switch (from->levels[i] & ~CS_TYPE_MUTMASK) {
		case CS_TYPE_ARR: i += 2; continue;
		case CS_TYPE_PTR: case CS_TYPE_REF:
		case CS_TYPE_SLICE: case CS_TYPE_ARRPTR: continue;
		}
		break;	// Hit sentinal
	}
	return true;
}

static cs_type_t cs_remove_level(const cs_type_t *type) {
	const int n = (type->levels[0] & ~CS_TYPE_MUTMASK) == CS_TYPE_ARR
		? 3 : 1;
	cs_type_t ret = *type;
	memmove(ret.levels, ret.levels + n, sizeof(ret.levels) - sizeof(ret.levels[0]) * n);
	return ret;
}

// Returns true if from can be cast to to
static bool cs_cast_type(const cs_state_t *state, const cs_type_t *from,
			const cs_type_t *to) {
	if (from->levels[0] == CS_TYPE_ERROR || to->levels[0] == CS_TYPE_ERROR) return false;
	switch (to->levels[0] & CS_TYPE_MUTMASK) {
	case CS_TYPE_BOOL:
		switch (from->levels[0] & CS_TYPE_MUTMASK) {
		case CS_TYPE_VOID: case CS_TYPE_ARR: case CS_TYPE_ENUM:
		case CS_TYPE_STRUCT: case CS_TYPE_SLICE: return false;
		case CS_TYPE_REF: {
			cs_type_t from_deref = cs_remove_level(from);
			return cs_cast_type(state, &from_deref, to);
		} }
		return true;
	case CS_TYPE_ARR: {
		const cs_type_t frominner = cs_remove_level(from),
			  toinner = cs_remove_level(to);
		if (!cs_types_eq(state, &frominner, &toinner, false)) return false;
		const unsigned fromlen = from->levels[1] | from->levels[2] << 8;
		const unsigned tolen = to->levels[1] | to->levels[2] << 8;
		if (tolen < fromlen) return false;
		return true;
	} case CS_TYPE_REF: {
		cs_type_t to_deref = cs_remove_level(to);
		return cs_cast_type(state, from, &to_deref);
	} case CS_TYPE_STRUCT: case CS_TYPE_ENUM:
	case CS_TYPE_PFN: case CS_TYPE_PTR: case CS_TYPE_ARRPTR:
		return cs_cast_mutability(state, from, to);
	default:	// Arithmetic types (just check mutability)
		return !(to->levels[0] & CS_TYPE_MUTMASK)
			|| (from->levels[0] & CS_TYPE_MUTMASK);
	}
}

static cs_val_t cs__eval_literal(cs_state_t *state, bool onlytype) {
	if (*state->src == '\'') { // This is a char
		int c, l;
		state->src++;
		const cs_type_t type = cs_parse_char(state, &c, &l);
		if (*state->src++ != '\''
			|| type.levels[0] == CS_TYPE_ERROR) goto emit_error;
		return (cs_val_t){ .type = type, .loc = CS_LOC_REG };
	} else if (*state->src == '"') {
		size_t len;
		const cs_type_t type = cs_parse_string(state, NULL, 0, &len);
		if (type.levels[0] == CS_TYPE_ERROR) goto emit_error;
		return (cs_val_t){ .type = type, .loc = CS_LOC_REG };
	} else if (cs_isdigit(*state->src)) {
		uint64_t u64;
		int64_t i64;
		double f64;

		const cs_type_t type = cs_parse_number(state, &u64, &i64, &f64);
		if (type.levels[0] == CS_TYPE_ERROR) goto emit_error;
		return (cs_val_t){ .type = type, .loc = CS_LOC_REG };
	}

	// At the end of an expression?
	return (cs_val_t){ .loc = CS_LOC_EOF };

emit_error:
	cs_error_with_context(state, "Expected literal.");
	return (cs_val_t){0};
}

static cs_val_t cs__eval_factor(cs_state_t *state, bool onlytype) {
	const cs_strview_t ident = cs_parse_word(state);

	// Parse a literal
	if (!ident.len) return cs__eval_literal(state, onlytype);
	
	// True/false
	if (cs_strview_eq(ident, CS_STRVIEW("true"))
		|| cs_strview_eq(ident, CS_STRVIEW("false"))) {
		return (cs_val_t){ .type.levels[0] = CS_TYPE_BOOL, .loc = CS_LOC_REG };
	}

	// Do struct/POD init
	if (*state->src == '{') {
		// Consume interior
		state->src++;
		int nbraces = 1;
		while (nbraces) {
			if (*state->src == '{') nbraces++;
			else if (*state->src =='}') nbraces--;
			state->src++;
		}

		// Get the struct/pod type
		const char *const backup = state->src;
		state->src = ident.str;
		const cs_type_t type = cs_parse_type(state, true);
		state->src = backup;
		return (cs_val_t){ .type = type, .loc = CS_LOC_REG };
	}

	// Look for variable with same name as ident
	for (int i = 0; i < state->nscopes; i++) {
		if (!cs_strview_eq(state->scopename[i], ident)) continue;
		return (cs_val_t){ .type = state->scope[i].type, .loc = CS_LOC_REG };
	}

	cs_error_with_context(state, "Unidentified identifier.");
	return (cs_val_t){0};
}

static cs_val_t cs__eval_grouping(cs_state_t *state, bool onlytype) {
	if (*state->src == '(') {
		state->src++;
		cs_val_t val = cs_eval(state, onlytype);
		if (*state->src != ')') {
			cs_error_with_context(state,
			"Expected ')'");
			return (cs_val_t){0};
		}
		state->src++;
		return val;
	} else if (*state->src == '{') {
		CS_PANIC("Process type of statement expression");
	} else {
		return cs__eval_factor(state, onlytype);
	}
}

static cs_val_t cs__eval_postfix(cs_state_t *state, bool onlytype) {
	cs_val_t val = cs__eval_grouping(state, onlytype);

	cs_parse_whitespace(state, false);
	if (*state->src == '-' && *(state->src+1) == '>') {
		if (val.type.levels[0] != CS_TYPE_PTR) {
			cs_error_with_context(state,
			"Expect pointer to struct or enum variant with arrow operator!");
			return (cs_val_t){0};
		}
		val.type = cs_remove_level(&val.type);
		if (val.type.levels[0] != CS_TYPE_STRUCT) {
			cs_error_with_context(state,
			"Expect struct or enum variant with arrow operator!");
			return (cs_val_t){0};
		}
		state->src += 2;
	} else if (*state->src == '.') {
		if (val.type.levels[0] == CS_TYPE_REF) val.type = cs_remove_level(&val.type);
		if (val.type.levels[0] != CS_TYPE_STRUCT) {
			cs_error_with_context(state,
			"Expect struct or enum variant with dot operator!");
			return (cs_val_t){0};
		}
		state->src++;
	} else if (*state->src == '(') {
		if ((val.type.levels[0] & ~CS_TYPE_MUTMASK) != CS_TYPE_PFN) {
			cs_error_with_context(state,
			"Must have function or pfn when calling!");
			return (cs_val_t){0};
		}

		state->src++;
		while (*state->src != ')') {
			if (cs_eval(state, onlytype).loc == CS_LOC_ERR) {
				cs_error_with_context(state,
				"Expected function parameter.");
				return (cs_val_t){0};
			}
			if (*state->src == ',') state->src++;
		}
		state->src++;
		
		// Check for normal functions (common path)
		if (val.type.levels[1] & CS_TYPE_PFNMASK) {
			val.type = state->funcs[val.type.levels[1] & ~CS_TYPE_PFNMASK].sig.ret;
		} else { // Check for function pointers
			val.type = state->usertypes[val.type.levels[1]].p.ret;
		}
		return val;
	} else if (*state->src == '[') {
		if (val.type.levels[0] != CS_TYPE_SLICE
			&& val.type.levels[0] != CS_TYPE_ARRPTR
			&& val.type.levels[0] != CS_TYPE_ARR) {
			cs_error_with_context(state,
			"Expected array for array subscript.");
			return (cs_val_t){0};
		}
		state->src++;
		if (cs_eval(state, onlytype).loc == CS_LOC_ERR) {
			cs_error_with_context(state,
			"Expected expression for array subscript.");
			return (cs_val_t){0};
		}
		if (*state->src++ != ']') {
			cs_error_with_context(state,
			"Expected expression for array subscript.");
			return (cs_val_t){0};
		}
		if ((val.type = cs_remove_level(&val.type)).levels[0] == CS_TYPE_ERROR) {
			cs_error_with_context(state,
			"Expected array when indexing array!");
			return (cs_val_t){0};
		}
		return val;
	} else {
		return val;
	}

	// Search for struct member
	const cs_strview_t member = cs_parse_word(state);
	const cs_struct_t *s = &state->usertypes[val.type.levels[1]].s;
	for (int i = 0; i < s->nmembers; i++) {
		if (!cs_strview_eq(s->members[i].name, member)) continue;
		return (cs_val_t){ .type = s->members[i].type, .loc = CS_LOC_REG };
	}

	cs_error_with_context(state, "Unidentified member name.");
	return (cs_val_t){0};
}

static cs_val_t cs__eval_builtin(cs_state_t *state, bool onlytype) {
	if (cs_expect_keyword(state, CS_STRVIEW("lenof"))) {
		if (*state->src++ != '(') goto paren_error;
		if (cs_eval(state, onlytype).loc == CS_LOC_ERR) goto expr_error;
		if (*state->src++ != ')') goto paren_error;
		return (cs_val_t){ .type.levels[0] = CS_TYPE_USIZE, .loc = CS_LOC_REG };
	} else if (cs_expect_keyword(state, CS_STRVIEW("typeof"))) {
		if (*state->src++ != '(') goto paren_error;
		const cs_val_t val = cs_eval(state, onlytype);
		if (val.loc == CS_LOC_ERR) goto expr_error;
		if (*state->src++ != ')') goto paren_error;
		return val;
	} else if (cs_expect_keyword(state, CS_STRVIEW("sizeof"))) {
		if (*state->src++ != '(') goto paren_error;
		if (cs_eval(state, onlytype).loc == CS_LOC_ERR) goto expr_error;
		if (*state->src++ != ')') goto paren_error;
		return (cs_val_t){ .type.levels[0] = CS_TYPE_USIZE, .loc = CS_LOC_REG };
	} else if (cs_expect_keyword(state, CS_STRVIEW("alignof"))) {
		if (*state->src++ != '(') goto paren_error;
		if (cs_eval(state, onlytype).loc == CS_LOC_ERR) goto expr_error;
		if (*state->src++ != ')') goto paren_error;
		return (cs_val_t){ .type.levels[0] = CS_TYPE_USIZE, .loc = CS_LOC_REG };
	} else {
		return cs__eval_postfix(state, onlytype);
	}

paren_error:
	cs_error_with_context(state, "Expected parenthesis");
	goto ret_error;
expr_error:
	cs_error_with_context(state, "Expected error");
ret_error:
	return (cs_val_t){0};
}

static cs_val_t cs__eval_prefix(cs_state_t *state, bool onlytype) {
	cs_parse_whitespace(state, false);

	char c = *state->src;
	switch (c) {
	case '+': case '-': case '!': case '~': case '&': case '*':
		break;
	default:
		return cs__eval_builtin(state, onlytype);
	}

	state->src++;
	cs_parse_whitespace(state, false);
	if (cs_isdigit(*state->src)) c = ' ';
	cs_val_t val = cs__eval_prefix(state, onlytype);
	if (c == '*') {
		if (val.type.levels[0] != CS_TYPE_PTR) {
			cs_error_with_context(state, "Can't derefrence non-ptr!");
			return (cs_val_t){0};
		}
	} else if (c == '!') {
		const cs_type_t to = (cs_type_t){ .levels[0] = CS_TYPE_BOOL };
		if (!cs_cast_type(state, &val.type, &to)) {
			cs_error_with_context(state,
			"Can't use \"!\" on a type that can't be cast to bool!");
			return (cs_val_t){0};
		}
		val.type = to;
	} else {
		if ((val.type = cs_arithmetic_promotion(&val.type)).levels[0] == CS_TYPE_ERROR) {
			cs_error_with_context(state,
			"Can't use unary arithmetic operations on non-arithmetic types!");
			return (cs_val_t){0};
		}
	}
	return val;
}

static cs_val_t cs__eval_multiplicative(cs_state_t *state, bool onlytype, cs_val_t left) {
	cs_parse_whitespace(state, false);
	if (*state->src != '*' && *state->src != '/' && *state->src != '%') {
		return cs__eval_prefix(state, onlytype);
	}

	while (*state->src == '*' || *state->src == '/' || *state->src == '%') {
		state->src++;
		cs_parse_whitespace(state, false);
		cs_val_t right = cs__eval_prefix(state, onlytype);
		if (right.loc == CS_LOC_EOF) goto error;
		if (right.loc == CS_LOC_ERR) goto error;
		left.type = cs_arithmetic_conversion(&left.type, &right.type);
		if (left.type.levels[0] == CS_TYPE_ERROR) goto error;
		cs_parse_whitespace(state, false);
	}
	return left;
error:
	cs_error_with_context(state,
	"Expected arithmetic types for left and right hand of multiplicative expr");
	return (cs_val_t){0};
}

static cs_val_t cs__eval_additive(cs_state_t *state, bool onlytype, cs_val_t left) {
	cs_parse_whitespace(state, false);
	if (*state->src != '+' && *state->src != '-') {
		left = cs__eval_multiplicative(state, onlytype, left);
	}

	while (*state->src == '+' || *state->src == '-') {
		state->src++;
		cs_parse_whitespace(state, false);
		const cs_val_t nextleft = cs__eval_prefix(state, onlytype);
		cs_val_t right = cs__eval_multiplicative(state, onlytype, nextleft);
		if (right.loc == CS_LOC_EOF) right = nextleft;
		if (right.loc == CS_LOC_ERR) goto error;
		left.type = cs_arithmetic_conversion(&left.type, &right.type);
		if (left.type.levels[0] == CS_TYPE_ERROR) goto error;
		cs_parse_whitespace(state, false);
	}
	return left;
error:
	cs_error_with_context(state,
	"Expected arithmetic types for left and right hand of additive expr");
	return (cs_val_t){0};
}

static cs_val_t cs__eval_bitshift(cs_state_t *state, bool onlytype, cs_val_t left) {
	cs_parse_whitespace(state, false);
	if ((*state->src != '<' || state->src[1] != '<')
		&& (*state->src != '>' || state->src[1] != '>')) {
		left = cs__eval_additive(state, onlytype, left);
	}

	while ((*state->src == '<' && state->src[1] == '<')
		|| (*state->src == '>' && state->src[1] == '>')) {
		state->src += 2;
		cs_parse_whitespace(state, false);
		const cs_val_t nextleft = cs__eval_prefix(state, onlytype);
		cs_val_t right = cs__eval_additive(state, onlytype, nextleft);
		if (right.loc == CS_LOC_EOF) right = nextleft;
		if (right.loc == CS_LOC_ERR) goto error;
		left.type = cs_arithmetic_conversion(&left.type, &right.type);
		if (left.type.levels[0] == CS_TYPE_ERROR) goto error;
		cs_parse_whitespace(state, false);
	}
	return left;
error:
	cs_error_with_context(state,
	"Expected arithmetic types for left and right hand of shift expr");
	return (cs_val_t){0};
}

static cs_val_t cs__eval_bit(cs_state_t *state, bool onlytype, cs_val_t left, int oplvl) {
	const char opchars[] = { '|', '^', '&' };
	cs_parse_whitespace(state, false);
	if (*state->src != opchars[oplvl]) {
		left = oplvl == 2 ? cs__eval_bitshift(state, onlytype, left)
			: cs__eval_bit(state, onlytype, left, oplvl+1);
	}

	while (*state->src == opchars[oplvl]) {
		state->src++;
		cs_parse_whitespace(state, false);
		const cs_val_t nextleft = cs__eval_prefix(state, onlytype);
		cs_val_t right = oplvl == 2 ? cs__eval_bitshift(state, onlytype, nextleft)
			: cs__eval_bit(state, onlytype, nextleft, oplvl+1);
		if (right.loc == CS_LOC_EOF) right = nextleft;
		if (right.loc == CS_LOC_ERR) goto error;
		left.type = cs_arithmetic_conversion(&left.type, &right.type);
		if (left.type.levels[0] == CS_TYPE_ERROR) goto error;
		cs_parse_whitespace(state, false);
	}
	return left;
error:
	cs_error_with_context(state,
	"Expected arithmetic types for left and right hand of bitwise expr");
	return (cs_val_t){0};
}

static cs_val_t cs__eval_equality(cs_state_t *state, bool onlytype, cs_val_t left, const bool rel) {
	cs_parse_whitespace(state, false);
	if (rel) {
		if (*state->src != '>' && *state->src != '<')
			left = cs__eval_bit(state, onlytype, left, 0);
	} else {
		if ((*state->src != '!' && *state->src != '=') || state->src[1] != '=')
			left = cs__eval_equality(state, onlytype, left, true);
	}

	const cs_type_t boolty = (cs_type_t){ .levels[0] = CS_TYPE_BOOL };
	while (rel && (*state->src == '>' || state->src[1] == '<')
		|| (*state->src == '!' || *state->src == '=') && state->src[1] == '=') {
		if (!cs_cast_type(state, &left.type, &boolty)) goto error;
		left.type = boolty;
		if (rel) {
			state->src++;
			if (*state->src == '=') state->src++;
		} else {
			state->src += 2;
		}
		cs_parse_whitespace(state, false);
		const cs_val_t nextleft = cs__eval_prefix(state, onlytype);
		cs_val_t right = rel ? cs__eval_bit(state, onlytype, nextleft, 0)
			: cs__eval_equality(state, onlytype, nextleft, true);
		if (right.loc == CS_LOC_EOF) right = nextleft;
		if (right.loc == CS_LOC_ERR) goto error;
		right.type = cs_arithmetic_conversion(&left.type, &right.type);
		if (right.type.levels[0] == CS_TYPE_ERROR) goto error;
		if (!cs_cast_type(state, &right.type, &boolty)) goto error;
		cs_parse_whitespace(state, false);
	}
	return left;
error:
	cs_error_with_context(state,
	"Expected boolean types for left and right hand of bitwise expr");
	return (cs_val_t){0};
}

static cs_val_t cs__eval_logic(cs_state_t *state, bool onlytype, cs_val_t left, const bool band) {
	cs_parse_whitespace(state, false);
	if (band) {
		if (*state->src != '&' || state->src[1] != '&')
			left = cs__eval_equality(state, onlytype, left, false);
	} else {
		if (*state->src != '|' || state->src[1] != '|')
			left = cs__eval_logic(state, onlytype, left, true);
	}

	const char opchar = band ? '&' : '|';
	const cs_type_t boolty = (cs_type_t){ .levels[0] = CS_TYPE_BOOL };
	while (*state->src == opchar && state->src[1] == opchar) {
		state->src += 2;
		if (!cs_cast_type(state, &left.type, &boolty)) goto error;
		left.type = boolty;
		cs_parse_whitespace(state, false);
		const cs_val_t nextleft = cs__eval_prefix(state, onlytype);
		cs_val_t right = band ? cs__eval_equality(state, onlytype, nextleft, false)
			: cs__eval_logic(state, onlytype, nextleft, false);
		if (right.loc == CS_LOC_EOF) right = nextleft;
		if (right.loc == CS_LOC_ERR) goto error;
		if (!cs_cast_type(state, &right.type, &boolty)) goto error;
		cs_parse_whitespace(state, false);
	}
	return left;
error:
	cs_error_with_context(state,
	"Expected boolean types for left and right hand of boolean logic expr");
	return (cs_val_t){0};
}

static cs_val_t cs__eval_assign(cs_state_t *state, bool onlytype) {
	cs_parse_whitespace(state, false);
	cs_val_t left = cs__eval_prefix(state, onlytype);
	if (left.loc == CS_LOC_ERR) {
		return (cs_val_t){0};
	}
	cs_parse_whitespace(state, false);
	if (*state->src != '=') return cs__eval_logic(state, onlytype, left, false);
	state->src++;
	if (cs_eval(state, onlytype).loc == CS_LOC_ERR) {
		return (cs_val_t){0};
	}
	return left;
}

static cs_val_t cs__eval_cond(cs_state_t *state, bool onlytype) {
	cs_val_t val = {0};
	cs_parse_whitespace(state, false);
	if (!cs_expect_keyword(state, CS_STRVIEW("if"))) {
		return cs__eval_assign(state, onlytype);
	}
	if (cs_eval(state, onlytype).loc == CS_LOC_ERR) return val;
	cs_parse_whitespace(state, false);
	if (!cs_expect_keyword(state, CS_STRVIEW("then"))) {
		cs_error_with_context(state, "Expected \"then\"");
		return val;
	}
	cs_val_t ontrue = cs_eval(state, onlytype);
	if (ontrue.loc == CS_LOC_ERR) return val;
	cs_parse_whitespace(state, false);
	if (!cs_expect_keyword(state, CS_STRVIEW("else"))) {
		cs_error_with_context(state, "Expected \"else\"");
		return val;
	}
	cs_val_t onfalse = cs_eval(state, onlytype);
	if (onfalse.loc == CS_LOC_ERR) return val;
	if ((val.type = cs_arithmetic_conversion(&ontrue.type, &onfalse.type)).levels[0] == CS_TYPE_ERROR) {
		if (!(cs_types_eq(state, &ontrue.type, &onfalse.type, false))) {
			cs_error_with_context(state,
			"Expect types of both sides to be equal!");
			return (cs_val_t){0};
		}
		val.type = ontrue.type;
	}
	return val;
}

static cs_val_t cs_eval(cs_state_t *state, bool onlytype) {
	cs_parse_whitespace(state, false);
	return cs__eval_cond(state, onlytype);
}

#endif

