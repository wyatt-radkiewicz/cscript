/*
 * Based Script
 */
#ifndef _BS_H_
#define _BS_H_

#include <limits.h>
#include <stddef.h>

typedef void(*bs_error_writer_t)(void *user, int line,
				int col, const char *msg);
struct bs_extern_fn {
	void (*pfn)(void *vm);
	const char *name;
};

#define BS_IMPL
#define BS_DEBUG

#ifdef BS_IMPL
/*
 * Utilities
 */

#ifndef BS_RECURRSION_LIMIT
# define BS_RECURRSION_LIMIT 32
#endif

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
# define BS_INLINE static inline
#else
# define BS_INLINE static
#endif

#ifdef BS_DEBUG
# include <assert.h>
# define BS_DBG_ASSERT(COND, MSG) assert((COND) && MSG)
#else
# define BS_DBG_ASSERT(COND, MSG)
#endif

#ifndef BS_NOSTDIO
# include <stdio.h>
# define BS_EPRINTLN(STR) printf("%s\n", STR)
#else
# define BS_EPRINTLN(STR)
#endif

#ifdef __GNUC__
# define BS_MEMCMP __builtin_memcmp
# define BS_MEMSET __builtin_memset
# define BS_MEMCPY __builtin_memcpy
BS_INLINE void bs_abort(const char *msg)
{
	BS_EPRINTLN(msg);
	__builtin_trap();
}
#else
/*
 * Not an actual standard conforming version of memcmp, but it's all that is
 * needed for Based Script
 */
BS_INLINE int BS_MEMCMP(const void *a, const void *b, size_t len)
{
	const unsigned char *_a, *_b;
	int ret;
	_a = a;
	_b = b;
	ret = 0;

	while (len) {
		ret |= *_a++ ^ *_b++;
		--len;
	}

	return ret;
}
BS_INLINE void BS_MEMSET(void *a, char x, size_t n)
{
	unsigned char *_a = a;
	while (n) {
		*a++ = x;
		--n;
	}
}
BS_INLINE void BS_MEMCPY(void *dest, const void *src, size_t len)
{
	const unsigned char *_dest, *_src;
	_dest = dest;
	_src = src;

	while (len) {
		*_dest++ = *_src++;
		--len;
	}
}
BS_INLINE void bs_abort(const char *msg)
{
	BS_EPRINTLN(msg);
	while (1);
}
#endif

#define BS_ARRSIZE(ARR) (sizeof(ARR)/sizeof((ARR)[0]))

/*
 * Helpful typedefs
 */
typedef unsigned char bs_byte;

/*
 * Used to explicitly and easily convey that something is either on/off
 */
typedef unsigned char bs_bool;
enum {
	BS_FALSE	= 0,
	BS_TRUE		= 1
};

/*
 * All strings in Based Script are not null-terminated so that strings can
 * point directly to the source code without having to create expensive buffers
 */
struct bs_strview {
	const char	*str;
	size_t		len;
};

BS_INLINE bs_bool bs_strview_eq(const struct bs_strview a,
				const struct bs_strview b)
{
	if (a.len != b.len) return BS_FALSE;
	return !BS_MEMCMP(a.str, b.str, a.len);
}

BS_INLINE bs_bool bs_isalpha(char x)
{
	return x >= 'a' && x <= 'z' || x >= 'A' && x <= 'Z'|| x == '_';
}
BS_INLINE bs_bool bs_isdigit(char x)
{
	return x >= '0' && x <= '9';
}
BS_INLINE bs_bool bs_tolower(char x)
{
	return x + (x >= 'A' && x <= 'Z') * 0x20;
}

/*
 * Lexer
 */
enum bs_token_type {
	BS_TOKEN_EOF,
	BS_TOKEN_NEWLINE,
	BS_TOKEN_IDENT,
	BS_TOKEN_NUMBER,
	BS_TOKEN_STRING,
	BS_TOKEN_CHAR,
	BS_TOKEN_OR,
	BS_TOKEN_AND,
	BS_TOKEN_EQEQ,
	BS_TOKEN_NEQ,
	BS_TOKEN_GT,
	BS_TOKEN_GE,
	BS_TOKEN_LT,
	BS_TOKEN_LE,
	BS_TOKEN_BITOR,
	BS_TOKEN_BITXOR,
	BS_TOKEN_BITAND,
	BS_TOKEN_SHL,
	BS_TOKEN_SHR,
	BS_TOKEN_PLUS,
	BS_TOKEN_MINUS,
	BS_TOKEN_STAR,
	BS_TOKEN_DIVIDE,
	BS_TOKEN_MODULO,
	BS_TOKEN_NOT,
	BS_TOKEN_BITNOT,
	BS_TOKEN_LPAREN,
	BS_TOKEN_RPAREN,
	BS_TOKEN_LBRACK,
	BS_TOKEN_RBRACK,
	BS_TOKEN_LBRACE,
	BS_TOKEN_RBRACE,
	BS_TOKEN_ARROW,
	BS_TOKEN_DOT,
	BS_TOKEN_COMMA,
	BS_TOKEN_EQUAL,
	BS_TOKEN_MAX
};

/*
 * Stores with is where it is in the source code.
 */
struct bs_token {
	enum bs_token_type	type;
	struct bs_strview	src;
};

/*
 * Based Script Types
 */
enum bs_typeclass {
	/* Special void type */
	BS_TYPE_VOID,

	/* POD sentinal types */
	BS_TYPE_BOOL,
	BS_TYPE_CHAR,
	BS_TYPE_I8,
	BS_TYPE_U8,
	BS_TYPE_I16,
	BS_TYPE_U16,
#ifdef BS_32BIT
	BS_TYPE_I32,
	BS_TYPE_ISIZE,
	BS_TYPE_U32,
	BS_TYPE_USIZE,
#else
	BS_TYPE_I32,
	BS_TYPE_U32,
	BS_TYPE_I64,
	BS_TYPE_ISIZE,
	BS_TYPE_U64,
	BS_TYPE_USIZE,
#endif
#ifndef BS_NOFP
	BS_TYPE_F32,
# ifndef BS_32BIT
	BS_TYPE_F64,
# endif
#endif
	/* These types continue on the type buffer */
	BS_TYPE_PTR,
	BS_TYPE_ARR,	/* next short contains the length */
	BS_TYPE_ARRPTR,
	BS_TYPE_SLICE,
	BS_TYPE_REF,

	/* User type sentinal types */
	BS_TYPE_USER,
	BS_TYPE_PFN,

	/* Used only in specific circumstances */
	BS_TYPE_TEMPLATE,

	BS_TYPE_MAX,

	/* Mutable modifier mask */
	BS_TYPE_FLAG_MUT	= 0x8000

};

#ifndef BS_PTR32
# if _WIN32 || _WIN64
#  if _WIN64
#   define BS_PTR32 0
#  else
#   define BS_PTR32 1
#  endif
# elif __GNUC__
#  if __x86_64__ || __ppc64__ || __aarch64__
#   define BS_PTR32 0
#  else
#   define BS_PTR32 1
#  endif
# elif UINTPTR_MAX > UINT_MAX
#  define BS_PTR32 0
# else
#  define BS_PTR32 1
# endif
#endif

