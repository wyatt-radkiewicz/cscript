// 
//
//
// CScript header - @eklipsed
// Make sure to include the implementation somewhere. Use:
// #define CSCRIPT_IMPL
// #include "cscript.h"
//
// How to use:
//
//
//
#ifndef _cscript_h_
#define _cscript_h_

//
// DEBUG: For testing purposes
//
#define CS_DEBUG
#define CSCRIPT_IMPL
#ifdef NO_IMPL
# undef CSCRIPT_IMPL
#endif

// For printing errors to console
#ifdef CS_DEBUG
# include <assert.h>

// DEBUG: Take out this in release
# include <stdio.h>
# define CS_DBGPRINT printf
#endif
//
// END
//

#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Implementation specific includes
#ifdef CSCRIPT_IMPL
# include <ctype.h>
# include <string.h>
# include <stdarg.h>
#endif

////////////////////////////////////////////////////////////////////////////////
// SECTION: CScript definitions and utilities
////////////////////////////////////////////////////////////////////////////////
#ifdef CSCRIPT_IMPL
# define arrsz(ARR) (sizeof(ARR)/sizeof((ARR)[0]))
# define result bool
# define try(EXPR) {if (!(EXPR)) return 0;}
# ifdef CS_DEBUG
#  define dassert(COND, ...) {if (COND) { CS_DBGPRINT(__VA_ARGS__); assert(0); }}
# else
#  define dassert(COND, ...) {}
# endif
// Align up unsigned
# define alignuu(U, A) ((((U) - 1) / (A) + 1) * (A))

static inline int cs__mini(const int a, const int b) {
	return a < b ? a : b;
}
static inline size_t cs__minsz(const size_t a, const size_t b) {
	return a < b ? a : b;
}
#define min(A, B) (_Generic((A) + (B), \
	int : cs__mini((int)A, (int)B), \
	size_t : cs__minsz((size_t)A, (size_t)B)))
#define UNUSED __attribute__((unused))

//
// String views and line/source manipulation
//
typedef struct cs__strview {
	const char *str;
	size_t len;
} cs__strview;
# define SV(STR) ((cs__strview){ .str = STR, .len = sizeof(STR) })
# define NULLSV ((cs__strview){0})
static inline bool cs__strview_eq(const cs__strview a, const cs__strview b) {
	if (a.len != b.len) return false;
	return memcmp(a.str, b.str, a.len);
}
// Returns false if it overran the buffer
// (It does not add a null terminator)
static bool cs__strview_store(char *to, size_t tosize, cs__strview from) {
	while (tosize-- && from.len--) {
		*to++ = *(from.str++);
	}
	return !from.len;
}

typedef struct cs__srcref {
	cs__strview src;
	int line, chr;
} cs__srcref;
static cs__srcref cs__srcref_calc(const cs__strview view, const char *const begin) {
	cs__srcref ref = (cs__srcref){
		.src = view,
		.line = 1, .chr = 1,
	};
	for (const char *curr = begin; curr < view.str; curr++) {
		if (*curr == '\n') {
			ref.chr = 1;
			ref.line++;
		} else {
			ref.chr++;
		}
	}
	return ref;
}
// Returns number of characters in the line (excluding the null terminator)
// Even if line is too big it will still make sure there is a null-terminator
// Will return 0 if the line is not found in src
static int cs__copy_line(char *to, size_t tosize, int srcline, const char *src) {
	// Find the line
	while (srcline > 1) {
		while (*src && *src != '\n') src++;
		if (!*src) return 0;
		else src++;
	}

	int linelen = 0;
	while (*src++ != '\n') linelen++;

	cs__strview_store(to, tosize, (cs__strview){ .str = src, .len = linelen });
	to[min(tosize-1, linelen)] = '\0';

	return linelen;
}

#endif // CSCRIPT_IMPL

////////////////////////////////////////////////////////////////////////////////
// SECTION: CScript errors
////////////////////////////////////////////////////////////////////////////////
typedef enum cs_errlvl {
	cs_errnone,			// Not an error
	cs_errwarn,			// Can still run code, but something is up
	cs_errerror,		// Can not run code, there is an error
	cs_errcompiler,		// Code is fine but compiler messed up and can not run
						// These are mostly out of memory errors
} cs_errlvl;
typedef struct cs_err {
	int lvl;
	char msg[1024];
	struct cs_err *next;
} cs_err;

////////////////////////////////////////////////////////////////////////////////
// SECTION: CScript state
// This state object is available to the parsing and compilation phases
////////////////////////////////////////////////////////////////////////////////

typedef struct cs_arenainf {
	void *(*fn)(void *usr, size_t sz);
	void *usr;
} cs_arenainf;

