/*
 * Based Script
 */
#ifndef _BS_H_
#define _BS_H_

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
enum {
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

	/* Used only in specific circumstances */
	BS_TYPE_TEMPLATE,

	BS_TYPE_MAX,

	/* Mutable modifier mask */
	BS_TYPE_FLAG_MUT	= 0x8000

};

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

	bs_type_t		**members;
	int			nmembers;
};

/*
 * User definable enum
 */
struct bs_enum {
	struct bs_strview	*templates;
	int			ntemplates;

	struct bs_strview	*variant_names;
	struct bs_struct	**variant_fields;
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
	BS_USERDEF_ENUM,
	BS_USERDEF_TYPEDEF,
	BS_USERDEF_FN,
	BS_USERDEF_PFN
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
		struct bs_fnsig		p;
	} def;
};

/*
 * Location of a Based Script object in memory
 */
enum {
	BS_LOC_PCREL,	/* Relative to the interpreter program counter */
	BS_LOC_SPREL,	/* Relative to the stack */
	BS_LOC_CODEREL,	/* Relative to the start of the code segment */
	BS_LOC_ABS,	/* Absolute memory addess */

	/* Flag to indicate indirection of location */
	BS_LOC_FLAG_IND = 0x80	
};

/*
 * Contains type, name, and location in memory of a variable. It can also be an
 * rvalue or generally unnamed.
 */
struct bs_var {
	bs_byte			loc;
	bs_type_t		*type;
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
 * Main Based Script State
 */
struct bs {
	struct bs_token		tok;
	struct bs_strview	src, srcall;
	bs_byte			*code, *codebegin;
	size_t			codelen;