#ifdef BS_32BIT
# define BS_INT BS_TYPE_I32
# define BS_UINT BS_TYPE_U32
typedef signed int bs_int;
typedef unsigned int bs_uint;
# ifndef BS_NOFP
typedef float bs_float;
#  define BS_FLOAT BS_TYPE_F32
# endif
# define BS_INT_MAX ((bs_int)0x7fffffff)
# define BS_INT_MIN ((bs_int)-0x80000000)
# define BS_UINT_MAX ((bs_uint)0xffffffff)
# define BS_UINT_MIN ((bs_uint)0)
# define BS_INT_ALIGN 4
# define BS_MAX_ALIGN 4
#else
# define BS_INT_MAX ((bs_int)0x7fffffffffffffff)
# define BS_INT_MIN ((bs_int)-0x8000000000000000)
# define BS_UINT_MAX ((bs_uint)0xffffffffffffffff)
# define BS_UINT_MIN ((bs_uint)0)
# define BS_INT BS_TYPE_I64
# define BS_UINT BS_TYPE_U64
# define BS_INT_ALIGN 8
# define BS_MAX_ALIGN 8
# ifndef BS_NOFP
typedef double bs_float;
#  define BS_FLOAT BS_TYPE_F64
# endif
# if __STDC_VERSION__ >= 199901L
#  include <stdint.h>
typedef int64_t bs_int;
typedef uint64_t bs_uint;
# elif defined(__GNUC__)
typedef signed long bs_int;
typedef unsigned long bs_uint;
# elif defined(_WIN32) || defined (_WIN64)
#  include <Windows.h>
typedef LONG64 bs_int;
typedef DWORD64 bs_uint;
# else
#  error "Can't find 64-bit type for 64-bit mode in Based Script header file!"
# endif
#endif

#if BS_PTR32
typedef signed int bs_iptr;
typedef unsigned int bs_uptr;
#else
# if __STDC_VERSION__ >= 199901L
#  include <stdint.h>
typedef intptr_t bs_iptr;
typedef uintptr_t bs_uptr;
# elif defined(__GNUC__)
typedef signed long bs_iptr;
typedef unsigned long bs_uptr;
# elif defined(_WIN32) || defined (_WIN64)
#  include <Windows.h>
typedef LONG64 bs_iptr;
typedef DWORD64 bs_uptr;
# else
#  error "Can't find 64-bit type for 64-bit mode in Based Script header file!"
# endif
#endif

/*
 * Types are simply sentinal-terminated pointers and each level of a type
 * is a short.
 */
typedef unsigned short bs_type_t;

/*
 * User definable struct
 */
struct bs_struct {
	struct bs_strview	*templates;
	int			ntemplates;

	struct bs_strview	*member_names;
	bs_type_t		**members;
	int			nmembers;
};

/*
 * User definable enum
 */
struct bs_enum {
	struct bs_strview	*templates;
	int			ntemplates;

	/* The variants come in the form of structs that appear after this
	 * user def */
	int			nvariants;
};

/*
 * A function signature (no templates; fully defined)
 */
struct bs_fnsig {
	bs_type_t		**params;
	int			nparams;
	bs_type_t		*ret;
};

/*
 * A function implementation contains a signature and a pointer to its place in
 * the code segment.
 */
struct bs_fnimpl {
	struct bs_fnsig		sig;
	int			loc;
};

/*
 * A Based Script function definition. Can be templated or external.
 */
struct bs_fn {
	bs_bool			ext;

	union {
		struct {
			struct bs_fnsig		sig;
			/* We use external ID here so that the compiled code can
			 * be saved to disk and then read back and interpreted
			 * right there and then. */
			int			id;
		} external;

		struct {
			struct bs_strview	*templates;
			int			ntemplates;
			struct bs_fnimpl	*impls;
			int			nimpls;
		} internal;
	} impl;
};

/*
 * Typedefs can also be templated
 */
struct bs_typedef {
	struct bs_strview	*templates;
	int			ntemplates;
	bs_type_t		*type;
};

/*
 * Userdefs can be either a user type or function definition
 */
enum bs_userdef_class {
	BS_USERDEF_STRUCT,
	BS_USERDEF_ENUMSTART,
	BS_USERDEF_TYPEDEF,
	BS_USERDEF_FN
};

/*
 * A union for userdefines. All of the structures have around the same space use
 * which means not much space is lost. Userdefs also contain the name of the def
 */
struct bs_userdef {
	enum bs_userdef_class	type;
	struct bs_strview	name;

	union {
		struct bs_struct	s;
		struct bs_enum		e;
		struct bs_typedef	t;
		struct bs_fn		f;
	} def;
};

/*
 * Location of a Based Script object in memory
 */
enum {
	BS_LOC_TYPEONLY,/* The type information is only relevant */
	BS_LOC_PCREL,	/* Relative to the interpreter program counter */
	BS_LOC_SPREL,	/* Relative to the stack */
	BS_LOC_CODEREL,	/* Relative to the start of the code segment */
	BS_LOC_ABS,	/* Absolute memory addess */
	BS_LOC_COMPTIME,/* Comptime code space */

	/* Flag to indicate indirection of location */
	BS_LOC_FLAG_IND = 0x80	
};

#ifndef BS_VARSIZE
# define BS_VARSIZE 23
#endif

/*
 * Contains type, name, and location in memory of a variable. It can also be an
 * rvalue or generally unnamed.
 */
struct bs_var {
	bs_byte			locty;
	int			loc;
	bs_type_t		type[BS_VARSIZE];
	struct bs_strview	name;
};

/*
 * A template name and its currently implemented type
 */
struct bs_tmpl_impl {
	int			nameid; /* Where the name is in strbuf */
	bs_type_t		*impl;
};

/*
 * What code mode the parser state is in. This can change so that one can see
 * if we are only finding the type of a part of code, compiling in in comptime,
 * or compiling it for runtime code.
 */
enum bs_mode {
	BS_MODE_OFF,
	BS_MODE_DEFAULT,
	BS_MODE_COMPTIME
};

/*
 * Main Based Script State
 */
struct bs {
	struct bs_token		tok;
	struct bs_strview	src, srcall;
	bs_byte			*code, *codebegin, *codeend;
	size_t			codelen;

	/* Virtual stack, compile time stack, and indent level */
	int			stack, indent;
	bs_byte			*ctstack;
	enum bs_mode		mode;

	/* This is the current scope basically */
	struct bs_var		vars[256];
	int			nvars;

	/* Currently defined templates */
	struct bs_tmpl_impl	templates[32];
	int			ntemplates;

	struct bs_strview	strbuf[256];
	int			strbuflen;
	bs_type_t		typebuf[1024];
	int			typebuflen;

	bs_type_t		*types[256];
	int			ntypes;
	struct bs_userdef	defs[256];
	int			ndefs;

	bs_error_writer_t	error_writer;
	void			*error_writer_user;
	int			errors;
	struct bs_extern_fn	*extern_fns;
	size_t			nextern_fns;
};

static void bs_init(struct bs *bs,
		struct bs_strview src,
		bs_error_writer_t error_writer,
		void *error_writer_user,
		struct bs_extern_fn *fns,
		size_t fnslen,
		bs_byte *code,
		size_t codelen) {
	BS_MEMSET(bs, 0, sizeof(*bs));
	bs->src		= src;
	bs->srcall	= src;
	bs->indent	= 0;
	bs->stack	= 0;
	bs->codeend	= code + codelen;
	bs->ctstack	= bs->codeend;
	bs->error_writer	= error_writer;
	bs->error_writer_user	= error_writer_user;
	bs->errors	= 0;
	bs->extern_fns	= fns;
	bs->nextern_fns = fnslen;
	bs->code	= code;
	bs->codebegin	= code;
	bs->codelen	= codelen;
}

/*
 * Emits an error with the error writer function, incs error count
 */
static void bs_emit_error(struct bs *bs, const char *msg) {
	const char *curr;
	int line, col;

	if (!bs->error_writer) return;

	line = 1;
	col = 1;
	curr = bs->src.str;
	while (curr < bs->srcall.str) {
		++col;
		if (*(curr++) == '\n') {
			++line;
			col = 1;
		}
	}

	++bs->errors;
	bs->error_writer(bs->error_writer_user, line, col, msg);
}

/*
 * Also consumes indents but NOT newlines
 */
static void bs_consume_whitespace(struct bs_strview *src, bs_bool skip_newline)
{
	while ((src->str[0] == ' ' || src->str[0] == '\t'
		|| (skip_newline && src->str[0] == '\n'))
		&& src->len) {
		++src->str;
		--src->len;
	}
}

/*
 * Returns the amount of indentation levels parsed
 */
static int bs_parse_indent(struct bs *bs) {
	int nspaces = 0;

	if (!bs->src.len) return 0;
	while (*bs->src.str == ' ' || *bs->src.str == '\t') {
		if (*bs->src.str == ' ') ++nspaces;
		else nspaces += 4;
		++bs->src.str;
		if (!(--bs->src.len)) break;
	}

	return nspaces / 4;
}

