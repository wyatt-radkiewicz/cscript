#ifndef _parser_h_
#define _parser_h_

#include "vm.h"
#include "lexer.h"

typedef enum prec {
	// Lowest prec (only literals and idents)
	prec_lowest,

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
	prec_full = prec_comma,
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
	ast_ternary,
	ast_binary,
	ast_unary,
	ast_comma,
	ast_eq,
	ast_literal,
	ast_type,
	ast_decldef,
	ast_enum_field,
	ast_typedef,
	ast_error,
	ast_init_list,
	ast_accessor,
	ast_ident,
	ast_init_designator,
	ast_func_call,
	ast_cast,
	ast_stmt,
} astty_t;

typedef enum declty {
	decl_pointer_to,
	decl_array_of,
	decl_void,
	decl_pod,
	decl_struct,
	decl_union,
	decl_typedef,
	decl_enum,
	decl_function,
} declty_t;

typedef enum stmtty {
	stmt_compound,
	stmt_expr,
	stmt_decldef,
	stmt_ifelse,
	stmt_switch,
	stmt_while,
	stmt_dowhile,
	stmt_for,
	stmt_break,
	stmt_cont,
	stmt_return,
	stmt_goto,
	stmt_null,
} stmtty_t;

typedef enum labelty {
	label_none,
	label_norm,
	label_case,
	label_default,
} labelty_t;

typedef enum cvrflags {
	cvr_none		= 0,
	cvr_static		= 1 << 0,
	cvr_const		= 1 << 1,
	cvr_restrict	= 1 << 2,
	cvr_volatile	= 1 << 3,
	cvr_extern		= 1 << 4,
} cvrflags_t;

typedef enum literalty {
	lit_pod,
	lit_str,
	lit_compound,
	lit_char,
} literalty_t;

#define AST_SENTINAL (-1)
typedef struct ast {
	astty_t ty;
	tok_t tok;
	union {
		struct {
			stmtty_t ty;
			labelty_t labelty;
			int label;
			union {
				int inner;
				struct {
					int cond, on_true, on_false;
				} ifelse;
				struct {
					int cond, inner;
				} cond;
				struct {
					int init_clause;
					int cond_expr, iter_expr;
					int stmt;
				} forinfo;
			};
		} stmt;
		struct { int left, right; } binary;
		struct { int cond, ontrue, onfalse; } ternary;
		struct {
			int expr, next;
		} comma;
		struct {
			int ret, params, body;
		} func;
		struct {
			int func, params;
		} func_call;
		struct {
			int type, def;
		} decldef;
		struct {
			int type, expr;
		} cast;
		struct {
			cvrflags_t cvrs;
			declty_t ty;
			// array of X, pointer to X, struct members, func return
			int inner;
			union {
				unitty_t pod;
				int funcparams;
				int arrlen;
				int tyname; // identifier of declaration type
			};
		} type;
		struct {
			int accessor, init;
		} designator;
		struct {
			int access_from, accessing;
		} accessor;
		int inner;
		struct {
			literalty_t ty;
			union {
				struct {
					int type, init;
				} compound;
				typed_unit_t pod;
				tok_t lit;
			} val;
		} literal;
	} info;
	int next, line, chr;
} ast_t;

struct parser {
	lexer_t lexer;
	ast_t *buf;
	size_t nnodes, buflen;
	err_t errs[32];
	size_t nerrs;
	int root;
	ident_map_t tymap;
	ident_ent_t tymap_ents[511];
};

int parser_add(parser_t *const state, ast_t newnode);
parser_t parse(const char *src, ast_t *astbuf, size_t buflen);
void dbg_ast_print(const ast_t *const ast, int root);

#endif