	/* Virtual stack and compile time stack */
	int			stack, ctstack;

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
static void bs_consume_whitespace(struct bs_strview *src)
{
	while ((src->str[0] == ' ' || src->str[0] == '\t') && src->len) {
		++src->str;
		--src->len;
	}
}

/*
 * Source pointer will point after the output token after this finishes.
 * Will output errors if it fails.
 */
static int bs_advance_token(struct bs *bs)
{
	char start;

	bs_consume_whitespace(&bs->src);

	bs->tok.type = BS_TOKEN_EOF;
	bs->tok.src.str = bs->src.str;
	bs->tok.src.len = 1;
	if (!bs->src.len) return 0;

	start = bs->src.str[0];
	++bs->src.str;
	--bs->src.len;

	switch (start) {
	case '\0': bs->tok.type = BS_TOKEN_EOF; return 1;
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
			|| bs_tolower(bs->src.str[0]) >= 'a'
			|| bs_tolower(bs->src.str[0]) <= 'f'
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

static int bs_parse_type(struct bs *bs, bs_type_t **type, int *len);

/*
 * Parse non sentinal types like pointers and qualifiers like const and mut
 */
static int bs_parse_type_levels(struct bs *bs, bs_type_t **type, int *len)
{
	/* String view constants */
	static const struct bs_strview _bs_strview_const = { "const", 5 };
	static const struct bs_strview _bs_strview_mut = { "mut", 3 };

	/* Get reset each time a pointer, etc is found so that there is no code
	 * that looks like "const mut &[] u8" */
	int qualifiers_seen = 0;

	if (bs_advance_token(bs)) return 1;
	**type = 0;

	while (BS_TRUE) {
		const struct bs_strview backup = bs->src;

		if (!*len) {
			bs_emit_error(bs, "bs_parse_type_levels: Ran out of type buffer storage!");
			return 1;
		}

		if (bs->tok.type == BS_TOKEN_IDENT
			&& bs_strview_eq(bs->tok.src, _bs_strview_const)) {
			++qualifiers_seen;
			**type &= ~BS_TYPE_FLAG_MUT;
		} else if (bs->tok.type == BS_TOKEN_IDENT
			&& bs_strview_eq(bs->tok.src, _bs_strview_mut)) {
			++qualifiers_seen;
			**type |= BS_TYPE_FLAG_MUT;
		} else if (bs->tok.type == BS_TOKEN_STAR) {
			if (bs_advance_token(bs)) return 1;
			if (bs->tok.type == BS_TOKEN_LBRACK) {
				if (bs_advance_token(bs)) return 1;
				if (bs->tok.type != BS_TOKEN_RBRACK) {
					bs_emit_error(bs, "Pointer to arrays shouldn't have length embedded into type.");
					return 1;
				}
				**type |= BS_TYPE_ARRPTR;
			} else {
				bs->src = backup;
				**type |= BS_TYPE_PTR;
			}
			qualifiers_seen = 0;
			*(++(*type)) = 0;
			--(*len);
		} else if (bs->tok.type == BS_TOKEN_BITAND) {
			if (bs_advance_token(bs)) return 1;
			if (bs->tok.type == BS_TOKEN_LBRACK) {
				if (bs_advance_token(bs)) return 1;
				if (bs->tok.type != BS_TOKEN_RBRACK) {
					bs_emit_error(bs, "Slices shouldn't have length embedded into type.");
					return 1;
				}
				**type |= BS_TYPE_SLICE;
			} else {
				bs->src = backup;
				**type |= BS_TYPE_REF;
			}
			qualifiers_seen = 0;
			*(++(*type)) = 0;
			--(*len);
		} else if (bs->tok.type == BS_TOKEN_LBRACK) {
			BS_DBG_ASSERT(BS_FALSE, "TODO: Implement array length in types");
		} else {
			return 0;
		}

		if (bs_advance_token(bs)) return 1;
		if (qualifiers_seen > 1) {
			bs_emit_error(bs, "Only one qualifier is expected when parsing type!");
			return 1;
		}
	}
}

static int bs_parse_type_integer(struct bs *bs, bs_type_t **out, int *len)
{
	/* String view constants */
	static const struct bs_strview _bs_strview_usize = { "usize", 5 };
	static const struct bs_strview _bs_strview_isize = { "isize", 5 };

	const bs_bool u = bs->tok.src.str[0] == 'u';

	if (!*len) {
		bs_emit_error(bs, "bs_parse_type_integer: Ran out of type buffer storage!");
		return 1;
	}

	if (bs->tok.src.len == 2 && bs->tok.src.str[1] == '8') {
		*((*out)++) |= u ? BS_TYPE_U8 : BS_TYPE_I8;
	} else if (bs->tok.src.len == 3) {
		if (bs->tok.src.str[1] == '1' && bs->tok.src.str[2] == '6') {
			*((*out)++) |= u ? BS_TYPE_U16 : BS_TYPE_I16;
		} else if (bs->tok.src.str[1] == '3' && bs->tok.src.str[2] == '2') {
			*((*out)++) |= u ? BS_TYPE_U32 : BS_TYPE_I32;
		}
#ifndef BS_32BIT
		else if (bs->tok.src.str[1] == '6' && bs->tok.src.str[2] == '4') {
			*((*out)++) |= u ? BS_TYPE_U64 : BS_TYPE_I64;
		}
#endif
		else {
			return -1;
		}
	} else if (bs_strview_eq(bs->tok.src, _bs_strview_usize)) {
		*((*out)++) |= BS_TYPE_USIZE;
	} else if (bs_strview_eq(bs->tok.src, _bs_strview_isize)) {
		*((*out)++) |= BS_TYPE_ISIZE;
	} else {
		return -1;
	}

	--(*len);
	if (bs_advance_token(bs)) return 1;
	return 0;
}

/*
 * Get how many type levels/slots a type takes up. If len is 0, then the type is
 * not a correct type and is corrupted.
 */
static int bs_type_len(const struct bs *bs, const bs_type_t *type, int rdepth)
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
	else if (def->type == BS_USERDEF_ENUM)
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
static int bs_parse_type_template_typedef(struct bs *bs,
				bs_type_t **type, int *len,
				const struct bs_userdef *def)
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
	i = 1;
	tmpls[0] = bs->src;
	depth = 1;
	while (depth) {
		if (bs_advance_token(bs)) return -1;
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
	if (bs_advance_token(bs)) return -1; /* Get past last > */

	/* Skip to where the template type list SHOULD be for the typedef
	 * while also copying the type over */
	iter = def->def.t.type;
	while (BS_TRUE) {
		switch (*iter & ~BS_TYPE_FLAG_MUT) {
		case BS_TYPE_ARR:
			if (!*len) goto storage_error;
			--*len;
			*(*type)++ = *iter++;
			/* Fall Through */
		case BS_TYPE_PTR: case BS_TYPE_REF:
		case BS_TYPE_ARRPTR: case BS_TYPE_SLICE:
			if (!*len) goto storage_error;
			--*len;
			*(*type)++ = *iter++;
			continue;
		default: break; /* Then continue to the break under here. */
		}
		break;
	}

	/* Now we use ntmpls to get the number of underlying templates */
	if (!*len) goto storage_error;
	--*len;
	tmp = *iter++;
	*(*type)++ = tmp;
	if ((tmp & ~BS_TYPE_FLAG_MUT) != BS_TYPE_USER) {
		return 0;
	} else {
		int id = *iter++;
		if (!*len) goto storage_error;
		--*len;
		*(*type)++ = id;
		switch (bs->defs[id].type) {
		case BS_USERDEF_ENUM:
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
			if (!*len) goto storage_error;
			--*len;
			*(*type)++ = *iter++;
		default:
			if (!*len) goto storage_error;
			--*len;
			*(*type)++ = *iter++;
			break;
		case BS_TYPE_TEMPLATE:
			++iter; /* Goto the template ID */
			tmp = def->def.t.templates - bs->strbuf;
			BS_DBG_ASSERT(*iter >= tmp, "Template ID out of bounds");
			BS_DBG_ASSERT(*iter < tmp + def->def.t.ntemplates,
				"Template ID out of bounds");
			bs->src = tmpls[*iter - tmp];
			if (bs_parse_type(bs, type, len)) return -1;
			++iter;
		}
	}
	
	bs->src = backup;
	return 0;
storage_error:
	bs_emit_error(bs, "bs_parse_type_typedef: Ran out of type buffer storage!");
	return 1;
}
static int bs_parse_type_typedef(struct bs *bs, bs_type_t **type, int *len,
				const struct bs_userdef *def)
{
	const bs_type_t *tdef;
	int tmplen;

	if (bs_advance_token(bs)) return -1;
	if (def->def.t.ntemplates != 0)
		return bs_parse_type_template_typedef(bs, type, len, def);

	if (!(tmplen = bs_type_len(bs, def->def.t.type, 0)))
		return 1;
	if (*len < tmplen) {
		bs_emit_error(bs, "bs_parse_type_typedef: Ran out of type buffer storage!");
		return 1;
	}
	tdef = def->def.t.type;

	/* Keep previous modifier? */
	*((*type)++) |= *tdef++ & ~BS_TYPE_FLAG_MUT;
	--(*len);
	--tmplen;
	BS_MEMCPY(*type, tdef, sizeof(*tdef) * tmplen);
	*type += tmplen;
	*len -= tmplen;

	return 0;
}


/*
 * Parse type and advance source pointer
 */
static int bs_parse_type(struct bs *bs, bs_type_t **type, int *len)
{
	/* String view constants */
	static const struct bs_strview _bs_strview_char = { "char", 4 };
	static const struct bs_strview _bs_strview_bool = { "bool", 4 };

	int i, ntemplates;

	if (!*len) goto storage_error;
	if (bs_parse_type_levels(bs, type, len)) return 1;
	if (bs->tok.type != BS_TOKEN_IDENT) {
		bs_emit_error(bs, "Expected identifier when parsing type");
		return 1;
	}

	/* Search for normal POD sentinal types */
	switch (bs->tok.src.str[0]) {
	case 'u': case 'i':
		if (!bs_parse_type_integer(bs, type, len)) return 0;
		break;
#ifndef BS_NOFP
	case 'f':
		if (bs->tok.src.len != 3) break;
		if (bs->tok.src.str[1] == '3' && bs->tok.src.str[2] == '2') {
			*((*type)++) |= BS_TYPE_F32;
			--(*len);
			return 0;
		}
# ifndef BS_32BIT
		else if (bs->tok.src.str[1] == '6'
			&& bs->tok.src.str[2] == '4') {
			*((*type)++) |= BS_TYPE_F64;
			--(*len);
			return 0;
		}
# endif
#endif
	case 'c':
		if (!bs_strview_eq(bs->tok.src, _bs_strview_char)) break;
		*((*type)++) |= BS_TYPE_CHAR;
		--(*len);
		return 0;
	case 'b':
		if (!bs_strview_eq(bs->tok.src, _bs_strview_bool)) break;
		*((*type)++) |= BS_TYPE_BOOL;
		--(*len);
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
			if (bs_parse_type_typedef(bs, type, len, bs->defs + i))
				return -1;
			return 0;
		}
		if (bs->defs[i].type == BS_USERDEF_ENUM)
			ntemplates = bs->defs[i].def.e.ntemplates;
		else if (bs->defs[i].type == BS_USERDEF_STRUCT)
			ntemplates = bs->defs[i].def.s.ntemplates;
		if (*len < 2) goto storage_error;
		*((*type)++) |= BS_TYPE_USER;
		BS_DBG_ASSERT(i <= 0xffff, "ID Passed max type ID!");
		*((*type)++) = i;
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

		if (*len < tmp) goto storage_error;
		/* Keep previous modifier? */
		*((*type)++) |= *tmpl++ & ~BS_TYPE_FLAG_MUT;
		--(*len);
		--tmp;
		BS_MEMCPY(*type, tmpl, sizeof(*tmpl) * tmp);
		*type += tmp;
		*len -= tmp;

		return 0;
	}

	bs_emit_error(bs, "Use of undeclared identifier when parsing type");
	return -1;

parse_template_params:
	/* Add additional template parameters */
	if (bs_advance_token(bs)) return -1;
	if (ntemplates == 0) return 0;
	if (bs->tok.type != BS_TOKEN_LT) {
		bs_emit_error(bs, "Expected template parameter list");
		return -1;
	}

	while (bs->tok.type != BS_TOKEN_GT) {
		if (bs_parse_type(bs, type, len)) return -1;
		if (bs->tok.type == BS_TOKEN_COMMA) {
			if (bs_advance_token(bs)) return -1;
		}
	}
	if (bs_advance_token(bs)) return -1;
	else return 0;

storage_error:
	bs_emit_error(bs, "bs_parse_type: Ran out of type buffer storage!");
	return 1;
}

#endif
#endif