/*
 * Source pointer will point after the output token after this finishes.
 * Will output errors if it fails.
 */
static int bs_advance_token(struct bs *bs, bs_bool skip_newline)
{
	char start;

	bs_consume_whitespace(&bs->src, skip_newline);

	bs->tok.type = BS_TOKEN_EOF;
	bs->tok.src.str = bs->src.str;
	bs->tok.src.len = 1;
	if (!bs->src.len) return 0;

	start = bs->src.str[0];
	++bs->src.str;
	--bs->src.len;

	switch (start) {
	case '\0': bs->tok.type = BS_TOKEN_EOF; return 1;
	case '\n': bs->tok.type = BS_TOKEN_NEWLINE; break;
	case '(': bs->tok.type = BS_TOKEN_LPAREN; break;
	case ')': bs->tok.type = BS_TOKEN_RPAREN; break;
	case '[': bs->tok.type = BS_TOKEN_LBRACK; break;
	case ']': bs->tok.type = BS_TOKEN_RBRACK; break;
	case '{': bs->tok.type = BS_TOKEN_LBRACE; break;
	case '}': bs->tok.type = BS_TOKEN_RBRACE; break;
	case '^': bs->tok.type = BS_TOKEN_BITXOR; break;
	case '.': bs->tok.type = BS_TOKEN_DOT; break;
	case '+': bs->tok.type = BS_TOKEN_PLUS; break;
	case '/': bs->tok.type = BS_TOKEN_DIVIDE; break;
	case '*': bs->tok.type = BS_TOKEN_STAR; break;
	case '%': bs->tok.type = BS_TOKEN_MODULO; break;
	case ',': bs->tok.type = BS_TOKEN_COMMA; break;
	case '|':
		if (bs->src.str[0] == '|') {
			bs->tok.type = BS_TOKEN_OR;
			bs->tok.src.len = 2;
			++bs->src.str;
			if (!bs->src.len) {
				bs_emit_error(bs, "Unexpected EOF when parsing logic or token");
				return 1;
			}
			--bs->src.len;
		} else {
			bs->tok.type = BS_TOKEN_BITOR;
		}
		break;
	case '&':
		if (bs->src.str[0] == '&') {
			bs->tok.type = BS_TOKEN_AND;
			bs->tok.src.len = 2;
			++bs->src.str;
			if (!bs->src.len) {
				bs_emit_error(bs, "Unexpected EOF when parsing logic and token");
				return 1;
			}
			--bs->src.len;
		} else {
			bs->tok.type = BS_TOKEN_BITAND;
		}
		break;
	case '=':
		if (bs->src.str[0] == '=') {
			bs->tok.type = BS_TOKEN_EQEQ;
			bs->tok.src.len = 2;
			++bs->src.str;
			if (!bs->src.len) {
				bs_emit_error(bs, "Unexpected EOF when parsing equality token");
				return 1;
			}
			--bs->src.len;
		} else {
			bs->tok.type = BS_TOKEN_EQUAL;
		}
		break;
	case '-':
		if (bs->src.str[0] == '>') {
			bs->tok.type = BS_TOKEN_ARROW;
			bs->tok.src.len = 2;
			++bs->src.str;
			if (!bs->src.len) {
				bs_emit_error(bs, "Unexpected EOF when parsing arrow token");
				return 1;
			}
			--bs->src.len;
		} else {
			bs->tok.type = BS_TOKEN_MINUS;
		}
		break;
	case '>':
		if (bs->src.str[0] == '>') {
			bs->tok.type = BS_TOKEN_SHR;
			bs->tok.src.len = 2;
			++bs->src.str;
			if (!bs->src.len) {
				bs_emit_error(bs, "Unexpected EOF when parsing shift right token");
				return 1;
			}
			--bs->src.len;
		} else if (bs->src.str[0] == '=') {
			bs->tok.type = BS_TOKEN_GE;
			bs->tok.src.len = 2;
			++bs->src.str;
			if (!bs->src.len) {
				bs_emit_error(bs, "Unexpected EOF when parsing greater than or equal token");
				return 1;
			}
			--bs->src.len;
		} else {
			bs->tok.type = BS_TOKEN_GT;
		}
		break;
	case '<':
		if (bs->src.str[0] == '<') {
			bs->tok.type = BS_TOKEN_SHL;
			bs->tok.src.len = 2;
			++bs->src.str;
			if (!bs->src.len) {
				bs_emit_error(bs, "Unexpected EOF when parsing logic shift left token");
				return 1;
			}
			--bs->src.len;
		} else if (bs->src.str[0] == '=') {
			bs->tok.type = BS_TOKEN_LE;
			bs->tok.src.len = 2;
			++bs->src.str;
			if (!bs->src.len) {
				bs_emit_error(bs, "Unexpected EOF when parsing less than or equal token");
				return 1;
			}
			--bs->src.len;
		} else {
			bs->tok.type = BS_TOKEN_LT;
		}
		break;
	case '"': case '\'':
		++bs->tok.src.str;
		bs->tok.src.len = 0;
		bs->tok.type = start == '"' ? BS_TOKEN_STRING : BS_TOKEN_CHAR;
		while (bs->src.str[0] != start) {
			++bs->src.str;
			++bs->tok.src.len;
			if (bs->src.str[0] == '\\') {
				++bs->src.str;
				++bs->tok.src.len;
				if (!bs->src.len--) {
					bs->src.len = 0;
					if (start == '"') bs_emit_error(bs, "Unexpected EOF when parsing string token");
					else bs_emit_error(bs, "Unexpected EOF when parsing character token");
					return 1;
				}
			}
			if (!bs->src.len--) {
				bs->src.len = 0;
				if (start == '"') bs_emit_error(bs, "Unexpected EOF when parsing string token");
				else bs_emit_error(bs, "Unexpected EOF when parsing character token");
				return 1;
			}
		}
		break;
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		bs->tok.type = BS_TOKEN_NUMBER;
		while (bs_isdigit(bs->src.str[0]) || bs->src.str[0] == 'x'
			|| (bs_tolower(bs->src.str[0]) >= 'a'
			&& bs_tolower(bs->src.str[0]) <= 'f')
			|| bs->src.str[0] == '.') {
			++bs->src.str;
			++bs->tok.src.len;
			if (!--bs->src.len) return 0;
		}
		break;
	default:
		if (!bs_isalpha(start)) return 1;
		bs->tok.type = BS_TOKEN_IDENT;
		while (bs_isalpha(bs->src.str[0])
			|| bs_isdigit(bs->src.str[0])
			|| bs->src.str[0] == '_') {
			++bs->src.str;
			++bs->tok.src.len;
			if (!--bs->src.len) return 0;
		}
		break;
	}

	return 0;
}

static int bs_parse_type(struct bs *bs, bs_type_t *type,
			int *len, int max, int depth);

/*
 * Parse non sentinal types like pointers and qualifiers like const and mut
 */