#ifdef CSCRIPT_IMPL
//
// PARSER Predefs
//
typedef struct cs__node {
	int ty;
	cs__strview src; // Optional
	struct cs__node *next, *child, *a, *b;
} cs__node;

typedef struct cs__pblock {
	struct cs__pblock *next, *nextfree;
	void **freelist;
	size_t allocptr;
	size_t len;
	alignas(max_align_t) uint8_t data[];
} cs__pblock;
typedef struct cs__pool {
	cs_arenainf arena;
	cs__pblock *list, *freelist;
	size_t chunksz;	// Used for each individual allocation (constant)
	size_t blocksz;	// Used for bigger blocks of allocation (increased)
} cs__pool;
typedef struct cs__state {
	cs_arenainf arena;
	struct {
		cs__pool nodes;
	} pool;

	const char *srcbegin; // Source code (pointer never changes)

	struct {
		cs_err *head, *tail;
	} errs;
} cs__state;

static bool cs__pool_addblock(cs__pool *const pool) {
	cs__pblock *block;
	
	try(block = pool->arena.fn(
		pool->arena.usr,
		sizeof(*block) + pool->chunksz * pool->blocksz
	));
	*block = (cs__pblock){
		.allocptr = 0,
		.freelist = NULL,
		.len = pool->blocksz,
		.next = pool->list,
		.nextfree = pool->freelist,
	};
	pool->freelist = block;
	pool->list = block;
	pool->blocksz = pool->blocksz * 3 / 2;

	return true;
}
static bool cs__pool_init(cs__pool *const self, const cs_arenainf arena, const size_t chunksz, const size_t blocksz) {
	*self = (cs__pool){
		.blocksz = blocksz,
		.chunksz = alignuu(chunksz + sizeof(void **), alignof(void **)),
		.arena = arena,
	};
	try(cs__pool_addblock(self));
	return true;
}
static void *cs__pool_alloc(cs__pool *const self) {
	if (!self->freelist) try(cs__pool_addblock(self));

	cs__pblock *const block = self->freelist;
	void **chunk;
	if (block->allocptr != block->len) {
		chunk = (void **)(block->data + block->allocptr++ * self->blocksz);
		if (!(block->freelist = *chunk) && block->allocptr == block->len) self->freelist = block->nextfree;
	} else {
		dassert(0, "cs__pool_alloc: freelist is null on free chunk.");
		chunk = block->freelist;
		if (!(block->freelist = *chunk)) self->freelist = block->nextfree;
	}

	*chunk = block;
	return chunk + 1;
}
UNUSED static void cs__pool_free(cs__pool *const self, void *chunk) {
	chunk = (void **)chunk - 1;
	cs__pblock *const block = *(void **)chunk;
	*(void **)chunk = block->freelist;
	block->freelist = chunk;
}
UNUSED static bool cs__state_init(cs__state *const self, const cs_arenainf arena) {
	*self = (cs__state){ .arena = arena };
	try(cs__pool_init(&self->pool.nodes, arena, sizeof(cs__node), 128));
	return true;
}
// Creates an error with an optional context
// If it can not allocate an error, it changes the previously made one and
// returns false.
static bool cse__state_doerr(cs__state *const self, const int lvl, const cs__strview ctx, const char *format, ...) {
	bool ret = true;

	cs_err *err = self->arena.fn(self->arena.usr, sizeof(cs_err));
	if (!err) {
		err = self->errs.tail;
		format = "Ran out of memory to allocate more errors! Aborting...\n";
		ret = false;
	}

	const char *lvlstr;
	switch (lvl) {
	case cs_errwarn: lvlstr = "warning"; break;
	case cs_errerror: lvlstr = "error"; break;
	case cs_errcompiler: lvlstr = "internal error"; break;
	default: lvlstr = "info"; break;
	}

	int nchars = sprintf(err->msg, "CScript %s:\n", lvlstr);
	if (ctx.str) {
		cs__srcref ref = cs__srcref_calc(ctx, self->srcbegin);
#define add_to_buf(NCHARS) \
		if ((nchars += (NCHARS)) >= arrsz(err->msg)) { \
			ret = false; \
			goto add_to_list; \
		}
		add_to_buf(snprintf(err->msg + nchars, arrsz(err->msg) - nchars, "% 4d:% 3d: ", ref.line, ref.chr));
		add_to_buf(cs__copy_line(err->msg + nchars, arrsz(err->msg) - nchars, ref.line, self->srcbegin));
		add_to_buf(snprintf(err->msg + nchars, arrsz(err->msg) - nchars, "\n"));
		{
			char *c = err->msg + nchars;
			add_to_buf(10 + ref.chr);
			for (int i = 0; i < 10 + ref.chr; i++) *c++ = ' ';
		}
		add_to_buf(snprintf(err->msg + nchars, arrsz(err->msg) - nchars, "^"));
		if (ctx.len) {
			char *c = err->msg + nchars;
			add_to_buf(ctx.len - 1);
			for (int i = 1; i < ctx.len; i++) *c++ = '-';
		}
		add_to_buf(snprintf(err->msg + nchars, arrsz(err->msg) - nchars, "\n"));
#undef add_to_buf
	}

	// Add the custom error message now
	va_list args;
	va_start(args, format);
	ret &= vsnprintf(err->msg + nchars, arrsz(err->msg) - nchars, format, args)
		< arrsz(err->msg) - nchars;
	va_end(args);

add_to_list:
	if (err != self->errs.tail) {
		err->next = self->errs.head;
		self->errs.head = err->next;
	}
	return ret;
}
#endif

