#ifndef _parser_h_
#define _parser_h_

#include "mem.h"
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

typedef struct ast ast_t;
struct ast {
	astty_t ty;
	tok_t tok;
	union {
		struct {
			stmtty_t ty;
			labelty_t labelty;
			ast_t *label;
			union {
				ast_t *inner;
				struct {
					ast_t *cond, *on_true, *on_false;
				} ifelse;
				struct {
					ast_t *cond, *inner;
				} cond;
				struct {
					ast_t *init_clause;
					ast_t *cond_expr, *iter_expr;
					ast_t *stmt;
				} forinfo;
			};
		} stmt;
		struct { ast_t *left, *right; } binary;
		struct { ast_t *cond, *ontrue, *onfalse; } ternary;
		struct {
			ast_t *expr, *next;
		} comma;
		struct {
			ast_t *ret, *params, *body;
		} func;
		struct {
			ast_t *func, *params;
		} func_call;
		struct {
			ast_t *type, *def;
		} decldef;
		struct {
			ast_t *type, *expr;
		} cast;
		struct {
			cvrflags_t cvrs;
			declty_t ty;
			// array of X, pointer to X, struct members, func return
			ast_t *inner;
			union {
				unitty_t pod;
				ast_t *funcparams;
				ast_t *arrlen;
				ast_t *tyname; // identifier of declaration type
			};
		} type;
		struct {
			ast_t *accessor, *init;
		} designator;
		struct {
			ast_t *access_from, *accessing;
		} accessor;
		ast_t *inner;
		struct {
			literalty_t ty;
			union {
				struct {
					ast_t *type, *init;
				} compound;
				typed_unit_t pod;
				tok_t lit;
			} val;
		} literal;
	} info;
	ast_t *next;
	int line, chr;
};

struct parser_result {
	dynpool_t ast_pool;
	ast_t *root;
	err_t *errs;
	size_t nerrs;
} parse(const char *src);
void dbg_ast_print(const ast_t *node, int tablvl);

#endif