static int bs_parse_type_levels(struct bs *bs, bs_type_t *type,
				int *len, const int max, const int depth)
{
	/* String view constants */
	static const struct bs_strview _bs_strview_const = { "const", 5 };
	static const struct bs_strview _bs_strview_mut = { "mut", 3 };

	/* Get reset each time a pointer, etc is found so that there is no code
	 * that looks like "const mut &[] u8" */
	int qualifiers_seen = 0;

	if (bs_advance_token(bs, BS_FALSE)) return 1;
	type[*len] = 0;

	while (BS_TRUE) {
		const struct bs_strview backup = bs->src;

		if (*len >= max) {
			bs_emit_error(bs, "bs_parse_type_levels: Ran out of type buffer storage!");
			return 1;
		}

		if (bs->tok.type == BS_TOKEN_IDENT
			&& bs_strview_eq(bs->tok.src, _bs_strview_const)) {
			++qualifiers_seen;
			type[*len] &= ~BS_TYPE_FLAG_MUT;
		} else if (bs->tok.type == BS_TOKEN_IDENT
			&& bs_strview_eq(bs->tok.src, _bs_strview_mut)) {
			++qualifiers_seen;
			type[*len] |= BS_TYPE_FLAG_MUT;
		} else if (bs->tok.type == BS_TOKEN_STAR) {
			if (bs_advance_token(bs, BS_FALSE)) return 1;
			if (bs->tok.type == BS_TOKEN_LBRACK) {
				if (bs_advance_token(bs, BS_FALSE)) return 1;
				if (bs->tok.type != BS_TOKEN_RBRACK) {
					bs_emit_error(bs, "Pointer to arrays shouldn't have length embedded into type.");
					return 1;
				}
				type[*len] |= BS_TYPE_ARRPTR;
			} else {
				bs->src = backup;
				type[*len] |= BS_TYPE_PTR;
			}
			qualifiers_seen = 0;
			if (*len >= max) {
				bs_emit_error(bs, "Ran out of storage buffer space");
				return -1;
			}
			type[++(*len)] = 0;
		} else if (bs->tok.type == BS_TOKEN_BITAND) {
			if (bs_advance_token(bs, BS_FALSE)) return 1;
			if (bs->tok.type == BS_TOKEN_LBRACK) {
				if (bs_advance_token(bs, BS_FALSE)) return 1;
				if (bs->tok.type != BS_TOKEN_RBRACK) {
					bs_emit_error(bs, "Slices shouldn't have length embedded into type.");
					return 1;
				}
				type[*len] |= BS_TYPE_SLICE;
			} else {
				bs->src = backup;
				type[*len] |= BS_TYPE_REF;
			}
			qualifiers_seen = 0;
			if (*len >= max) {
				bs_emit_error(bs, "Ran out of storage buffer space");
				return -1;
			}
			type[++(*len)] = 0;
		} else if (bs->tok.type == BS_TOKEN_LBRACK) {
			BS_DBG_ASSERT(BS_FALSE, "TODO: Implement array length in types");
		} else {
			return 0;
		}

		if (bs_advance_token(bs, BS_FALSE)) return 1;
		if (qualifiers_seen > 1) {
			bs_emit_error(bs, "Only one qualifier is expected when parsing type!");
			return 1;
		}
	}
}

static int bs_parse_type_integer(struct bs *bs, bs_type_t *type,
				int *len, const int max, const int depth)
{
	/* String view constants */
	static const struct bs_strview _bs_strview_usize = { "usize", 5 };
	static const struct bs_strview _bs_strview_isize = { "isize", 5 };

	const bs_bool u = bs->tok.src.str[0] == 'u';

	if (*len >= max) {
		bs_emit_error(bs, "bs_parse_type_integer: Ran out of type buffer storage!");
		return 1;
	}

	if (bs->tok.src.len == 2 && bs->tok.src.str[1] == '8') {
		type[(*len)++] |= u ? BS_TYPE_U8 : BS_TYPE_I8;
	} else if (bs->tok.src.len == 3) {
		if (bs->tok.src.str[1] == '1' && bs->tok.src.str[2] == '6') {
			type[(*len)++] |= u ? BS_TYPE_U16 : BS_TYPE_I16;
		} else if (bs->tok.src.str[1] == '3' && bs->tok.src.str[2] == '2') {
			type[(*len)++] |= u ? BS_TYPE_U32 : BS_TYPE_I32;
		}
#ifndef BS_32BIT
		else if (bs->tok.src.str[1] == '6' && bs->tok.src.str[2] == '4') {
			type[(*len)++] |= u ? BS_TYPE_U64 : BS_TYPE_I64;
		}
#endif
		else {
			return -1;
		}
	} else if (bs_strview_eq(bs->tok.src, _bs_strview_usize)) {
		type[(*len)++] |= BS_TYPE_USIZE;
	} else if (bs_strview_eq(bs->tok.src, _bs_strview_isize)) {
		type[(*len)++] |= BS_TYPE_ISIZE;
	} else {
		return -1;
	}

	if (bs_advance_token(bs, BS_FALSE)) return 1;
	return 0;
}

/*
 * Get how many type levels/slots a type takes up. If len is 0, then the type is
 * not a correct type and is corrupted.
 */
static int bs_type_len(const struct bs *bs, const bs_type_t *type,
			const int rdepth)
{
	const struct bs_userdef *def;
	int len = 0, ntemplates = 0;

	if (rdepth > BS_RECURRSION_LIMIT) {
		bs_abort("bs_type_len: Max recurrsion depth hit!");
	}

	while (BS_TRUE) {
		switch (*type & ~BS_TYPE_FLAG_MUT) {
		case BS_TYPE_REF: case BS_TYPE_ARRPTR:
		case BS_TYPE_PTR: case BS_TYPE_SLICE:
			++len;
			continue;
		case BS_TYPE_ARR:
			len += 2;
			continue;
		case BS_TYPE_USER:
			++len;
			goto compute_templates;
		default: return len + 1;
		}
	}

compute_templates:
	def = bs->defs + type[len++];
	if (def->type == BS_USERDEF_STRUCT)
		ntemplates = def->def.s.ntemplates;
	else if (def->type == BS_USERDEF_ENUMSTART)
		ntemplates = def->def.e.ntemplates;
	BS_DBG_ASSERT(def->type != BS_USERDEF_TYPEDEF, "Can not have typedefs in finalized types!");

	/* No templates to process */
	if (ntemplates == 0) return len;

	while (ntemplates) {
		const int tmp = bs_type_len(bs, type, rdepth + 1);
		BS_DBG_ASSERT(tmp, "bs_type_len: Expected type for template param");
		type += tmp;
		len += tmp;
		--ntemplates;
	}

	return len;
}

/*
 * Parsing typedefs is quite difficult since the templates of the typedef can
 * be used to further specialize the underlying struct/enum if there is one
 */