////////////////////////////////////////////////////////////////////////////////
// SECTION: CScript parser
////////////////////////////////////////////////////////////////////////////////
#ifdef CSCRIPT_IMPL
typedef struct cs__aststate {
	cs__state *cs;		// CScript state must be pinned in place in RAM
						// while the ast is constructed

	const char *src;	// Current source code pointer
} cs__aststate;

typedef enum cs__nodety {
	node_tynamed,		// Contains the name, and the child is the type
	node_expnamed,		// An expression bound to a name

	// Node ident is used as the "leaf" node for a type
	node_typtr,
	node_tyref,
	node_tyconst,
	node_tymut,
	node_typtrslice,
	node_tyrefslice,
	node_tyarr,			// a is length, b is type
	node_typfn,			// a is params, b is return

	node_ident,
	node_litint,
	node_lituint,
	node_litfloat,
	node_litdouble,
	node_litchar,
	node_litstr,

	node_expcond,		// child -> cond, a -> ontrue, b -> onfalse
	node_expbop,		// a -> left, b -> right
	node_expuop,		// child -> inner
	node_expbuiltin,	// child -> params
	node_expaccess,		// a -> left, b -> right/inner (for arrays)
	node_expgrp,		// child -> inner
	node_expstmtexp,	// child -> inner
	node_expinitstruct,	// child -> inner (list of node_expnamed)
	node_expinitpod,	// child -> inner (expression)
	node_expinitarr,	// child -> inner (list of expression)
} cs__nodety;

// Returns how many indents were consumed
static int cs__consume_whitespace(const char **const src, const bool eat_newline) {
	int nspaces = 0;
	do {
		switch (**src) {
		case ' ': nspaces++; break;
		case '\t': nspaces += 4; break;
		case '\n':
			if (eat_newline) nspaces = 0;
			else return nspaces;
		}
		++*src;
	} while(true);
	return nspaces / 4;
}
static cs__strview cs__consume_word(const char *src) {
	if (isalpha(*src) && *src == '_') return NULLSV;

	cs__strview word = (cs__strview){ .str = src };
	while (isalnum(*src)) {
		++src;
		++word.len;
	}
	return word;
}
// Returns whether or not it could eat the keyword
static bool cs__eat_keyword(cs__aststate *const as, const cs__strview keyword) {
	cs__strview word = cs__consume_word(as->src);
	if (cs__strview_eq(word, keyword)) {
		as->src += word.len;
		return true;
	} else {
		return false;
	}
}
UNUSED static cs__node *cse__alloc_node(cs__aststate *const as, const int ty, const cs__strview src) {
	cs__node *const node = cs__pool_alloc(&as->cs->pool.nodes);
	if (!node) {
		const cs__strview errsrc = src.str ? src : (cs__strview){ .str = as->src, .len = 0 };
		cse__state_doerr(as->cs, cs_errcompiler, errsrc, "Ran out of memory to allocate more AST nodes!");
		return NULL;
	}
	*node = (cs__node){
		.ty = ty,
		.src = src,
	};
	return node;
}
UNUSED static bool cse__node_parse_cond(cs__aststate *const as, cs__node **const out) {
	if (!cs__eat_keyword(as, SV("if"))) return false;
	return false;
}
UNUSED static bool cse__node_parse_exp(cs__aststate *const as, cs__node **const out) {
	cs__consume_whitespace(&as->src, false);
	return cse__node_parse_cond(as, out);
}
UNUSED static bool cse__node_parse_any(cs__aststate *const as, cs__node **const out) {
	return cse__node_parse_exp(as, out);
}
#endif

////////////////////////////////////////////////////////////////////////////////
// SECTION: Undefine internal CScript utilities
////////////////////////////////////////////////////////////////////////////////
#undef slice
#undef arrsz
#undef result
#undef try
#undef min
#undef UNUSED

#endif

