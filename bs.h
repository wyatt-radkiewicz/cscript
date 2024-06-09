/*
 * Based Script
 */
#ifndef _BS_H_
#define _BS_H_

#include <stddef.h>

/*
 * This callback is called when the based script compiler encounters and error
 * and needs to give the user feedback to show that the error occured.
 */
typedef void(*bs_error_writer_t)(void *user, int line,
				int col, const char *msg);

/*
 * You pass and array of this struct to based script to tell it about external
 * functions.
 */
struct bsvm;
struct bs_extern_fn {
	void (*pfn)(struct bsvm *vm);
	const char *name;
};

/*
 * When a based script script needs to allocate memory or free it, it calls
 * these functions. Also zalloc must allocate ZERO initialized memory.
 */
typedef void*(*bs_vmzalloc_t)(void *user, size_t size);
typedef void(*bs_vmfree_t)(void *user, void *blk);

#define BS_IMPL
#define BS_DEBUG

#ifdef BS_IMPL
/******************************************************************************
 * Utilities
 *****************************************************************************/

#ifndef BS_32BIT
# define BS_32BIT 0
#endif

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
# define BS_STRLEN __builtin_strlen
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
BS_INLINE int BS_STRLEN(const char *s)
{
	int len = 0;
	while (*s) {
		++len;
		++s;
	}
	return len;
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

/******************************************************************************
 * Based Script Types
 *****************************************************************************/

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
#if BS_32BIT
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
# if !BS_32BIT
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

#if BS_32BIT
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
typedef uint64_t bs_u64;
typedef uint64_t bs_i64;
# elif defined(__GNUC__)
typedef signed long bs_int;
typedef unsigned long bs_uint;
typedef unsigned long bs_u64;
typedef unsigned long bs_i64;
# elif defined(_WIN32) || defined (_WIN64)
#  include <Windows.h>
typedef LONG64 bs_int;
typedef DWORD64 bs_uint;
typedef DWORD64 bs_u64;
typedef DWORD64 bs_i64;
# else
#  error "Can't find 64-bit type for 64-bit mode in Based Script header file!"
# endif
#endif

#if __STDC_VERSION__ >= 199901L
# include <stdint.h>
typedef int8_t		bs_i8;
typedef int16_t		bs_i16;
typedef uint16_t	bs_u16;
typedef int32_t		bs_i32;
typedef uint32_t	bs_u32;
#else
typedef char		bs_i8;
typedef signed short	bs_i16;
typedef unsigned short	bs_u16;
typedef signed int	bs_i32;
typedef unsigned int	bs_u32;
#endif

#ifndef BS_NOFP
typedef float		bs_f32;
# if !BS_32BIT
typedef double		bs_f64;
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

	int			parentenum;
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
	struct bs_strview	*param_names;
	int			nparams;
	bs_type_t		*ret;
};

/*
 * A function implementation contains a signature and a pointer to its place in
 * the code segment.
 */
struct bs_fnimpl {
	/* This may be zero if the function is not templated */
	bs_type_t		**types;
	int			loc;
};

/*
 * A Based Script function definition. Can be templated or external.
 */
struct bs_fn {
	bs_bool			ext;
	struct bs_fnsig		sig;

	union {
		struct {
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
	BS_LOC_COMPTIME /* Comptime code space */
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
	BS_MODE_COMPTIME,
	BS_MODE_TRY_COMPTIME
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

	struct bs_fnimpl	fnimpls[256];
	int			nfnimpls;