static int bs_parse_type_template_typedef(struct bs *bs, bs_type_t *type,
				int *len, const int max,
				const struct bs_userdef *def, const int rdepth)
{
	struct bs_strview tmpls[16], backup;
	const bs_type_t *iter;
	int ntmpls, depth, i, tmp;

	/* Parse the templates and get the areas */
	if (bs->tok.type != BS_TOKEN_LT) {
		bs_emit_error(bs, "Expected template parameter list <...>");
		return 1;
	}

	/* ntmpls here is used as sort of a iterator index */
	/* Here we are collecting up the source areas of every template
	 * parameter */
	i = 1;
	tmpls[0] = bs->src;
	depth = 1;
	while (depth) {
		if (bs_advance_token(bs, BS_FALSE)) return -1;
		if (bs->tok.type == BS_TOKEN_LT) ++depth;
		if (bs->tok.type == BS_TOKEN_GT) --depth;
		if (bs->tok.type == BS_TOKEN_COMMA && depth == 1) {
			if (i == BS_ARRSIZE(tmpls)) {
				bs_emit_error(bs, "Type has too many template parameters");
				return -1;
			}
			tmpls[i++] = bs->src;
		}
	}
	backup = bs->src;
	if (bs_advance_token(bs, BS_FALSE)) return -1; /* Get past last > */

	/* Skip to where the template type list SHOULD be for the typedef
	 * while also copying the type over */
	iter = def->def.t.type;
	while (BS_TRUE) {
		switch (*iter & ~BS_TYPE_FLAG_MUT) {
		case BS_TYPE_ARR:
			if (*len >= max) goto storage_error;
			type[(*len)++] = *iter++;
			/* Fall Through */
		case BS_TYPE_PTR: case BS_TYPE_REF:
		case BS_TYPE_ARRPTR: case BS_TYPE_SLICE:
			if (*len >= max) goto storage_error;
			type[(*len)++] = *iter++;
			continue;
		default: break; /* Then continue to the break under here. */
		}
		break;
	}

	/* Now we use ntmpls to get the number of underlying templates */
	if (*len >= max) goto storage_error;
	tmp = *iter++;
	type[(*len)++] = tmp;
	if ((tmp & ~BS_TYPE_FLAG_MUT) != BS_TYPE_USER) {
		return 0;
	} else {
		int id = *iter++;
		if (*len >= max) goto storage_error;
		type[(*len)++] = id;
		switch (bs->defs[id].type) {
		case BS_USERDEF_ENUMSTART:
			ntmpls = bs->defs[id].def.e.ntemplates;
			break;
		case BS_USERDEF_STRUCT:
			ntmpls = bs->defs[id].def.s.ntemplates;
			break;
		default:
			ntmpls = 0;
			break;
		}
	}

	/* Go through the underlying types and replace templates with the
	 * actual type that we can parse. */
	while (ntmpls) {
		switch (*iter & ~BS_TYPE_FLAG_MUT) {
		case BS_TYPE_ARR: case BS_TYPE_PTR: case BS_TYPE_ARRPTR:
		case BS_TYPE_SLICE: case BS_TYPE_REF: break;
		default: --ntmpls;
		}
		switch (*iter & ~BS_TYPE_FLAG_MUT) {
		case BS_TYPE_USER: case BS_TYPE_ARR:
			if (*len >= max) goto storage_error;
			type[(*len)++] = *iter++;
		default:
			if (*len >= max) goto storage_error;
			type[(*len)++] = *iter++;
			break;
		case BS_TYPE_TEMPLATE:
			++iter; /* Goto the template ID */
			tmp = def->def.t.templates - bs->strbuf;
			BS_DBG_ASSERT(*iter >= tmp, "Template ID out of bounds");
			BS_DBG_ASSERT(*iter < tmp + def->def.t.ntemplates,
				"Template ID out of bounds");
			bs->src = tmpls[*iter - tmp];
			if (bs_parse_type(bs, type, len, max, rdepth + 1))
				return -1;
			++iter;
		}
	}
	
	bs->src = backup;
	return 0;
storage_error:
	bs_emit_error(bs, "bs_parse_type_typedef: Ran out of type buffer storage!");
	return 1;
}
static int bs_parse_type_typedef(struct bs *bs, bs_type_t *type,
				int *len, const int max,
				const struct bs_userdef *def, const int rdepth)
{
	const bs_type_t *tdef;
	int tmplen;

	if (bs_advance_token(bs, BS_FALSE)) return -1;
	if (def->def.t.ntemplates != 0)
		return bs_parse_type_template_typedef(bs, type, len, max,
							def, rdepth);

	if (!(tmplen = bs_type_len(bs, def->def.t.type, 0)))
		return 1;
	if (max - *len < tmplen) {
		bs_emit_error(bs, "bs_parse_type_typedef: Ran out of type buffer storage!");
		return 1;
	}
	tdef = def->def.t.type;

	/* Keep previous modifier? */
	type[(*len)++] |= *tdef++ & ~BS_TYPE_FLAG_MUT;
	--tmplen;
	BS_MEMCPY(type + *len, tdef, sizeof(*tdef) * tmplen);
	*len += tmplen;

	return 0;
}

/*
 * Parse type and advance source pointer
 */
static int bs_parse_type(struct bs *bs, bs_type_t *type,
			int *len, int max, int depth)
{
	/* String view constants */
	static const struct bs_strview _bs_strview_char = { "char", 4 };
	static const struct bs_strview _bs_strview_bool = { "bool", 4 };

	int i, ntemplates;

	if (depth >= BS_RECURRSION_LIMIT) {
		bs_emit_error(bs, "Max recurrsion limit hit");
		return -1;
	}

	if (*len >= max) goto storage_error;
	if (bs_parse_type_levels(bs, type, len, max, depth)) return 1;
	if (bs->tok.type != BS_TOKEN_IDENT
		&& bs->tok.type != BS_TOKEN_LPAREN) {
		if (*len >= max) goto storage_error;
		type[(*len)++] = BS_TYPE_VOID;
		return 0;
	}

	/* Search for normal POD sentinal types */
	switch (bs->tok.src.str[0]) {
	case 'u': case 'i':
		if (!bs_parse_type_integer(bs, type, len, max, depth))
			return 0;
		break;
#ifndef BS_NOFP
	case 'f':
		if (bs->tok.src.len != 3) break;
		if (bs->tok.src.str[1] == '3' && bs->tok.src.str[2] == '2') {
			type[(*len)++] |= BS_TYPE_F32;
			if (bs_advance_token(bs, BS_FALSE)) return 1;
			return 0;
		}
# ifndef BS_32BIT
		else if (bs->tok.src.str[1] == '6'
			&& bs->tok.src.str[2] == '4') {
			type[(*len)++] |= BS_TYPE_F64;
			if (bs_advance_token(bs, BS_FALSE)) return 1;
			return 0;
		}
# endif
#endif
	case 'c':
		if (!bs_strview_eq(bs->tok.src, _bs_strview_char)) break;
		type[(*len)++] |= BS_TYPE_CHAR;
		if (bs_advance_token(bs, BS_FALSE)) return 1;
		return 0;
	case 'b':
		if (!bs_strview_eq(bs->tok.src, _bs_strview_bool)) break;
		type[(*len)++] |= BS_TYPE_BOOL;
		if (bs_advance_token(bs, BS_FALSE)) return 1;
		return 0;
	}

	/* Is it a pointer to function? */
	if (bs->tok.type == BS_TOKEN_LPAREN) {
		bs_type_t *pfnlen;

		if (max - *len < 2) goto storage_error;
		type[(*len)++] |= BS_TYPE_PFN;
		pfnlen = type + (*len)++;

		*pfnlen = 0;
		while (bs->tok.type != BS_TOKEN_RPAREN) {
			if (bs_parse_type(bs, type, len, max, depth + 1))
				return -1;
			++(*pfnlen);
		}
		if (bs_parse_type(bs, type, len, max, depth + 1))
			return -1;

		return 0;
	}

	/* Look for usertypes */
	if (bs->tok.type != BS_TOKEN_IDENT) {
		bs_emit_error(bs, "Expected identifier while parsing type");
		return 1;
	}

	ntemplates = 0;
	for (i = 0; i < bs->ndefs; i++) {
		if (!bs_strview_eq(bs->defs[i].name, bs->tok.src)) continue;
		if (bs->defs[i].type == BS_USERDEF_TYPEDEF) {
			if (bs_parse_type_typedef(bs, type, len, max,
						bs->defs + i, depth))
				return -1;
			return 0;
		}
		if (bs->defs[i].type == BS_USERDEF_ENUMSTART)
			ntemplates = bs->defs[i].def.e.ntemplates;
		else if (bs->defs[i].type == BS_USERDEF_STRUCT)
			ntemplates = bs->defs[i].def.s.ntemplates;
		if (*len + 2 > max) goto storage_error;
		type[(*len)++] |= BS_TYPE_USER;
		BS_DBG_ASSERT(i <= 0xffff, "ID Passed max type ID!");
		type[(*len)++] = i;
		goto parse_template_params;
	}

	/* Look for currently defined templates */
	for (i = 0; i < bs->ntemplates; i++) {
		bs_type_t *tmpl;
		int tmp;

		if (!bs_strview_eq(bs->strbuf[bs->templates[i].nameid], bs->tok.src))
			continue;
		tmpl = bs->templates[i].impl;
		if (!(tmp = bs_type_len(bs, tmpl, 0)))
			return -1;

		if (max - *len > tmp) goto storage_error;
		/* Keep previous modifier? */
		type[(*len)++] |= *tmpl++ & ~BS_TYPE_FLAG_MUT;
		--tmp;
		BS_MEMCPY(type + *len, tmpl, sizeof(*tmpl) * tmp);
		*len += tmp;

		return 0;
	}

	bs_emit_error(bs, "Undeclared identifier");
	return -1;

parse_template_params:
	/* Add additional template parameters */
	if (bs_advance_token(bs, BS_FALSE)) return -1;
	if (ntemplates == 0) return 0;
	if (bs->tok.type != BS_TOKEN_LT) {
		bs_emit_error(bs, "Expected template parameter list");
		return -1;
	}

	while (bs->tok.type != BS_TOKEN_GT) {
		if (bs_parse_type(bs, type, len, max, depth + 1)) return -1;
		if (bs->tok.type == BS_TOKEN_COMMA) {
			if (bs_advance_token(bs, BS_FALSE)) return -1;
		}
	}
	if (bs_advance_token(bs, BS_FALSE)) return -1;
	else return 0;

storage_error:
	bs_emit_error(bs, "bs_parse_type: Ran out of type buffer storage!");
	return 1;
}

