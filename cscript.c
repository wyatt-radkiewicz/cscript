#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "cscript.h"

typedef uint8_t		u8;
typedef int8_t		i8;
typedef uint16_t	u16;
typedef int16_t		i16;
typedef uint32_t	u32;
typedef int32_t		i32;
typedef uint64_t	u64;
typedef int64_t		i64;
typedef float		f32;
typedef double		f64;
typedef size_t		usize;
typedef uintptr_t	uptr;
typedef intptr_t	iptr;

typedef struct {
	const char	*str;
	usize		len;
} sview_t;
static inline bool sview_eq(const sview_t a, const sview_t b) {
	if (a.len != b.len) return false;
	return memcmp(a.str, b.str, a.len) == 0;
}
#define SV(LIT) ((sview_t){ .str = LIT, .len = sizeof(LIT)-1 })

#define MAXSTACK	sizeof(csvm_ret_t)*1024*8
#define MAXRETBUF	512
enum opty {
	OPNEGI,
	OPADDI,
	OPSUBI,
	OPMULI,
	OPDIVI,
	OPMODI,
	OPMULU,
	OPDIVU,
	OPMODU,
	OPNEGF,
	OPADDF,
	OPSUBF,
	OPMULF,
	OPDIVF,
	OPMODF,

	OPJE,
	OPJNE,
	OPJGI,
	OPJGEI,
	OPJLI,
	OPJLEI,
	OPJGU,
	OPJGEU,
	OPJLU,
	OPJLEU,
	OPJGF,
	OPJGEF,
	OPJLF,
	OPJLEF,

	OPOR,
	OPXOR,
	OPAND,
	OPNOT,
	OPSLL,
	OPSRA,
	OPSRL,
	OPEXT,	// size

	OPALLOC,
	OPFREE,
	OPCALL,
	OPCALLEXT,
	OPRET,
	OPF2I,
	OPI2F,

	OPLOAD,	// size
	OPSTORE,// size

	OPIMMQ, // imm (0-3)
	OPIMMU,	// [get next 4/8 bytes]
};
typedef struct {
	u8	op : 6;
	u8	arg : 2;
} op_t;
typedef struct {
	u32	ext	: 1;		// Is external?
	u32	offs	: 31;		// Offset to ext pfn, or func itself
} pfn_t;
typedef struct {
	void	*ptr;
	usize	len;
} slice_t;
struct csvm {
	u8	*out;
	u8	*stack;
	int	*ret;

	u8	stackbuf[MAXSTACK];
	int	retbuf[MAXRETBUF];
};

#define MAXNESTING	16
#define MAXTEMPLATES	8
#define MAXTIMPLS	64
#define MAXSCOPE	128
#define MAXUSERTY	128
#define MAXFUNCS	64
#define MAXTYBUF	256
enum tyclass {
	TYVOID,
	TYBOOL,
	TYCHAR,
	TYI8,
	TYU8,
	TYI16,
	TYU16,
	TYI32,
	TYU32,
#ifdef CS_32BIT
	TYUSIZE,
#else
	TYI64,
	TYU64,
	TYUSIZE,
#endif
	TYF32,
	TYF64,

	TYSLICE,
	TYARR,
	TYPTR,
	TYARRPTR,

	TYTYPE,
	TYENUM,
	TYPFN,
	TYTMPL,
};
typedef struct {
	u8		lvl[MAXNESTING];
	u8		id[MAXNESTING];
} ty_t;
typedef struct {
	ty_t		ty;
	sview_t		name;
} named_t;
typedef struct {
	sview_t		tmpls[MAXTEMPLATES];
	usize		cached_size;	// Size after template substitution
	usize		cached_align;
	named_t		*cached_mbrs;
	named_t		*mbrs;		// Types with TYTMPL still there (don't change)
	int		nmbrs;
} struct_t;
typedef struct {
	sview_t		tmpls[MAXTEMPLATES];
	usize		cached_size;
	usize		cached_align;
	int		nvariants;	// Variants come right after this one here
} enum_t;
typedef struct {
	named_t		*params;
	int		nparams;
	ty_t		ret;
} fnsig_t;
typedef struct {
	ty_t		tmpls[MAXTEMPLATES];
	int		loc;
	ty_t		ret;
} fnimpl_t;
typedef struct {
	sview_t		name;
	sview_t		tmpls[MAXTEMPLATES];
	const char	*def;			// Pointer to source code where it is
	fnimpl_t	impls[MAXTIMPLS];	// Index 0 if not template func
} fn_t;
enum usertyclass {
	USERTY_STRUCT,
	USERTY_ENUM,
	USERTY_PFN,
	USERTY_TYPEDEF,
};
typedef struct {
	sview_t			name;
	int			ty;
	union {
		struct_t	s;
		enum_t		e;
		fnsig_t		p;
		ty_t		t;
	};
} userty_t;
enum locty {
	LOCERR,		// Error evaluating something
	LOCEND,

	LOCPC,		// PC relative
	LOCSP,		// Stack relative
	LOCIND,		// Add indirection ontop of that
};
typedef struct {
	u8	loc;	// locty
	i16	scope;	// -1(rvalue) 0+ (scope ID)
	int	offs;
	ty_t	ty;
} val_t;

// Main cscript state
typedef struct {
	sview_t		scopename[MAXSCOPE];
	val_t		scope[MAXSCOPE];
	int		nscopes;

	int		vstack;		// Virtual stack

	userty_t	usertypes[MAXUSERTY];
	int		nusertypes;
	fn_t		funcs[MAXFUNCS];
	int		nfuncs;

	named_t		typebuf[MAXTYBUF];
	int		ntypes;		// Length of typebuffer

	const char	*srcbegin;
	const char	*src;

	u8		*outbegin;
	usize		outlen;
	u8		*out;
	u8		*data;

	cs_error_writer_t	writer_cb;
	cs_func_callback_t	func_cb;
	cs_extfunc_callback_t	ext_cb;
} cs_t;

static inline bool isalpha_(char c) {
	return isalpha(c) || c == '_';
}
// Will advance src if token is the same
static inline bool expect_token(cs_t *cs, const char *token) {
	const usize len = strlen(token);
	if (strncmp(cs->src, token, len)) return false;
	cs->src += len;
	return true;
}
static sview_t parse_word(cs_t *cs) {
	if (!isalpha_(*cs->src)) return (sview_t){0};
	sview_t word = (sview_t){.str = cs->src, .len=0};
	while (isalpha_(*cs->src) || isdigit(*cs->src)) {
		cs->src++;
		word.len++;
	}
	return word;
}
static inline bool expect_word(cs_t *cs, sview_t word) {
	const char *backup = cs->src;
	if (sview_eq(parse_word(cs), word)) return true;
	cs->src = backup;
	return false;
}



