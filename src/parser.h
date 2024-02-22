#ifndef _parser_h_
#define _parser_h_

#include "vm.h"
#include "lexer.h"

typedef enum prec {
	// Left to Right
	prec_grouping,

	// Right to Left
	prec_2,

	// Left to right
	prec_multiplicative,
	prec_additive,
	prec_bitshifting,
	prec_relational,
	prec_eqeq,
	prec_bitand,
	prec_bitxor,
	prec_bitor,
	prec_and,
	prec_or,

	// Right to Left
	prec_ternary,
	prec_assign,

	// Left to Right
	prec_comma,
	prec_full,
	prec_null,
} prec_t;

typedef struct parser parser_t;
typedef int(parse_unary_fn)(parser_t *const state);
typedef int(parse_binary_fn)(
	parser_t *const state,
	const prec_t prec,
	int left
);
typedef parse_unary_fn *parse_unary_pfn;
typedef parse_binary_fn *parse_binary_pfn;

typedef struct parse_rule {
	parse_unary_pfn prefix;
	parse_binary_pfn infix;
	prec_t prec;
} parse_rule_t;

typedef enum astty {
	ast_binary,
	ast_unary,
	ast_literal,
	ast_ident,
	ast_typedef,
	ast_struct,
	ast_member,
	ast_enum,
	ast_union,
	ast_function,
	ast_global,
	ast_decl,
} astty_t;

typedef enum dataty {
	dataty_struct,
	dataty_enum,
	dataty_union,
	dataty_typedef,
	dataty_pod,
	dataty_void,
} dataty_t;

typedef enum cvrflags {
	cvr_none,
	cvr_const		= 1 << 0,
	cvr_volatile	= 1 << 1,
	cvr_restrict	= 1 << 2,
	cvr_static		= 1 << 3,
	cvr_extern		= 1 << 4,
} cvrflags_t;

typedef enum lvlty {
	lvlty_data, // includes typedefs which could be arrays or pointers
	lvlty_pointer, // directly pointer
	lvlty_array, // directly array
	lvlty_func, // directly function
} lvlty_t;

typedef struct lvldata {
	dataty_t ty;
	union {
		tok_t tyname; // name of struct/typedef/etc
		unitty_t pod;
	};
} lvldata_t;

typedef struct lvlfunc {
	int ret;
	int params[31];
	int nparams;
} lvlfunc_t;

typedef struct declvl {
	lvlty_t ty;
	cvrflags_t cvr;
	union {
		lvldata_t data; // only for data type
		lvlfunc_t func; // only defined for func
		size_t array_len; // 0 for [], only for array type
	}; // pointer needs no extra info, that will be in next layer down
	   // likewise with array
} declvl_t;

typedef struct decl {
	declvl_t lvls[32];
	int nlvls;
	tok_t ident;
} decl_t;

typedef union metadecl {
	decl_t root;
	decl_t decls[64]; // Params start at index 1
} metadecl_t;

#define AST_SENTINAL (-1)
typedef struct ast {
	astty_t ty;
	tok_t tok;
	union {
		struct { int left, right; } binary;
		struct {
			int cond;
			union {
				struct { int true_stmt, false_stmt; };
				struct { int stmt; };
			};
		} cond;
		struct {
			int ret, params, body;
		} func;
		metadecl_t decl;
		int struct_members;
		int enums;
		int inner;
		typed_unit_t val;
	} info;
	int next;
} ast_t;

struct parser {
	lexer_t lexer;
	ast_t *buf;
	size_t nnodes, buflen;
	err_t errs[32];
	size_t nerrs;
	int root;
};

int parser_add(parser_t *const state, ast_t newnode);
parser_t parse(const char *src, ast_t *astbuf, size_t buflen);
parse_unary_fn parse_number_lit;
parse_unary_fn parse_ident;
parse_unary_fn parse_unary;
parse_binary_fn parse_binary;
void parse_toplevels(parser_t *const state);
int parse_expression(parser_t *const state, const prec_t prec);
void dbg_ast_print(const ast_t *const ast, int root);

#endif