/*
 * Parse type and advance source pointer
 */
static int bs_parse_typedef(struct bs *bs)
{
	static const struct bs_strview _type_str = { "type", 4 };

	bs_type_t tmpl_types[16][2];
	struct bs_strview *templates, backup;
	struct bs_userdef *def;
	int ntemplates;

	const int tmplbase = bs->ntemplates;

	BS_DBG_ASSERT(bs->tok.type == BS_TOKEN_IDENT
		&& bs_strview_eq(bs->tok.src, _type_str), "Expected 'type'");

	/* Allocate the definition */
	if (bs->ndefs == BS_ARRSIZE(bs->defs)) {
		bs_emit_error(bs, "Hit def buffer max");
		return -1;
	}
	def = bs->defs + bs->ndefs++;

	if (bs_advance_token(bs, BS_FALSE)) return -1;
	def->name = bs->tok.src;
	backup = bs->src;
	if (bs_advance_token(bs, BS_FALSE)) return -1;

	/* Parse template parameters */
	templates = NULL;
	ntemplates = 0;
	if (bs->tok.type == BS_TOKEN_LT) {
		templates = bs->strbuf + bs->strbuflen;
		if (bs_advance_token(bs, BS_FALSE)) return -1;
		while (bs->tok.type != BS_TOKEN_GT) {
			if (bs->strbuflen == BS_ARRSIZE(bs->strbuf)) {
				bs_emit_error(bs, "Hit string buffer max");
				return -1;
			}
			if (ntemplates == BS_ARRSIZE(tmpl_types)) {
				bs_emit_error(bs, "Hit max templates");
				return -1;
			}
			if (bs->ntemplates == BS_ARRSIZE(bs->templates)) {
				bs_emit_error(bs, "Hit max active templates");
				return -1;
			}
			bs->templates[bs->ntemplates].nameid = bs->strbuflen;
			tmpl_types[ntemplates][0] = BS_TYPE_TEMPLATE;
			tmpl_types[ntemplates][1] = bs->strbuflen;
			bs->templates[bs->ntemplates++].impl =
				tmpl_types[ntemplates++];
			bs->strbuf[bs->strbuflen++] = bs->tok.src;
			if (bs_advance_token(bs, BS_FALSE)) return -1;
			if (bs->tok.type == BS_TOKEN_COMMA
				&& bs_advance_token(bs, BS_FALSE)) return -1;
		}
		backup = bs->src;
		if (bs_advance_token(bs, BS_FALSE)) return -1;
	}

	/* Do a typedef */
	if (bs->tok.type != BS_TOKEN_NEWLINE) {
		int len = 0;
		bs->src = backup;
		def->def.t.type = bs->typebuf + bs->typebuflen;
		if (bs_parse_type(bs, def->def.t.type, &len,
				BS_ARRSIZE(bs->typebuf) - bs->typebuflen, 0))
			return -1;
		bs->typebuflen += len;
		def->type = BS_USERDEF_TYPEDEF;
		def->def.t.templates = templates;
		def->def.t.ntemplates = ntemplates;

		bs->ntemplates = tmplbase;
		return 0;
	}
	
	/* Else, do a struct */
	bs->indent = 1;
	if (bs_parse_indent(bs) != bs->indent) {
		bs_emit_error(bs, "Ident level incorrect after struct definition");
		return -1;
	}

	def->type = BS_USERDEF_STRUCT;
	def->def.s.templates = templates;
	def->def.s.ntemplates = ntemplates;
	def->def.s.members = bs->types + bs->ntypes;
	def->def.s.member_names = bs->strbuf + bs->strbuflen;
	def->def.s.nmembers = 0;
	do {
		int len = 0;

		if (bs_advance_token(bs, BS_FALSE)) return -1;
		if (bs->tok.type != BS_TOKEN_IDENT) {
			bs_emit_error(bs, "Expected struct field name");
			return -1;
		}
		if (bs->strbuflen == BS_ARRSIZE(bs->strbuf)) {
			bs_emit_error(bs, "String buffer max hit");
			return -1;
		}
		if (bs->ntypes == BS_ARRSIZE(bs->types)) {
			bs_emit_error(bs, "Type pointer buffer max hit");
			return -1;
		}

		bs->strbuf[bs->strbuflen++] = bs->tok.src;
		bs->types[bs->ntypes] = bs->typebuf + bs->typebuflen;
		if (bs_parse_type(bs, bs->types[bs->ntypes++], &len,
				BS_ARRSIZE(bs->typebuf) - bs->typebuflen, 0))
			return -1;
		bs->typebuflen += len;
		++def->def.s.nmembers;
	} while (bs_parse_indent(bs) == bs->indent);

	bs->ntemplates = tmplbase;
	bs->indent = 0;
	return 0;
}

struct bs_parser_backup {
	enum bs_mode		mode;
	struct bs_strview	src;
	struct bs_token		tok;
};

static void bs_parser_backup_init(struct bs *bs, struct bs_parser_backup *b)
{
	b->src = bs->src;
	b->tok = bs->tok;
	b->mode = bs->mode;
}

static void bs_parser_backup_restore(struct bs *bs, struct bs_parser_backup *b)
{
	bs->src = b->src;
	bs->tok = b->tok;
	bs->mode = b->mode;
}

/*
 * Based Script code emmiter
 */

static bs_bool bs_is_arithmetic(struct bs *bs, const bs_type_t *type)
{
	switch (*type) {
	case BS_TYPE_CHAR: case BS_TYPE_BOOL: case BS_TYPE_I8: case BS_TYPE_U8:
	case BS_TYPE_I16: case BS_TYPE_U16: case BS_TYPE_I32: case BS_TYPE_U32:
#ifndef BS_32BIT
	case BS_TYPE_I64: case BS_TYPE_U64:
# ifndef BS_NOFP
	case BS_TYPE_F32: case BS_TYPE_F64:
# endif
#elif !defined(BS_NOFP)
	case BS_TYPE_F32:
#endif
		return BS_TRUE;
	default:
		return BS_FALSE;
	}
}

static int bs_promote_arithmetic(struct bs *bs, const bs_type_t *type,
				bs_type_t *out)
{
	switch (*type) {
	case BS_TYPE_CHAR: case BS_TYPE_BOOL: case BS_TYPE_I8:
	case BS_TYPE_I16: case BS_TYPE_I32:
#ifndef BS_32BIT
	case BS_TYPE_I64:
#endif
		*out = BS_INT;
		return 0;
	case BS_TYPE_U8: case BS_TYPE_U16: case BS_TYPE_U32:
#ifndef BS_32BIT
	case BS_TYPE_U64:
#endif
		*out = BS_UINT;
		return 0;
#ifndef BS_NOFP
	case BS_TYPE_F32:
# ifndef BS_32BIT
	case BS_TYPE_F64:
# endif
		*out = BS_FLOAT;
		return 0;
#endif
	default:
		return -1;
	}
}

#define BS_ALIGNDU(X, A) ((X) / (A) * (A))
#define BS_ALIGNUU(X, A) ((((X) - 1) / (A) + 1) * (A))

static int bs_push_data(void *boundary, bs_byte **out, const void *data,
			size_t len, size_t align)
{
	bs_byte *newloc = (bs_byte *)BS_ALIGNDU(
		(bs_uptr)*out - len, align);

	if ((void *)newloc < boundary) return -1;
	*out = newloc;
	BS_MEMCPY(*out, data, len);
	return 0;
}