	bs_error_writer_t	error_writer;
	void			*error_writer_user;
	int			errors;
	struct bs_extern_fn	*extern_fns;
	size_t			nextern_fns;
};

#define BS_CTLOC(BS) ((BS)->codeend - (BS)->ctstack)

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

/******************************************************************************
 * Based Script parsing and lexing utilities
 *****************************************************************************/

/*
 * Emits an error with the error writer function, incs error count
 */
static void bs_emit_error(struct bs *bs, const char *msg) {
	const char *curr;
	int line, col;

	if (!bs->error_writer) return;

	line = 1;
	col = 1;
	curr = bs->srcall.str;
	while (curr < bs->src.str) {
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

/*
 * This is used to backup state after getting an expression's type
 */
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

/******************************************************************************
 * Based Script Virtual Machine (VM)
 *****************************************************************************/
enum bs_opcode {
	BS_OPEXIT,
	
	/* Math ops */
	BS_OPADD,
	BS_OPADDQ,
	BS_OPSUB,
	BS_OPSUBQ,
	BS_OPMULI,
	BS_OPDIVI,
	BS_OPMULU,
	BS_OPDIVU,
	BS_OPMODI,
	BS_OPMODU,
	BS_OPNEG,
	BS_OPSLL,
	BS_OPSRL,
	BS_OPSRA,
	BS_OPAND,
	BS_OPOR,
	BS_OPXOR,
	BS_OPNOT,

#ifndef BS_NOFP
	BS_OPADDF,
	BS_OPSUBF,
	BS_OPMULF,
	BS_OPDIVF,
	BS_OPNEGF,
	BS_OPF2I,
	BS_OPF2U,
	BS_OPU2F,
	BS_OPI2F,
#endif

	/* Branching/Calling ops */
	BS_OPBEQ,
	BS_OPBNE,
	BS_OPBGT,
	BS_OPBLT,
	BS_OPBGE,
	BS_OPBLE,
	BS_OPJMP,
	BS_OPCALL,
	BS_OPRET,

#ifndef BS_NOFP
	BS_OPBGTF,
	BS_OPBLTF,
	BS_OPBGEF,
	BS_OPBLEF,
#endif

	/* Stack operations */
	BS_OPPUSH,
	BS_OPPUSHQ,
	BS_OPPOP,
	BS_OPPOPQ,
	BS_OPCONST1,
	BS_OPCONST2,
	BS_OPCONST2Q,
	BS_OPCONST4,
	BS_OPCONST4Q,
	BS_OPDUP1,
	BS_OPDUP2,
	BS_OPDUP4,
#if !BS_32BIT
	BS_OPCONST8,
	BS_OPCONST8Q,
	BS_OPDUP8,
#endif
	BS_OPALLOC,
	BS_OPDECRC,

	/* Load/Store ops */
	BS_OPLOADSP,
	BS_OPLOADPC,
	BS_OPLOADCODE,

	BS_OPLOADBUF,
	BS_OPSTOREBUF,
	BS_OPLOADI8,
	BS_OPLOADI8SQ,
	BS_OPLOADI16,
	BS_OPLOADI16SQ,
	BS_OPLOADI32,
	BS_OPLOADI32SQ,
	BS_OPLOADU8,
	BS_OPLOADU8SQ,
	BS_OPLOADU16,
	BS_OPLOADU16SQ,
	BS_OPLOADU32,
	BS_OPLOADU32SQ,
	BS_OPSTOREB8,
	BS_OPSTOREB8SQ,
	BS_OPSTOREB16,
	BS_OPSTOREB16SQ,
	BS_OPSTOREB32,
	BS_OPSTOREB32SQ,

#if !BS_32BIT
	BS_OPLOADB64,
	BS_OPLOADB64SQ,
	BS_OPSTOREB64,
	BS_OPSTOREB64SQ,
#endif

#ifndef BS_NOFP
	BS_OPLOADF32,
	BS_OPLOADF32SQ,
	BS_OPSTOREF32,
	BS_OPSTOREF32SQ
#endif
};

struct bs_ptrhdr {
	bs_uint		refs;
	bs_uint		len;
};

struct bs_pfn {
	bs_bool		ext;
	unsigned	loc;
};

struct bs_slice {
	void		*ptr;
	size_t		len;
};

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
	if (out) BS_MEMCPY(out, *ptr, len);
	*ptr += tonext;
	return 0;
}

#ifndef BS_VMSTACK_SIZE
# define BS_VMSTACK_SIZE 1024*8
#endif
#ifndef BS_VMCALLSTACK_SIZE
# define BS_VMCALLSTACK_SIZE 256
#endif

enum bs_vmerror {
	BS_VMERROR_OKAY,
	BS_VMERROR_ERROR,
	BS_VMERROR_OUTOFMEM,
	BS_VMERROR_STACKOVERFLOW,
	BS_VMERROR_CALLSTACKOVERFLOW,
	BS_VMERROR_NOEXTFUNC,
	BS_VMERROR_NULLREF
};

struct bsvm {
	const bs_byte		*pc;
	const bs_byte		*code;
	size_t			codelen;

	bs_byte			stack[BS_VMSTACK_SIZE];
	bs_byte			*sp;

	int			callstack[BS_VMCALLSTACK_SIZE];
	int			ret;

	bs_vmzalloc_t		alloc;
	bs_vmfree_t		free;
	void			*alloc_user;

	struct bs_extern_fn	*extfns;
	size_t			nextfns;
};

static void bsvm_init(struct bsvm *vm, const bs_byte *code, size_t codelen,
			bs_vmzalloc_t alloc, bs_vmfree_t free, void *user,
			struct bs_extern_fn *fns, size_t nfns)
{
	vm->code = code;
	vm->codelen = codelen;
	vm->pc = code;
	vm->sp = vm->stack + BS_VMSTACK_SIZE;
	vm->ret = 0;
	vm->alloc = alloc;
	vm->free = free;
	vm->alloc_user = user;
	vm->extfns = fns;
	vm->nextfns = nfns;
}

#define bs_vmpop(VM, DATA, SIZE, MAXALIGN)				\
	(bs_pop_data((VM)->stack + BS_ARRSIZE((VM)->stack),		\
			&(VM)->sp,					\
			(DATA), sizeof(*(DATA)),			\
			BS_ALIGNUU(sizeof(*(DATA)),			\
			((MAXALIGN) ? BS_MAX_ALIGN : sizeof(*(DATA)))))	\
		? BS_VMERROR_ERROR : BS_VMERROR_OKAY)
#define bs_vmpush(VM, DATA, SIZE, MAXALIGN)				\
	(bs_push_data((VM)->stack + BS_ARRSIZE((VM)->stack),		\
			&(VM)->sp,					\
			(DATA), sizeof(*(DATA)),			\
			BS_ALIGNUU(sizeof(*(DATA)),			\
			((MAXALIGN) ? BS_MAX_ALIGN : sizeof(*(DATA)))))	\
		? BS_VMERROR_STACKOVERFLOW : BS_VMERROR_OKAY)

static int bsvm_extract_b16(struct bsvm *vm, void *out)
{
	if (vm->pc + 2 > vm->code + vm->codelen) return BS_VMERROR_ERROR;
	*(bs_u16 *)out = *vm->pc++;
	*(bs_u16 *)out |= *vm->pc++ << 8;
	return BS_VMERROR_OKAY;
}

static int bsvm_extract_b32(struct bsvm *vm, void *out)
{
	if (vm->pc + 4 > vm->code + vm->codelen) return BS_VMERROR_ERROR;
	*(bs_u32 *)out = *vm->pc++;
	*(bs_u32 *)out |= *vm->pc++ << 8;
	*(bs_u32 *)out |= *vm->pc++ << 16;
	*(bs_u32 *)out |= *vm->pc++ << 24;
	return BS_VMERROR_OKAY;
}

#if !BS_32BIT
static int bsvm_extract_b64(struct bsvm *vm, void *out)
{
	if (vm->pc + 8 > vm->code + vm->codelen) return BS_VMERROR_ERROR;
	*(bs_u64 *)out =  (bs_u64)*vm->pc++;
	*(bs_u64 *)out |= (bs_u64)*vm->pc++ << 8;
	*(bs_u64 *)out |= (bs_u64)*vm->pc++ << 16;
	*(bs_u64 *)out |= (bs_u64)*vm->pc++ << 24;
	*(bs_u64 *)out |= (bs_u64)*vm->pc++ << 32;
	*(bs_u64 *)out |= (bs_u64)*vm->pc++ << 40;
	*(bs_u64 *)out |= (bs_u64)*vm->pc++ << 48;
	*(bs_u64 *)out |= (bs_u64)*vm->pc++ << 56;
	return BS_VMERROR_OKAY;
}
#endif

static int bsvm_runop(struct bsvm *vm, bs_bool *keep_running)
{
	bs_uint		ul, ur;
	bs_int		il, ir;
#ifndef BS_NOFP
	bs_float	fl, fr;
#endif
	struct bs_pfn	pfn;
	struct bs_slice	slice;
	union {
		bs_byte			*b;
		struct bs_ptrhdr	*ph;
	}		p;
	bs_byte		b;
	unsigned	u;
	int		i;

	bs_u16		bu16;
	bs_u32 		bu32;
	bs_i16 		bi16;
	bs_i32 		bi32;
#if !BS_32BIT
	bs_u64		bu64;
	bs_i64		bi64;
#endif

	switch ((enum bs_opcode)*vm->pc++) {
	case BS_OPEXIT:
		*keep_running = BS_FALSE;
		break;
	/* Math ops */
#define BINARYOP(OP, NAME, PREFIX)					\
	case BS_OP##NAME:						\
		if (bs_vmpop(vm, &PREFIX##r, sizeof(PREFIX##r), BS_TRUE)) return BS_VMERROR_ERROR;\
		if (bs_vmpop(vm, &PREFIX##l, sizeof(PREFIX##l), BS_TRUE)) return BS_VMERROR_ERROR;\
		PREFIX##l OP PREFIX##r;					\
		if (bs_vmpush(vm, &PREFIX##l, sizeof(PREFIX##l), BS_TRUE)) return BS_VMERROR_STACKOVERFLOW;\
		break;
	BINARYOP(+=, ADD, u)
	BINARYOP(-=, SUB, u)
	BINARYOP(*=, MULI, i)
	BINARYOP(*=, MULU, u)
	BINARYOP(/=, DIVI, i)
	BINARYOP(/=, DIVU, u)
	BINARYOP(%=, MODI, i)
	BINARYOP(%=, MODU, u)
	BINARYOP(<<=, SLL, u)
	BINARYOP(>>=, SRL, u)
	BINARYOP(>>=, SRA, i)
	BINARYOP(&=, AND, u)
	BINARYOP(|=, OR, u)
	BINARYOP(^=, XOR, u)
	case BS_OPADDQ:
		if (vm->pc >= vm->code + vm->codelen) return BS_VMERROR_ERROR;
		*(bs_uint *)(vm->sp) += *vm->pc++;
		break;
	case BS_OPSUBQ:
		if (vm->pc >= vm->code + vm->codelen) return BS_VMERROR_ERROR;
		*(bs_uint *)(vm->sp) -= *vm->pc++;
		break;
	case BS_OPNEG:
		*(bs_int *)(vm->sp) = -(*(bs_int *)(vm->sp));
		break;
	case BS_OPNOT:
		*(bs_uint *)(vm->sp) = ~(*(bs_uint *)(vm->sp));
		break;

#ifndef BS_NOFP
	BINARYOP(+=, ADDF, f)
	BINARYOP(-=, SUBF, f)
	BINARYOP(*=, MULF, f)
	BINARYOP(/=, DIVF, f)
	case BS_OPNEGF:
		*(bs_float *)(vm->sp) = -(*(bs_float *)(vm->sp));
		break;
	case BS_OPF2I:
		*(bs_int *)(vm->sp) = *(bs_float *)(vm->sp);
		break;
	case BS_OPF2U:
		*(bs_uint *)(vm->sp) = *(bs_float *)(vm->sp);
		break;
	case BS_OPU2F:
		*(bs_float *)(vm->sp) = *(bs_uint *)(vm->sp);
		break;
	case BS_OPI2F:
		*(bs_float *)(vm->sp) = *(bs_int *)(vm->sp);
		break;
#endif

	/* Branching/Calling ops */
#define BRANCHOP(COND, NAME, VAR)					\
	case BS_OP##NAME:						\
		if (bsvm_extract_b32(vm, &u)) return BS_VMERROR_ERROR;	\
		if (bs_vmpop(vm, &(VAR), sizeof(VAR), BS_TRUE)) return BS_VMERROR_ERROR;\
		if (u >= vm->codelen) return BS_VMERROR_ERROR;		\
		if (COND) vm->pc = vm->code + u;			\
		break;

	BRANCHOP(!ul, BEQ, ul)
	BRANCHOP(ul, BNE, ul)
	BRANCHOP(il > 0, BGT, il)
	BRANCHOP(il < 0, BLT, il)
	BRANCHOP(il >= 0, BGE, il)
	BRANCHOP(il <= 0, BLE, il)
	case BS_OPJMP:
		if (bsvm_extract_b32(vm, &u)) return BS_VMERROR_ERROR;
		if (u >= vm->codelen) return BS_VMERROR_ERROR;
		vm->pc = vm->code + u;
		break;
	case BS_OPCALL:
		if (bs_vmpop(vm, &pfn, sizeof(pfn), BS_TRUE)) return BS_VMERROR_ERROR;
		if (pfn.ext) {
			if (pfn.loc >= vm->nextfns || !vm->extfns
				|| !vm->extfns[pfn.loc].pfn)
				return BS_VMERROR_NOEXTFUNC;
			vm->extfns[pfn.loc].pfn(vm);
		} else {
			if (vm->ret >= BS_VMCALLSTACK_SIZE)
				return BS_VMERROR_CALLSTACKOVERFLOW;
			vm->callstack[vm->ret++] = vm->pc - vm->code;
			vm->pc = vm->pc + pfn.loc;
		}
		break;
	case BS_OPRET:
		if (!vm->ret) {
			*keep_running = BS_FALSE;
			break;
		}
		vm->pc = vm->code + vm->callstack[--vm->ret];
		break;

#ifndef BS_NOFP
	BRANCHOP(fl > 0.0, BGTF, fl)
	BRANCHOP(fl < 0.0, BLTF, fl)
	BRANCHOP(fl >= 0.0, BGEF, fl)
	BRANCHOP(fl <= 0.0, BLEF, fl)
#endif

	/* Stack operations */
	case BS_OPPUSH:
		if (bsvm_extract_b32(vm, &u)) return BS_VMERROR_ERROR;
		if (vm->sp - u < vm->stack) return BS_VMERROR_STACKOVERFLOW;
		vm->sp -= u;
		break;
	case BS_OPPUSHQ:
		if (vm->pc >= vm->code + vm->codelen) return BS_VMERROR_ERROR;
		u = *vm->pc++;
		if (vm->sp - u < vm->stack) return BS_VMERROR_STACKOVERFLOW;
		vm->sp -= u;
		break;
	case BS_OPPOP:
		if (bsvm_extract_b32(vm, &u)) return BS_VMERROR_ERROR;
		if (vm->sp + u >= vm->stack + BS_VMSTACK_SIZE)
			return BS_VMERROR_STACKOVERFLOW;
		vm->sp += u;
		break;
	case BS_OPPOPQ:
		if (vm->pc >= vm->code + vm->codelen) return BS_VMERROR_ERROR;
		u = *vm->pc++;
		if (vm->sp + u >= vm->stack + BS_VMSTACK_SIZE)
			return BS_VMERROR_STACKOVERFLOW;
		vm->sp += u;
		break;
	case BS_OPCONST1:
		if (vm->pc >= vm->code + vm->codelen) return BS_VMERROR_ERROR;
		b = *vm->pc++;
		if (bs_vmpush(vm, &b, sizeof(b), BS_FALSE)) return BS_VMERROR_STACKOVERFLOW;
		break;
	case BS_OPCONST2:
		if (bsvm_extract_b16(vm, &bu16)) return BS_VMERROR_ERROR;
		if (bs_vmpush(vm, &bu16, sizeof(bu16), BS_FALSE)) return BS_VMERROR_STACKOVERFLOW;
		break;
	case BS_OPCONST2Q:
		if (vm->pc >= vm->code + vm->codelen) return BS_VMERROR_ERROR;
		bi16 = *(bs_i8 *)vm->pc++;
		if (bs_vmpush(vm, &bi16, sizeof(bi16), BS_FALSE)) return BS_VMERROR_STACKOVERFLOW;
		break;
	case BS_OPCONST4:
		if (bsvm_extract_b32(vm, &bu32)) return BS_VMERROR_ERROR;
		if (bs_vmpush(vm, &bu32, sizeof(bu32), BS_FALSE)) return BS_VMERROR_STACKOVERFLOW;
		break;
	case BS_OPCONST4Q:
		if (vm->pc >= vm->code + vm->codelen) return BS_VMERROR_ERROR;
		bi32 = *(bs_i8 *)vm->pc++;
		if (bs_vmpush(vm, &bi32, sizeof(bi32), BS_FALSE)) return BS_VMERROR_STACKOVERFLOW;
		break;
	case BS_OPDUP1:
		b = *vm->sp;
		if (bs_vmpush(vm, &b, sizeof(b), BS_FALSE)) return BS_VMERROR_STACKOVERFLOW;
		break;
	case BS_OPDUP2:
		bu16 = *(bs_u16 *)vm->sp;
		if (bs_vmpush(vm, &bu16, sizeof(bu16), BS_FALSE)) return BS_VMERROR_STACKOVERFLOW;
		break;
	case BS_OPDUP4:
		bu32 = *(bs_u32 *)vm->sp;
		if (bs_vmpush(vm, &bu32, sizeof(bu32), BS_FALSE)) return BS_VMERROR_STACKOVERFLOW;
		break;
#if !BS_32BIT
	case BS_OPCONST8:
		if (bsvm_extract_b64(vm, &bu64)) return BS_VMERROR_ERROR;
		if (bs_vmpush(vm, &bu64, sizeof(bu64), BS_FALSE)) return BS_VMERROR_STACKOVERFLOW;
		break;
	case BS_OPCONST8Q:
		if (vm->pc >= vm->code + vm->codelen) return BS_VMERROR_ERROR;
		bi64 = *vm->pc++;
		if (bs_vmpush(vm, &bi64, sizeof(bi64), BS_FALSE)) return BS_VMERROR_STACKOVERFLOW;
		break;
	case BS_OPDUP8:
		bu64 = *(bs_u64 *)vm->sp;
		if (bs_vmpush(vm, &bu64, sizeof(bu64), BS_FALSE)) return BS_VMERROR_STACKOVERFLOW;
		break;
#endif
	case BS_OPALLOC:
		if (bs_vmpop(vm, &ur, sizeof(ur), BS_TRUE)) return BS_VMERROR_ERROR;
		if (bs_vmpop(vm, &ul, sizeof(ul), BS_TRUE)) return BS_VMERROR_ERROR;
		p.ph = vm->alloc ? vm->alloc(vm->alloc_user, ur * ul + sizeof(struct bs_ptrhdr)) : NULL;
		if (!p.ph) return BS_VMERROR_OUTOFMEM;
		p.ph->refs = 1;
		p.ph->len = ur;
		++p.ph;
		if (bs_vmpush(vm, &p, sizeof(p), BS_FALSE)) return BS_VMERROR_STACKOVERFLOW;
		break;
	case BS_OPDECRC:
		if (vm->sp + sizeof(void *) > vm->stack + BS_VMSTACK_SIZE) return BS_VMERROR_ERROR;
		p.b = *(bs_byte **)vm->sp;
		p.ph = (struct bs_ptrhdr *)p.b - 1;
		if (--p.ph->refs != 0) break;
		*(bs_byte **)vm->sp = NULL;
		if (!vm->free) break;
		vm->free(vm->alloc_user, p.ph);
		break;

	/* Load/Store ops */
	case BS_OPLOADSP:
		if (bs_vmpush(vm, &vm->sp, sizeof(vm->sp), BS_TRUE)) return BS_VMERROR_STACKOVERFLOW;
		break;
	case BS_OPLOADPC:
		if (bs_vmpush(vm, &vm->pc, sizeof(vm->pc), BS_TRUE)) return BS_VMERROR_STACKOVERFLOW;
		break;
	case BS_OPLOADCODE:
		if (bs_vmpush(vm, &vm->code, sizeof(vm->code), BS_TRUE)) return BS_VMERROR_STACKOVERFLOW;
		break;

	case BS_OPLOADBUF:
		if (bsvm_extract_b32(vm, &u)) return BS_VMERROR_ERROR;
		if (vm->sp - u < vm->stack) return BS_VMERROR_STACKOVERFLOW;
		vm->sp -= u;
		if (bs_vmpop(vm, &p, sizeof(p), BS_TRUE)) return BS_VMERROR_ERROR;
		if (!p.b) return BS_VMERROR_NULLREF;
		BS_MEMCPY(vm->sp, p.b, u);
		break;
	case BS_OPSTOREBUF:
		if (bsvm_extract_b32(vm, &u)) return BS_VMERROR_ERROR;
		if (vm->sp + u >= vm->stack + BS_VMSTACK_SIZE) return BS_VMERROR_STACKOVERFLOW;
		if (bs_vmpop(vm, &p, sizeof(p), BS_TRUE)) return BS_VMERROR_ERROR;
		if (!p.b) return BS_VMERROR_NULLREF;
		BS_MEMCPY(p.b, vm->sp, u);
		break;
#define LOADOP(NAME, TY, VAR)						\
	case BS_OPLOAD##NAME:						\
		if (bs_vmpop(vm, &p, sizeof(p), BS_TRUE)) return BS_VMERROR_ERROR;\
		VAR = *(TY *)p.b;					\
		if (bs_vmpush(vm, &(VAR), sizeof(VAR), BS_TRUE)) return BS_VMERROR_STACKOVERFLOW;\
		break;
#define LOADSQOP(NAME, TY, VAR)						\
	case BS_OPLOAD##NAME##SQ:					\
		if (vm->pc >= vm->code + vm->codelen) return BS_VMERROR_ERROR;\
		b = *vm->pc++;						\
		if (vm->sp + b + sizeof(VAR) > vm->code + vm->codelen) return BS_VMERROR_ERROR;\
		VAR = *(TY *)(vm->sp + b);				\
		if (bs_vmpush(vm, &(VAR), sizeof(VAR), BS_TRUE)) return BS_VMERROR_STACKOVERFLOW;\
		break;
	LOADOP(I8, bs_i8, il)
	LOADSQOP(I8, bs_i8, il)
	LOADOP(I16, bs_i16, il)
	LOADSQOP(I16, bs_i16, il)
	LOADOP(I32, bs_i32, il)
	LOADSQOP(I32, bs_i32, il)
	LOADOP(U8, bs_byte, ul)
	LOADSQOP(U8, bs_byte, ul)
	LOADOP(U16, bs_u16, ul)
	LOADSQOP(U16, bs_u16, ul)
	LOADOP(U32, bs_u32, ul)
	LOADSQOP(U32, bs_u32, ul)
#define STOREOP(NAME, TY, VAR)						\
	case BS_OPSTORE##NAME:						\
		if (bs_vmpop(vm, &p, sizeof(p), BS_TRUE)) return BS_VMERROR_ERROR;\
		if (bs_vmpop(vm, &(VAR), sizeof(VAR), BS_TRUE)) return BS_VMERROR_ERROR;\
		if (!p.b) return BS_VMERROR_NULLREF;			\
		*(TY *)p.b = VAR;				\
		break;
#define STORESQOP(NAME, TY, VAR)					\
	case BS_OPSTORE##NAME##SQ:					\
		if (bs_vmpop(vm, &(VAR), sizeof(VAR), BS_TRUE)) return BS_VMERROR_ERROR;\
		if (vm->pc >= vm->code + vm->codelen) return BS_VMERROR_ERROR;\
		b = *vm->pc++;						\
		if (vm->sp + b + sizeof(VAR) > vm->code + vm->codelen) return BS_VMERROR_ERROR;\
		*(TY *)(vm->sp + b) = VAR;				\
		break;
	STOREOP(B8, bs_byte, ul)
	STORESQOP(B8, bs_byte, ul)
	STOREOP(B16, bs_u16, ul)
	STORESQOP(B16, bs_u16, ul)
	STOREOP(B32, bs_u32, ul)
	STORESQOP(B32, bs_u32, ul)

#if !BS_32BIT
	LOADOP(B64, bs_u64, ul)
	LOADSQOP(B64, bs_u64, ul)
	STOREOP(B64, bs_u64, ul)
	STORESQOP(B64, bs_u64, ul)
#endif

#ifndef BS_NOFP
	LOADOP(F32, bs_f32, fl)
	LOADSQOP(F32, bs_f32, fl)
	STOREOP(F32, bs_f32, fl)
	STORESQOP(F32, bs_f32, fl)
#endif
	}

	return BS_VMERROR_OKAY;

#undef BINARYOP
#undef BRANCHOP
#undef LOADSQOP
#undef STORESQOP
#undef LOADOP
#undef STOREOP
}

static int bsvm_run(struct bsvm *vm, int loc)
{
	bs_bool running = BS_TRUE;

	if (loc < 0 || loc > vm->codelen) return BS_VMERROR_ERROR;
	vm->pc = vm->code + loc;

	while (running) {
		const int ret = bsvm_runop(vm, &running);
		if (ret) return ret;
	}
	
	return BS_VMERROR_OKAY;
}

/******************************************************************************
 * Based Script code emmiter
 *****************************************************************************/

/*
 * Writes a byte to the code segment, incrementing the code pointer in the
 * process. If there is no space left, the function returns NULL and
 * also throws a compiler error, otherwise it returns the old byte pointer
 * before the increment.
 */
BS_INLINE bs_byte *bs_emit_byte(struct bs *bs, bs_byte byte)
{
	bs_byte *ptr = bs->code;

	if (bs->code >= bs->ctstack) {
		bs_emit_error(bs, "Ran out of code space");
		return NULL;
	}

	*bs->code++ = byte;
	return ptr;
}
BS_INLINE bs_byte *bs_emit_len(struct bs *bs, const void *buf, size_t len)
{
	bs_byte *ptr = bs->code;

	if (bs->code + len > bs->ctstack) {
		bs_emit_error(bs, "Ran out of code space");
		return NULL;
	}

	BS_MEMCPY(bs->code, buf, len);
	bs->code += len;
	return ptr;
}

static bs_bool bs_is_arithmetic(struct bs *bs, const bs_type_t *type)
{
	switch (*type) {
	case BS_TYPE_CHAR: case BS_TYPE_BOOL: case BS_TYPE_I8: case BS_TYPE_U8:
	case BS_TYPE_I16: case BS_TYPE_U16: case BS_TYPE_I32: case BS_TYPE_U32:
#if !BS_32BIT
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
#if !BS_32BIT
	case BS_TYPE_I64:
#endif
		*out = BS_INT;
		return 0;
	case BS_TYPE_U8: case BS_TYPE_U16: case BS_TYPE_U32:
#if !BS_32BIT
	case BS_TYPE_U64:
#endif
		*out = BS_UINT;
		return 0;
#ifndef BS_NOFP
	case BS_TYPE_F32:
# if !BS_32BIT
	case BS_TYPE_F64:
# endif
		*out = BS_FLOAT;
		return 0;
#endif
	default:
		return -1;
	}
}

static int bs_compile_const_int(struct bs *bs, struct bs_var *out, bs_int i)
{
	out->type[0] = BS_INT;
	switch (bs->mode) {
	case BS_MODE_OFF:
		out->locty = BS_LOC_TYPEONLY;
		break;
	case BS_MODE_DEFAULT:
		out->locty = BS_LOC_SPREL;
		if (i <= 127 || i >= -128) {
			if (!bs_emit_byte(bs, BS_32BIT ? BS_OPCONST4Q : BS_OPCONST8Q)) return -1;
			if (!bs_emit_byte(bs, (char)i)) return -1;
		} else {
			if (!bs_emit_byte(bs, BS_32BIT ? BS_OPCONST4 : BS_OPCONST8)) return -1;
			if (!bs_emit_len(bs, &i, sizeof(i))) return -1;
		}
		bs->stack -= BS_ALIGNUU(sizeof(i), BS_MAX_ALIGN);
		out->loc = bs->stack;
		break;
	case BS_MODE_COMPTIME:
	case BS_MODE_TRY_COMPTIME:
		out->locty = BS_LOC_COMPTIME;
		if (bs_push_data(bs->codebegin, &bs->ctstack, &i, sizeof(i), BS_MAX_ALIGN)) {
			bs_emit_error(bs, "Ran out of comptime stack space!");
			return -1;
		}
		out->loc = BS_CTLOC(bs);
		break;
	}
	return 0;
}

static int bs_compile_const_uint(struct bs *bs, struct bs_var *out, bs_uint u)
{
	out->type[0] = BS_UINT;
	switch (bs->mode) {
	case BS_MODE_OFF:
		out->locty = BS_LOC_TYPEONLY;
		break;
	case BS_MODE_DEFAULT:
		out->locty = BS_LOC_SPREL;
		if (u < 128) {
			if (!bs_emit_byte(bs, BS_32BIT ? BS_OPCONST4Q : BS_OPCONST8Q)) return -1;
			if (!bs_emit_byte(bs, (bs_byte)u)) return -1;
		} else {
			if (!bs_emit_byte(bs, BS_32BIT ? BS_OPCONST4 : BS_OPCONST8)) return -1;
			if (!bs_emit_len(bs, &u, sizeof(u))) return -1;
		}
		bs->stack -= BS_ALIGNUU(sizeof(u), BS_MAX_ALIGN);
		out->loc = bs->stack;
		break;
	case BS_MODE_COMPTIME:
	case BS_MODE_TRY_COMPTIME:
		out->locty = BS_LOC_COMPTIME;
		if (bs_push_data(bs->codebegin, &bs->ctstack, &u, sizeof(u), BS_MAX_ALIGN)) {
			bs_emit_error(bs, "Ran out of comptime stack space!");
			return -1;
		}
		out->loc = BS_CTLOC(bs);
		break;
	}
	return 0;
}

#ifndef BS_NOFP
static int bs_compile_const_float(struct bs *bs, struct bs_var *out, bs_float f)
{
	out->type[0] = BS_FLOAT;
	switch (bs->mode) {
	case BS_MODE_OFF:
		out->locty = BS_LOC_TYPEONLY;
		break;
	case BS_MODE_DEFAULT:
		out->locty = BS_LOC_SPREL;
		if (!bs_emit_byte(bs, BS_32BIT ? BS_OPCONST4 : BS_OPCONST8)) return -1;
		if (!bs_emit_len(bs, &f, sizeof(f))) return -1;
		bs->stack -= BS_ALIGNUU(sizeof(f), BS_MAX_ALIGN);
		out->loc = bs->stack;
		break;
	case BS_MODE_COMPTIME:
	case BS_MODE_TRY_COMPTIME:
		out->locty = BS_LOC_COMPTIME;
		if (bs_push_data(bs->codebegin, &bs->ctstack, &f, sizeof(f), BS_MAX_ALIGN)) {
			bs_emit_error(bs, "Ran out of comptime stack space!");
			return -1;
		}
		out->loc = BS_CTLOC(bs);
		break;
	}
	return 0;
}
#endif

/*
 * Since objects may be aligned oddly on the stack, tonext specifies by how
 * much to change the next pointer
 */
static int bs_compile_pop_data(struct bs *bs, size_t amount)
{
	bs_u32 buf = amount;

	switch (bs->mode) {
	case BS_MODE_OFF:
		break;
	case BS_MODE_DEFAULT:
		if (amount < 0x100) {
			if (bs_emit_byte(bs, BS_OPPOPQ)) return -1;
			if (bs_emit_byte(bs, amount)) return -1;
		} else {
			if (bs_emit_byte(bs, BS_OPPOP)) return -1;
			if (bs_emit_len(bs, &buf, sizeof(buf))) return -1;
		}
		break;
	case BS_MODE_COMPTIME:
	case BS_MODE_TRY_COMPTIME:
		BS_DBG_ASSERT(!bs_pop_data(bs->codeend, &bs->ctstack, NULL, 0, amount), "ctstack undeflow");
		break;
	}
	return 0;
}

static int bs_compile_cast_comptime(struct bs *bs, struct bs_var *var,
				const bs_type_t *newtype)
{
#ifndef BS_NOFP
	bs_float f;
	union {
		bs_uint u;
		bs_int i;
	} ints;

	if (*newtype == BS_FLOAT && var->type[0] != BS_FLOAT) {
		if (bs_pop_data(bs->code + bs->codelen, &bs->ctstack,
				&ints, sizeof(ints), BS_MAX_ALIGN))
			return -1;
		if (var->type[0] == BS_UINT) f = (bs_float)ints.u;
		else f = (bs_float)ints.i;
		return bs_push_data(bs->codebegin, &bs->ctstack,
				&f, sizeof(f), BS_MAX_ALIGN);
	} else if (*newtype != BS_FLOAT && var->type[0] == BS_FLOAT) {
		if (bs_pop_data(bs->codeend, &bs->ctstack,
				&f, sizeof(f), BS_MAX_ALIGN))
			return -1;
		if (*newtype == BS_UINT) ints.u = (bs_uint)f;
		else ints.i = (bs_int)f;
		return bs_push_data(bs->codebegin, &bs->ctstack,
				&ints, sizeof(ints), BS_MAX_ALIGN);
	}
#endif
	return 0;
}

static int bs_compile_cast_default(struct bs *bs, struct bs_var *var,
				const bs_type_t *newtype)
{
#ifndef BS_NOFP
	if (*newtype == BS_FLOAT && var->type[0] != BS_FLOAT) {
		if (var->type[0] == BS_UINT) {
			if (bs_emit_byte(bs, BS_OPU2F)) return -1;
		} else if (bs_emit_byte(bs, BS_OPI2F)) return -1;
	} else if (*newtype != BS_FLOAT && var->type[0] == BS_FLOAT) {
		if (*newtype == BS_UINT) {
			if (bs_emit_byte(bs, BS_OPF2U)) return -1;
		} else if (bs_emit_byte(bs, BS_OPF2I)) return -1;
	}
#endif
	return 0;
}

static int bs_compile_cast(struct bs *bs, struct bs_var *var,
			const bs_type_t *newtype)
{
	if (!bs_is_arithmetic(bs, var->type)
		|| !bs_is_arithmetic(bs, newtype)) {
		bs_emit_error(bs, "Expected arithmetic types in cast!");
		return -1;
	}

	if (bs->mode == BS_MODE_OFF) return 0;
	if ((bs->mode == BS_MODE_COMPTIME
		|| bs->mode == BS_MODE_TRY_COMPTIME)
		&& bs_compile_cast_comptime(bs, var, newtype)) return -1;
	else if (bs->mode == BS_MODE_DEFAULT
		&& bs_compile_cast_default(bs, var, newtype)) return -1;
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

static int bs_compile_math_comptime(struct bs *bs, bs_type_t type, int mathop)
{
	union {
		bs_uint u;
		bs_int i;
		bs_float f;
	} l, r;
	size_t size;

	if (type == BS_UINT || type == BS_INT) size = sizeof(l.u);
#ifndef BS_NOFP
	else if (type == BS_FLOAT) size = sizeof(l.f);
#endif
	else bs_abort("expected int,uint, or fp in comptime expr");

	BS_DBG_ASSERT(!bs_pop_data(bs->codeend, &bs->ctstack, &r, size, size)
		&& !bs_pop_data(bs->codeend, &bs->ctstack, &l, size, size),
		"ctstack underflow!");

#ifndef BS_NOFP
# define OPF(NAME, OP)							\
	case NAME:							\
		if (type == BS_UINT) l.u OP r.u;			\
		else if (type == BS_INT) l.i OP r.i;			\
		else if (type == BS_FLOAT) l.f OP r.f;			\
		break;
#else
# define OPF(NAME, _OP) OP(NAME, _OP)
#endif

# define OP(NAME, OP)							\
	case NAME:							\
		if (type == BS_UINT) l.u OP r.u;			\
		else if (type == BS_INT) l.i OP r.i;			\
		break;

	switch (mathop) {
	OPF(BS_TOKEN_PLUS, +=)
	OPF(BS_TOKEN_MINUS, -=)
	OPF(BS_TOKEN_DIVIDE, /=)
	OPF(BS_TOKEN_STAR, *=)
	OP(BS_TOKEN_BITAND, &=)
	OP(BS_TOKEN_BITOR, |=)
	OP(BS_TOKEN_BITXOR, ^=)
	OP(BS_TOKEN_SHL, <<=)
	OP(BS_TOKEN_SHR, >>=)
	}

	if (bs_push_data(bs->codebegin, &bs->ctstack, &l, size, size)) {
		bs_emit_error(bs, "Comptime stack overflow!");
		return -1;
	}
	
	return 0;
#undef OP
#undef OPF
}

static int bs_compile_math_default(struct bs *bs, bs_type_t type, int mathop)
{
	switch (mathop) {
# define OP(NAME, MATHOP)						\
	case NAME:							\
		if (type == BS_FLOAT) {					\
			bs_emit_error(bs, "Expected int or float");	\
			return -1;					\
		}							\
		if (bs_emit_byte(bs, MATHOP)) return -1;		\
		break;

	case BS_TOKEN_PLUS:
		if (type == BS_FLOAT) {
			if (bs_emit_byte(bs, BS_OPADDF)) return -1;
		} else if (bs_emit_byte(bs, BS_OPADD)) return -1;
		break;
	case BS_TOKEN_MINUS:
		if (type == BS_FLOAT) {
			if (bs_emit_byte(bs, BS_OPSUBF)) return -1;
		} else if (bs_emit_byte(bs, BS_OPSUB)) return -1;
		break;
	case BS_TOKEN_DIVIDE:
		if (type == BS_FLOAT) {
			if (bs_emit_byte(bs, BS_OPDIVF)) return -1;
		} else if (type == BS_UINT) {
			if (bs_emit_byte(bs, BS_OPDIVU)) return -1;
		} else if (bs_emit_byte(bs, BS_OPDIVI)) return -1;
		break;
	case BS_TOKEN_STAR:
		if (type == BS_FLOAT) {
			if (bs_emit_byte(bs, BS_OPMULF)) return -1;
		} else if (type == BS_UINT) {
			if (bs_emit_byte(bs, BS_OPMULU)) return -1;
		} else if (bs_emit_byte(bs, BS_OPMULI)) return -1;
		break;
	OP(BS_TOKEN_BITAND, BS_OPAND)
	OP(BS_TOKEN_BITOR, BS_OPOR)
	OP(BS_TOKEN_BITXOR, BS_OPXOR)
	case BS_TOKEN_SHL:
		if (type == BS_FLOAT) {
			bs_emit_error(bs, "Expected int or uint");
			return -1;
		}
		if (bs_emit_byte(bs, BS_OPSLL)) return -1;
		break;
	case BS_TOKEN_SHR:
		if (type == BS_FLOAT) {
			bs_emit_error(bs, "Expected int or uint");
			return -1;
		}
		if (type == BS_UINT) {
			if (bs_emit_byte(bs, BS_OPSRL)) return -1;
		} else {
			if (bs_emit_byte(bs, BS_OPSRA)) return -1;
		}
		break;
	}

	return 0;
#undef OP
}

static int bs_compile_math(struct bs *bs, bs_type_t type, int mathop)
{
	switch (bs->mode) {
	case BS_MODE_OFF: break;
	case BS_MODE_DEFAULT:
		return bs_compile_math_default(bs, type, mathop);
	case BS_MODE_COMPTIME:
	case BS_MODE_TRY_COMPTIME:
		return bs_compile_math_comptime(bs, type, mathop);
	}

	return 0;
}

/*
 * Forward definitions for based script expression parsing engine
 * This is needed by the type parser to get array sizes
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

static int bs_parse_expr(struct bs *bs, enum bs_prec prec,
			struct bs_var *out, int depth);

/******************************************************************************
 * Based Script type parser engine
 *****************************************************************************/
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
			enum bs_mode mode = bs->mode;
			struct bs_var arrlen_var;
			bs_uint arrlen, tonext = BS_CTLOC(bs);

			if (bs_advance_token(bs, BS_TRUE)) return 1;
			
			bs->mode = BS_MODE_COMPTIME;
			if (bs_parse_expr(bs, BS_PREC_FULL, &arrlen_var, depth + 1)) return -1;
			bs->mode = mode;

			tonext = BS_CTLOC(bs) - tonext;
			if (arrlen_var.type[0] == BS_INT) {
				bs_int i;
				BS_DBG_ASSERT(!bs_pop_data(bs->codeend,
						&bs->ctstack, &i,
						sizeof(i), tonext),
						"ctstack underflow");
				if (i < 1) {
					bs_emit_error(bs, "Negative array lengths not allowed");
					return -1;
				}
				arrlen = i;
			} else if (arrlen_var.type[0] == BS_UINT) {
				BS_DBG_ASSERT(!bs_pop_data(bs->codeend,
						&bs->ctstack, &arrlen,
						sizeof(arrlen), tonext),
						"ctstack underflow");
			} else {
				bs_emit_error(bs, "Non-integer array length specified!");
				return -1;
			}

			if (bs->tok.type != BS_TOKEN_RBRACK) {
				bs_emit_error(bs, "Expected ] after array size!");
				return -1;
			}

			type[*len] |= BS_TYPE_ARR;
			qualifiers_seen = 0;
			if (*len + 2 > max) {
				bs_emit_error(bs, "Ran out of storage buffer space");
				return -1;
			}
			type[++(*len)] = arrlen;
			type[++(*len)] = 0;
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
#if !BS_32BIT
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
		case BS_TYPE_TEMPLATE:
			return len + 2;
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
# if !BS_32BIT
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
		if (bs_advance_token(bs, BS_FALSE)) return -1;
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

		if (*len + tmp > max) goto storage_error;
		/* Keep previous modifier? */
		type[(*len)++] |= *tmpl++ & ~BS_TYPE_FLAG_MUT;
		--tmp;
		BS_MEMCPY(type + *len, tmpl, sizeof(*tmpl) * tmp);
		*len += tmp;

		if (bs_advance_token(bs, BS_FALSE)) return -1;
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

/******************************************************************************
 * Based Script USER type compiler engine
 *****************************************************************************/
static int bs_parse_template_params(struct bs *bs,
				struct bs_strview **templates,
				int *ntemplates,
				bs_type_t (*tmpl_types)[2],
				int maxtemplates)
{
	*templates = NULL;
	*ntemplates = 0;
	if (bs->tok.type == BS_TOKEN_LT) {
		*templates = bs->strbuf + bs->strbuflen;
		if (bs_advance_token(bs, BS_FALSE)) return -1;
		while (bs->tok.type != BS_TOKEN_GT) {
			if (bs->strbuflen == BS_ARRSIZE(bs->strbuf)) {
				bs_emit_error(bs, "Hit string buffer max");
				return -1;
			}
			if (*ntemplates == maxtemplates) {
				bs_emit_error(bs, "Hit max templates");
				return -1;
			}
			if (bs->ntemplates == BS_ARRSIZE(bs->templates)) {
				bs_emit_error(bs, "Hit max active templates");
				return -1;
			}
			bs->templates[bs->ntemplates].nameid = bs->strbuflen;
			tmpl_types[*ntemplates][0] = BS_TYPE_TEMPLATE;
			tmpl_types[*ntemplates][1] = bs->strbuflen;
			bs->templates[bs->ntemplates++].impl =
				tmpl_types[(*ntemplates)++];
			bs->strbuf[bs->strbuflen++] = bs->tok.src;
			if (bs_advance_token(bs, BS_FALSE)) return -1;
			if (bs->tok.type == BS_TOKEN_COMMA
				&& bs_advance_token(bs, BS_FALSE)) return -1;
		}
		if (bs_advance_token(bs, BS_FALSE)) return -1;
	}

	return 0;
}

static int bs_alloc_def(struct bs *bs, struct bs_userdef **def)
{
	if (bs->ndefs == BS_ARRSIZE(bs->defs)) {
		bs_emit_error(bs, "Hit max number of user definables");
		return -1;
	}
	*def = bs->defs + bs->ndefs++;
	return 0;
}

static int bs_parse_typedef_type(struct bs *bs, struct bs_userdef *def,
			struct bs_strview *templates, int ntemplates)
{
	int len = 0;
	def->def.t.type = bs->typebuf + bs->typebuflen;
	if (bs_parse_type(bs, def->def.t.type, &len,
			BS_ARRSIZE(bs->typebuf) - bs->typebuflen, 0))
		return -1;
	bs->typebuflen += len;
	def->type = BS_USERDEF_TYPEDEF;
	def->def.t.templates = templates;
	def->def.t.ntemplates = ntemplates;

	return 0;
}

static int bs_parse_struct_members(struct bs *bs, struct bs_userdef *def,
			struct bs_strview *templates, int ntemplates,
			bs_bool optional)
{
	bs_type_t tmpl_types[16][2];
	struct bs_strview backup;

	def->type = BS_USERDEF_STRUCT;
	def->def.s.templates = templates;
	def->def.s.ntemplates = ntemplates;
	def->def.s.members = NULL;
	def->def.s.member_names = NULL;
	def->def.s.nmembers = 0;
	def->def.s.parentenum = -1;

	++bs->indent;
	backup = bs->src;
	if (bs_parse_indent(bs) != bs->indent) {
		if (!optional) bs_emit_error(bs, "Ident level incorrect after struct declaration");
		bs->src = backup;
		--bs->indent;
		return optional ? 0 : -1;
	}

	def->def.s.members = bs->types + bs->ntypes;
	def->def.s.member_names = bs->strbuf + bs->strbuflen;

	do {
		int len = 0;

		if (bs_advance_token(bs, BS_FALSE)) return -1;
		if (bs->tok.type != BS_TOKEN_IDENT) {
			bs_emit_error(bs, "Expected struct field name");
			goto error;
		}
		if (bs->strbuflen == BS_ARRSIZE(bs->strbuf)) {
			bs_emit_error(bs, "String buffer max hit");
			goto error;
		}
		if (bs->ntypes == BS_ARRSIZE(bs->types)) {
			bs_emit_error(bs, "Type pointer buffer max hit");
			goto error;
		}

		bs->strbuf[bs->strbuflen++] = bs->tok.src;
		bs->types[bs->ntypes] = bs->typebuf + bs->typebuflen;
		if (bs_advance_token(bs, BS_FALSE)) return -1;
		if (bs_parse_type(bs, bs->types[bs->ntypes++], &len,
				BS_ARRSIZE(bs->typebuf) - bs->typebuflen, 0))
			goto error;
		bs->typebuflen += len;
		++def->def.s.nmembers;
		backup = bs->src;
	} while (bs_parse_indent(bs) == bs->indent);
	bs->src = backup;

	--bs->indent;
	return 0;

error:
	--bs->indent;
	return -1;
}

static int bs_parse_enumdef(struct bs *bs)
{
	bs_type_t tmpl_types[16][2];
	struct bs_strview *templates, backup;
	struct bs_userdef *def;
	int ntemplates;

	const int tmplbase = bs->ntemplates;

	if (bs_alloc_def(bs, &def)) return -1;
	def->name = bs->tok.src;
	def->type = BS_USERDEF_ENUMSTART;
	def->def.e.templates = templates;
	def->def.e.ntemplates = ntemplates;
	def->def.e.nvariants = 0;
	if (bs_advance_token(bs, BS_FALSE)) return -1;
	if (bs_parse_template_params(bs, &templates, &ntemplates, tmpl_types,
				BS_ARRSIZE(tmpl_types)))
		return -1;
	if (bs->tok.type != BS_TOKEN_NEWLINE) {
		bs_emit_error(bs, "Expected newline and enum variants after enum declaration");
		return -1;
	}

	++bs->indent;
	if (bs_parse_indent(bs) != bs->indent) {
		bs_emit_error(bs, "Indent level incorrect after enum declaration");
		return -1;
	}
	do {
		struct bs_userdef *variant;

		if (bs_alloc_def(bs, &variant)) return -1;
		if (bs_advance_token(bs, BS_FALSE)) return -1;
		variant->name = bs->tok.src;
		if (bs_advance_token(bs, BS_FALSE)) return -1;
		if (bs->tok.type != BS_TOKEN_NEWLINE) {
			bs_emit_error(bs, "Expected newline after variant declaration.");
			return -1;
		}
		if (bs_parse_struct_members(bs, variant, templates, ntemplates, BS_TRUE))
			return -1;
		variant->def.s.parentenum = def - bs->defs;
		++def->def.e.nvariants;
		backup = bs->src;
	} while (bs_parse_indent(bs) == bs->indent);
	bs->src = backup;
	--bs->indent;

	return 0;
}

static int bs_parse_typedef(struct bs *bs)
{
	static const struct bs_strview _type_str = { "type", 4 };
	static const struct bs_strview _enum_str = { "enum", 4 };

	bs_type_t tmpl_types[16][2];
	struct bs_strview *templates;
	struct bs_userdef *def;
	int ntemplates;

	const int tmplbase = bs->ntemplates;

	BS_DBG_ASSERT(bs->tok.type == BS_TOKEN_IDENT, "expected type/enum");
	if (bs_strview_eq(bs->tok.src, _enum_str)) {
		if (bs_advance_token(bs, BS_FALSE)) return -1;
		return bs_parse_enumdef(bs);
	}
	BS_DBG_ASSERT(bs_strview_eq(bs->tok.src, _type_str), "expected 'type'");

	/* Parse the name and template params */
	if (bs_advance_token(bs, BS_FALSE)) return -1;
	if (bs_alloc_def(bs, &def)) return -1;
	def->name = bs->tok.src;
	if (bs_advance_token(bs, BS_FALSE)) return -1;
	if (bs_parse_template_params(bs, &templates, &ntemplates, tmpl_types,
				BS_ARRSIZE(tmpl_types)))
		return -1;
	
	if (bs->tok.type != BS_TOKEN_NEWLINE) { /* Do a typedef */
		if (bs_parse_typedef_type(bs, def, templates, ntemplates)) return -1;
	} else { /* Else, do a struct */
		if (bs_parse_struct_members(bs, def, templates, ntemplates, BS_FALSE)) return -1;
	}

	bs->ntemplates = tmplbase;
	return 0;
}

/******************************************************************************
 * Based Script function compiler
 *****************************************************************************/
static int bs_parse_func_sig(struct bs *bs, struct bs_fnsig *sig, bs_bool named)
{
	int len;

	sig->nparams = 0;
	sig->params = NULL;
	sig->param_names = NULL;
	sig->ret = NULL;

	if (bs->tok.type != BS_TOKEN_LPAREN) {
		bs_emit_error(bs, "Expected parameter list starting with (");
		return -1;
	}
	if (bs_advance_token(bs, BS_TRUE)) return -1;
	if (bs->tok.type != BS_TOKEN_RPAREN) {
		if (named) sig->param_names = bs->strbuf + bs->strbuflen;
		sig->params = bs->types + bs->ntypes;
	}
	while (bs->tok.type != BS_TOKEN_RPAREN) {
		if (named) {
			if (bs->strbuflen >= BS_ARRSIZE(bs->strbuf)) {
				bs_emit_error(bs, "Ran out of string buffer space.");
				return -1;
			}
			if (bs->tok.type != BS_TOKEN_IDENT) {
				bs_emit_error(bs, "Expected parameter name");
				return -1;
			}
			sig->param_names[sig->nparams] = bs->tok.src;
			if (bs_advance_token(bs, BS_FALSE)) return -1;
			++bs->strbuflen;
		}

		if (bs->ntypes >= BS_ARRSIZE(bs->types)
			|| bs->typebuflen >= BS_ARRSIZE(bs->typebuf)) {
			bs_emit_error(bs, "Ran out of type space.");
			return -1;
		}

		len = 0;
		sig->params[sig->nparams] = bs->typebuf + bs->typebuflen;
		if (bs_parse_type(bs, sig->params[sig->nparams], &len,
				BS_ARRSIZE(bs->typebuf)-bs->typebuflen, 0))
			return -1;
		bs->typebuflen += len;
		++bs->ntypes;

		++sig->nparams;
		if (bs->tok.type == BS_TOKEN_COMMA
			&& bs_advance_token(bs, BS_TRUE)) return -1;
	}
	if (bs_advance_token(bs, BS_FALSE)) return -1;

	len = 0;
	sig->ret = bs->typebuf + bs->typebuflen;
	if (bs->typebuflen >= BS_ARRSIZE(bs->typebuf)) {
		bs_emit_error(bs, "Ran out of type space.");
		return -1;
	}
	if (bs_parse_type(bs, sig->ret, &len, BS_ARRSIZE(bs->typebuf) - bs->typebuflen, 0))
		return -1;

	return 0;
}

static int bs_parse_func(struct bs *bs)
{
	static const struct bs_strview _str_ext = { "ext", 3 };
	static const struct bs_strview _str_fn = { "fn", 2 };

	bs_type_t tmpl_types[16][2];
	struct bs_userdef *def;
	struct bs_fn *fn;
	const int template_base = bs->ntemplates;

	if (bs->ndefs >= BS_ARRSIZE(bs->defs)) {
		bs_emit_error(bs, "Definition buffer full, can't compile function");
		return -1;
	}
	def = bs->defs + bs->ndefs++;
	def->type = BS_USERDEF_FN;
	fn = &def->def.f;

	BS_DBG_ASSERT(bs->tok.type == BS_TOKEN_IDENT, "expected ext or fn");

	if (bs_strview_eq(bs->tok.src, _str_ext)) {
		fn->ext = BS_TRUE;
	} else if (bs_strview_eq(bs->tok.src, _str_fn)) {
		fn->ext = BS_FALSE;
	} else {
		bs_emit_error(bs, "Expected 'ext' or 'fn'");
		return -1;
	}

	if (bs_advance_token(bs, BS_FALSE)) return -1;
	if (bs->tok.type != BS_TOKEN_IDENT) {
		bs_emit_error(bs, "Expected function name!");
		return -1;
	}
	def->name = bs->tok.src;
	if (fn->ext) {
		int i;
		fn->impl.external.id = -1;
		for (i = 0; i < bs->nextern_fns; i++) {
			struct bs_strview extfn;
			extfn.str = bs->extern_fns[i].name;
			extfn.len = BS_STRLEN(bs->extern_fns[i].name);

			if (!bs_strview_eq(def->name, extfn))
				continue;
			fn->impl.external.id = i;
			break;
		}
	} else {
		fn->impl.internal.templates = NULL;
		fn->impl.internal.ntemplates = 0;
		fn->impl.internal.impls = NULL;
		fn->impl.internal.nimpls = 0;
	}

	if (bs_advance_token(bs, BS_FALSE)) return -1;
	if (bs->tok.type == BS_TOKEN_LT && !fn->ext) {
		if (bs_parse_template_params(bs,
					&fn->impl.internal.templates,
					&fn->impl.internal.ntemplates,
					tmpl_types,
					BS_ARRSIZE(tmpl_types))) return -1;
	} else if (bs->tok.type == BS_TOKEN_LT && fn->ext) {
		bs_emit_error(bs, "Can not have templated external functions!");
		return -1;
	}
	if (bs_parse_func_sig(bs, &fn->sig, BS_TRUE)) return -1;

	bs->ntemplates = template_base;
	return 0;
}

/******************************************************************************
 * Based Script expression compiler
 *****************************************************************************/
typedef int(*bs_parse_expr_func_t)(struct bs *bs, enum bs_prec prec,
				struct bs_var *out, int depth);

struct bs_expr_rule {
	bs_parse_expr_func_t	prefix;
	bs_parse_expr_func_t	postfix;
	enum bs_prec		prec;
};

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
			return bs_compile_const_float(bs, out, fp);
		}
	}
#endif

	if (bs_parse_uint(bs, &u)) return -1;
	if (neg || u <= BS_INT_MAX) {
		bs_int i = neg ? -(bs_int)u : u;
		out->type[0] = BS_INT;
		return bs_compile_const_int(bs, out, i);
	} else {
		out->type[0] = BS_UINT;
		return bs_compile_const_uint(bs, out, u);
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