static int bs_pop_data(void *boundary, bs_byte **ptr, void *out,
			size_t len, size_t tonext)
{
	if ((void *)(*ptr + tonext) > boundary) return -1;
	BS_MEMCPY(out, *ptr, len);
	*ptr += tonext;
	return 0;
}
/*
 * NOTE: Only updates loctype and loc of out variable
 */
static int bs_compile_push_data(struct bs *bs, struct bs_var *out,
				const void *data, size_t len, size_t align)
{
	switch (bs->mode) {
	case BS_MODE_OFF:
		out->locty = BS_LOC_TYPEONLY;
		break;
	case BS_MODE_DEFAULT:
		out->locty = BS_LOC_SPREL;
		out->loc = bs->stack;
		break;
	case BS_MODE_COMPTIME:
		out->locty = BS_LOC_COMPTIME;
		if (bs_push_data(bs->codebegin, &bs->ctstack, data, len, align)) {
			bs_emit_error(bs, "Ran out of comptime stack space!");
			return -1;
		}
		out->loc = bs->codeend - bs->ctstack;
		break;
	}
	return 0;
}

/*
 * Since objects may be aligned oddly on the stack, tonext specifies by how
 * much to change the next pointer
 */
static int bs_compile_pop_data(struct bs *bs, void *out,
				size_t len, size_t tonext)
{
	switch (bs->mode) {
	case BS_MODE_OFF:
		break;
	case BS_MODE_DEFAULT:
		break;
	case BS_MODE_COMPTIME:
		if (bs_pop_data(bs->codeend, &bs->ctstack, out, len, tonext)) {
			bs_emit_error(bs, "Comptime stack underflow!");
			return -1;
		}
		break;
	}
	return 0;
}

static int bs_compile_cast(struct bs *bs, struct bs_var *var,
			const bs_type_t *newtype)
{
#ifndef BS_NOFP
	bs_float f;
	bs_uint u;
	bs_int i;
#endif

	if (!bs_is_arithmetic(bs, var->type)
		|| !bs_is_arithmetic(bs, newtype)) {
		bs_emit_error(bs, "Expected arithmetic types in cast!");
		return -1;
	}

	if (bs->mode == BS_MODE_OFF) return 0;
#ifndef BS_NOFP
	if (*newtype == BS_FLOAT && var->type[0] != BS_FLOAT) {
		if (var->type[0] == BS_UINT) {
			if (bs_compile_pop_data(bs, &u, sizeof(u), sizeof(u))) return -1;
			f = (bs_float)u;
		} else {
			if (bs_compile_pop_data(bs, &i, sizeof(i), sizeof(i))) return -1;
			f = (bs_float)i;
		}
		return bs_compile_push_data(bs, var, &f, sizeof(f), sizeof(f));
	} else if (*newtype != BS_FLOAT && var->type[0] == BS_FLOAT) {
		if (bs_compile_pop_data(bs, &f, sizeof(f), sizeof(f))) return -1;
		if (*newtype == BS_UINT) {
			u = (bs_uint)f;
			if (bs_compile_push_data(bs, var, &u, sizeof(u), sizeof(u))) return -1;
		} else {
			i = (bs_int)f;
			if (bs_compile_push_data(bs, var, &i, sizeof(i), sizeof(i))) return -1;
		}
	}
#endif
	var->type[0] = *newtype;
	return 0;
}

static int bs_arithmetic_convert(struct bs *bs, const bs_type_t *left,
				const bs_type_t *right, bs_type_t *out)
{
	bs_type_t pl, pr;
	if (bs_promote_arithmetic(bs, left, &pl)) return -1;
	if (bs_promote_arithmetic(bs, right, &pr)) return -1;
#ifndef BS_NOFP
	if (pl == BS_FLOAT || pr == BS_FLOAT) *out = BS_FLOAT;
	else if (pl == BS_UINT || pr == BS_UINT) *out = BS_UINT;
#else
	if (pl == BS_UINT || pr == BS_UINT) *out = BS_UINT;
#endif
	else *out = BS_INT;
	return 0;
}

static int bs_compile_math(struct bs *bs, bs_type_t type, int mathop)
{
	union {
		bs_uint u;
		bs_int i;
		bs_float f;
	} l, r;
	size_t size;

	switch (bs->mode) {
	case BS_MODE_OFF: break;
	case BS_MODE_DEFAULT:
		break;
	case BS_MODE_COMPTIME:
		if (type == BS_UINT || type == BS_INT) size = sizeof(l.u);
#ifndef BS_NOFP
		else if (type == BS_FLOAT) size = sizeof(l.f);
#endif
		else bs_abort("expected int,uint, or fp in comptime expr");

		if (bs_pop_data(bs->codeend, &bs->ctstack, &r, size, size)
			|| bs_pop_data(bs->codeend, &bs->ctstack, &l, size, size)) {
			bs_emit_error(bs, "Comptime stack underflow!");
			return -1;
		}

		if (type == BS_UINT) l.u += r.u;
		else if (type == BS_INT) l.i += r.i;
#ifndef BS_NOFP
		else if (type == BS_FLOAT) l.f += r.f;
#endif

		if (bs_push_data(bs->codebegin, &bs->ctstack, &l, size, size)) {
			bs_emit_error(bs, "Comptime stack overflow!");
			return -1;
		}
		break;
	}

	return 0;
}

/*
 * Based Script expression compiler
 */
enum bs_prec {
	BS_PREC_FACTOR,
	BS_PREC_GROUPING,
	BS_PREC_POSTFIX,
	BS_PREC_BUILTIN,
	BS_PREC_PREFIX,
	BS_PREC_MUL,
	BS_PREC_ADD,
	BS_PREC_SHIFT,
	BS_PREC_BITAND,
	BS_PREC_BITXOR,
	BS_PREC_BITOR,
	BS_PREC_REL,
	BS_PREC_EQ,
	BS_PREC_AND,
	BS_PREC_OR,
	BS_PREC_COND,
	BS_PREC_ASSIGN,
	BS_PREC_FULL = BS_PREC_ASSIGN,
	BS_PREC_NONE
};

#define BS_EXPR_TYBUF_SIZE 64

typedef int(*bs_parse_expr_func_t)(struct bs *bs, enum bs_prec prec,
				struct bs_var *out, int depth);

struct bs_expr_rule {
	bs_parse_expr_func_t	prefix;
	bs_parse_expr_func_t	postfix;
	enum bs_prec		prec;
};

static int bs_parse_expr(struct bs *bs, enum bs_prec prec,
			struct bs_var *out, int depth);
static int bs_parse_number(struct bs *bs, enum bs_prec prec,
			struct bs_var *out, int depth);
static int bs_parse_add(struct bs *bs, enum bs_prec prec,
			struct bs_var *out, int depth);

static const struct bs_expr_rule bs_expr_rules[BS_TOKEN_MAX] = {
/*	  prefix	postfix		prec	     */
	{ NULL,		NULL,		BS_PREC_NONE },	/* BS_TOKEN_EOF */
	{ NULL,		NULL,		BS_PREC_NONE },	/* BS_TOKEN_NEWLINE */
	{ NULL,		NULL,		BS_PREC_NONE },	/* BS_TOKEN_IDENT */
	{ bs_parse_number, NULL,	BS_PREC_FACTOR },	/* BS_TOKEN_NUMBER */
	{ NULL,		NULL,		BS_PREC_NONE },	/* BS_TOKEN_STRING */
	{ NULL,		NULL,		BS_PREC_NONE },	/* BS_TOKEN_CHAR */
	{ NULL,		NULL,		BS_PREC_NONE },	/* BS_TOKEN_OR */
	{ NULL,		NULL,		BS_PREC_NONE },	/* BS_TOKEN_AND */
	{ NULL,		NULL,		BS_PREC_NONE },	/* BS_TOKEN_EQEQ */
	{ NULL,		NULL,		BS_PREC_NONE },	/* BS_TOKEN_NEQ */
	{ NULL,		NULL,		BS_PREC_NONE },	/* BS_TOKEN_GT */
	{ NULL,		NULL,		BS_PREC_NONE },	/* BS_TOKEN_GE */
	{ NULL,		NULL,		BS_PREC_NONE },	/* BS_TOKEN_LT */
	{ NULL,		NULL,		BS_PREC_NONE },	/* BS_TOKEN_LE */
	{ NULL,		NULL,		BS_PREC_NONE },	/* BS_TOKEN_BITOR */
	{ NULL,		NULL,		BS_PREC_NONE },	/* BS_TOKEN_BITXOR */
	{ NULL,		NULL,		BS_PREC_NONE },	/* BS_TOKEN_BITAND */
	{ NULL,		NULL,		BS_PREC_NONE },	/* BS_TOKEN_SHL */
	{ NULL,		NULL,		BS_PREC_NONE },	/* BS_TOKEN_SHR */
	{ NULL,		bs_parse_add,	BS_PREC_ADD },	/* BS_TOKEN_PLUS */
	{ NULL,		NULL,		BS_PREC_NONE }, /* BS_TOKEN_MINUS */
	{ NULL,		NULL,		BS_PREC_NONE }, /* BS_TOKEN_STAR */
	{ NULL,		NULL,		BS_PREC_NONE }, /* BS_TOKEN_DIVIDE */
	{ NULL,		NULL,		BS_PREC_NONE }, /* BS_TOKEN_MODULO */
	{ NULL,		NULL,		BS_PREC_NONE }, /* BS_TOKEN_NOT */
	{ NULL,		NULL,		BS_PREC_NONE }, /* BS_TOKEN_BITNOT */
	{ NULL,		NULL,		BS_PREC_NONE }, /* BS_TOKEN_LPAREN */
	{ NULL,		NULL,		BS_PREC_NONE }, /* BS_TOKEN_RPAREN */
	{ NULL,		NULL,		BS_PREC_NONE }, /* BS_TOKEN_LBRACK */
	{ NULL,		NULL,		BS_PREC_NONE }, /* BS_TOKEN_RBRACK */
	{ NULL,		NULL,		BS_PREC_NONE }, /* BS_TOKEN_LBRACE */
	{ NULL,		NULL,		BS_PREC_NONE }, /* BS_TOKEN_RBRACE */
	{ NULL,		NULL,		BS_PREC_NONE }, /* BS_TOKEN_ARROW */
	{ NULL,		NULL,		BS_PREC_NONE }, /* BS_TOKEN_DOT */
	{ NULL,		NULL,		BS_PREC_NONE }, /* BS_TOKEN_COMMA */
	{ NULL,		NULL,		BS_PREC_NONE }, /* BS_TOKEN_EQUAL */
};

static int bs_parse_uint(struct bs *bs, bs_uint *out)
{
	bs_uint pow = 1, last = 0; /* last is to detect overflow */
	bs_bool hex = BS_FALSE;
	int i;

	struct bs_token tok = bs->tok;

	if (bs->tok.src.len > 2 && bs->tok.src.str[2] == 'x') {
		hex = BS_TRUE;
		tok.src.str += 2;
		tok.src.len -= 2;
	}

	*out = 0;
	for (i = tok.src.len - 1; i >= 0; --i) {
		int digit;

		if (!bs_isdigit(tok.src.str[i])
			&& (!hex || bs_tolower(tok.src.str[i] < 'a')
				|| bs_tolower(tok.src.str[i] > 'f'))) {
			bs_emit_error(bs, "Malformed int literal");
			return -1;
		}
		if (*out < last) { /* detect overflow */
			bs_emit_error(bs, "Overflow detected in int literal");
			return -1;
		}

		if (bs_isdigit(tok.src.str[i])) {
			digit = tok.src.str[i] - '0';
		} else {
			digit = bs_tolower(tok.src.str[i]) - 'a' + 10;
		}

		last = *out;
		*out += digit * pow;
		pow *= hex ? 16 : 10;
	}

	return 0;
}
#ifndef BS_NOFP
static int bs_parse_float(struct bs *bs, bs_float *out)
{
	bs_float pow = 1.0;
	int i, dot;

	for (dot = 0; dot < bs->tok.src.len; ++dot) {
		if (bs->tok.src.str[dot] == '.') break;
	}

	*out = 0;
	for (i = dot - 1; i >= 0; --i) {
		if (!bs_isdigit(bs->tok.src.str[i])) {
			bs_emit_error(bs, "Malformed float literal");
			return -1;
		}
		*out += (bs_float)(bs->tok.src.str[i] - '0') * pow;
		pow *= 10.0;
	}
	pow = 0.1;
	for (i = dot + 1; i < bs->tok.src.len; ++i) {
		if (!bs_isdigit(bs->tok.src.str[i])) {
			bs_emit_error(bs, "Malformed float literal");
			return -1;
		}
		*out += (bs_float)(bs->tok.src.str[i] - '0') * pow;
		pow *= 0.1;
	}

	return 0;
}
#endif

static int bs_parse_number_lit(struct bs *bs, struct bs_var *out, bs_bool neg)
{
	bs_uint u;

#ifndef BS_NOFP
	int i;

	for (i = 0; i < bs->tok.src.len; i++) {
		if (bs->tok.src.str[i] == '.') {
			bs_float fp;

			if (bs_parse_float(bs, &fp)) return -1;
			out->type[0] = BS_FLOAT;
			return bs_compile_push_data(bs, out, &fp, sizeof(fp), sizeof(fp));
		}
	}
#endif

	if (bs_parse_uint(bs, &u)) return -1;
	if (neg || u <= BS_INT_MAX) {
		bs_int i = neg ? -(bs_int)u : u;
		out->type[0] = BS_INT;
		return bs_compile_push_data(bs, out, &i, sizeof(i), sizeof(i));
	} else {
		out->type[0] = BS_UINT;
		return bs_compile_push_data(bs, out, &u, sizeof(u), sizeof(u));
	}
}

static int bs_parse_number(struct bs *bs, enum bs_prec prec,
			struct bs_var *out, int depth)
{
	if (bs_parse_number_lit(bs, out, BS_FALSE)) return -1;
	if (bs_advance_token(bs, BS_TRUE)) return -1;
	return 0;
}

static int bs_parse_add(struct bs *bs, enum bs_prec prec,
			struct bs_var *out, int depth)
{
	struct bs_var right;
	bs_type_t common;
	struct bs_parser_backup backup;

	if (bs_advance_token(bs, BS_TRUE)) return -1;

	/* Get the right type, and get the common type */
	bs_parser_backup_init(bs, &backup);
	bs->mode = BS_MODE_OFF;
	if (bs_parse_expr(bs, BS_PREC_MUL, &right, depth + 1)) return -1;
	bs_parser_backup_restore(bs, &backup);
	if (bs_arithmetic_convert(bs, out->type, right.type, &common)) return -1;

	/* Cast left hand side first */
	if (bs_compile_cast(bs, out, &common)) return -1;

	/* Compile and cast right hand side */
	if (bs_parse_expr(bs, BS_PREC_MUL, &right, depth + 1)) return -1;
	if (bs_compile_cast(bs, &right, &common)) return -1;

	/* Add the two */
	if (bs_compile_math(bs, common, BS_TOKEN_PLUS)) return -1;

	return 0;
}

static int bs_parse_expr(struct bs *bs, enum bs_prec prec,
			struct bs_var *out, int depth)
{
	const struct bs_expr_rule *rule = bs_expr_rules + bs->tok.type;

	BS_DBG_ASSERT(out, "bs_parse_expr: out parameter is NULL");

	if (depth >= BS_RECURRSION_LIMIT) {
		bs_emit_error(bs, "Hit expression recurrsion limit");
		return -1;
	}

	if (!rule->prefix) {
		bs_emit_error(bs, "Expected expression");
		return -1;
	}
	if (rule->prefix(bs, rule->prec, out, depth)) return -1;

	rule = bs_expr_rules + bs->tok.type;
	while (rule->postfix && prec >= rule->prec) {
		if (rule->postfix(bs, rule->prec, out, depth)) return -1;
		rule = bs_expr_rules + bs->tok.type;
	}

	return 0;
}

#endif
#endif

