#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "parser.h"

typedef struct parser parser_t;
typedef ast_t *(parse_unary_fn)(parser_t *const state);
typedef ast_t *(parse_binary_fn)(
	parser_t *const state,
	const prec_t prec,
	ast_t *left
);
typedef parse_unary_fn *parse_unary_pfn;
typedef parse_binary_fn *parse_binary_pfn;

typedef struct parse_rule {
	parse_unary_pfn prefix;
	parse_binary_pfn infix;
	prec_t prec;
} parse_rule_t;

#define MAX_ERRS 128

struct parser {
	lexer_t lexer;
	dynpool_t ast_pool;
	ast_t *root;

	err_t *errs;
	size_t nerrs;

	ident_map_t *tymap;
};

// Will parse the type and either return
// just the ast_type with type info
// or the ast_decl with type info and identifier
static ast_t *parse_statement(parser_t *const state);
static ast_t *parse_initializer(parser_t *const state, const bool allow_desginator);
static ast_t *parse_type(parser_t *const state, const bool multi_decl, const bool allow_idents);
static typed_unit_t parse_number(parser_t *const state);
static void parse_toplevels(parser_t *const state);
static ast_t *parse_expression(parser_t *const state, const prec_t prec);
static parse_unary_fn parse_access_prefix;
static parse_binary_fn parse_access_post;
static parse_unary_fn parse_ident;
static parse_unary_fn parse_number_lit;
static parse_unary_fn parse_string_lit;
static parse_unary_fn parse_unary;
static parse_binary_fn parse_binary;
static parse_binary_fn parse_call;
static parse_binary_fn parse_comma;
static parse_binary_fn parse_eq;

static const parse_rule_t _rules[] = {
	// Keywords
	[tok_auto] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_bool] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_break] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_case] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_char] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_const] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_continue] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_default] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_do] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_double] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_else] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_enum] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_extern] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_false] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_float] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_for] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_goto] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_if] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_int] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_long] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_nullptr] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_restrict] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_return] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_short] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_signed] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_static] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_static_assert] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_struct] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_switch] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_true] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_typedef] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_typeof] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_union] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_unsigned] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_void] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_volatile] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_while] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	
	// Tokens (first and second chars)
	[tok_eof] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_plus] = (parse_rule_t){ .prefix = parse_unary, .infix = parse_binary, .prec = prec_additive },
	[tok_plusplus] = (parse_rule_t){ .prefix = parse_unary, .infix = parse_binary, .prec = prec_2 },
	[tok_minus] = (parse_rule_t){ .prefix = parse_unary, .infix = parse_binary, .prec = prec_additive },
	[tok_minusminus] = (parse_rule_t){ .prefix = parse_unary, .infix = parse_binary, .prec = prec_2 },
	[tok_star] = (parse_rule_t){ .prefix = parse_access_prefix, .infix = parse_binary, .prec = prec_multiplicative },
	[tok_dot] = (parse_rule_t){ .prefix = NULL, .infix = parse_access_post, .prec = prec_grouping },
	[tok_arrow] = (parse_rule_t){ .prefix = NULL, .infix = parse_access_post, .prec = prec_grouping },
	[tok_eq] = (parse_rule_t){ .prefix = NULL, .infix = parse_eq, .prec = prec_assign },
	[tok_pluseq] = (parse_rule_t){ .prefix = NULL, .infix = parse_eq, .prec = prec_assign },
	[tok_minuseq] = (parse_rule_t){ .prefix = NULL, .infix = parse_eq, .prec = prec_assign },
	[tok_stareq] = (parse_rule_t){ .prefix = NULL, .infix = parse_eq, .prec = prec_assign },
	[tok_slasheq] = (parse_rule_t){ .prefix = NULL, .infix = parse_eq, .prec = prec_assign },
	[tok_bitandeq] = (parse_rule_t){ .prefix = NULL, .infix = parse_eq, .prec = prec_assign },
	[tok_bitoreq] = (parse_rule_t){ .prefix = NULL, .infix = parse_eq, .prec = prec_assign },
	[tok_xoreq] = (parse_rule_t){ .prefix = NULL, .infix = parse_eq, .prec = prec_assign },
	[tok_shreq] = (parse_rule_t){ .prefix = NULL, .infix = parse_eq, .prec = prec_assign },
	[tok_shleq] = (parse_rule_t){ .prefix = NULL, .infix = parse_eq, .prec = prec_assign },
	[tok_eqeq] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_eqeq },
	[tok_bitand] = (parse_rule_t){ .prefix = parse_access_prefix, .infix = parse_binary, .prec = prec_bitand },
	[tok_and] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_and },
	[tok_bitor] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_bitor },
	[tok_or] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_or },
	[tok_bitnot] = (parse_rule_t){ .prefix = parse_unary, .infix = NULL, .prec = prec_null },
	[tok_noteq] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_eqeq },
	[tok_not] = (parse_rule_t){ .prefix = parse_unary, .infix = NULL, .prec = prec_null },
	[tok_xor] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_bitxor },
	[tok_lparen] = (parse_rule_t){ .prefix = parse_unary, .infix = parse_call, .prec = prec_grouping },
	[tok_rparen] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_lbrack] = (parse_rule_t){ .prefix = parse_access_prefix, .infix = parse_access_post, .prec = prec_grouping },
	[tok_rbrack] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_lbrace] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_rbrace] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_gt] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_relational },
	[tok_ge] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_relational },
	[tok_shr] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_bitshifting },
	[tok_lt] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_relational },
	[tok_le] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_relational },
	[tok_shl] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_bitshifting },
	[tok_comma] = (parse_rule_t){ .prefix = NULL, .infix = parse_comma, .prec = prec_comma },
	[tok_semicol] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_slash] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_multiplicative },
	[tok_percent] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_multiplicative },

	// Misc tokens
	[tok_string] = (parse_rule_t){ .prefix = parse_string_lit, .infix = NULL, .prec = prec_null },
	[tok_char_lit] = (parse_rule_t){ .prefix = parse_string_lit, .infix = NULL, .prec = prec_null },
	[tok_integer] = (parse_rule_t){ .prefix = parse_number_lit, .infix = NULL, .prec = prec_null },
	[tok_floating] = (parse_rule_t){ .prefix = parse_number_lit, .infix = NULL, .prec = prec_null },
	[tok_ident] = (parse_rule_t){ .prefix = parse_ident, .infix = NULL, .prec = prec_null },
	[tok_error] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_undefined] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_sizeof] = (parse_rule_t){ .prefix = parse_unary, .infix = NULL, .prec = prec_2 },
	[tok_qmark] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_colon] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_doublehash] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_hash] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
};

static inline ast_t *parser_add(
	parser_t *const state,
	ast_t node
) {
	ast_t *ptr = dynpool_alloc(state->ast_pool);
	*ptr = node;
	return ptr;
}

void parser_err(
	parser_t *const state,
	err_t err
) {
	if (state->nerrs == MAX_ERRS - 1) {
		state->errs[state->nerrs++] = (err_t){ .ty = err_toomany, .msg = "Too many errors, stopping.", .line = err.line, .chr = err.chr };
	} else if (state->nerrs < MAX_ERRS) {
		state->errs[state->nerrs++] = err;
	}

	// TODO: Continue to next statement I guess, it depends
	while (state->lexer.curr.ty != tok_semicol
		&& state->lexer.curr.ty != tok_eof
		&& state->lexer.curr.ty != tok_error) {
		lexer_next(&state->lexer);
	}
	lexer_next(&state->lexer);
}

static bool parser_eat(parser_t *const state, const tokty_t tokty, const char *const expect_msg) {
	if (state->lexer.curr.ty == tokty) {
		lexer_next(&state->lexer);
		return true;
	} else {
		const err_t err = (err_t){ .ty = err_expected, .msg = expect_msg, .line = state->lexer.curr.line, .chr = state->lexer.curr.chr };
		parser_err(state, err);
		return false;
	}
}

static ast_t *_parse_cast(parser_t *const state, ast_t *const type) {
	parser_eat(state, tok_rparen, "\")\"");
	const int line = state->lexer.curr.line, chr = state->lexer.curr.chr;
	ast_t *init = state->lexer.curr.ty == tok_lbrace ? parse_initializer(state, false) : NULL;
	if (!init) {
		ast_t *const expr = parse_expression(state, prec_2);
		if (expr) {
			return parser_add(state, (ast_t){
				.ty = ast_cast,
				.tok = (tok_t){ .ty = tok_undefined },
				.info.cast = {
					.type = type,
					.expr = expr,
				},
				.line = line,
				.chr = chr,
			});
		} else {
			return type;
		}
	} else {
		return parser_add(state, (ast_t){
			.ty = ast_literal,
			.tok = (tok_t){ .ty = tok_undefined },
			.info.literal = {
				.ty = lit_compound,
				.val.compound = {
					.type = type,
					.init = init,
				},
			},
			.line = line,
			.chr = chr,
		});
	}
}

static ast_t *parse_unary(parser_t *const state) {
	const tok_t tok = state->lexer.curr;
	ast_t *val = NULL;
	lexer_next(&state->lexer);
	switch (tok.ty) {
	case tok_lparen:
		{
			ast_t *const type = parse_type(state, false, false);
			if (type) return _parse_cast(state, type);
			val = parse_expression(state, prec_assign);
			parser_eat(state, tok_rparen, "right paren");
			break;
		}
	default:
		val = parse_expression(state, prec_grouping);
		break;
	}
	return parser_add(state, (ast_t){
		.ty = ast_unary,
		.tok = tok,
		.info.inner = val,
		.line = tok.line,
		.chr = tok.chr,
	});
}

static ast_t *parse_comma(
	parser_t *const state,
	const prec_t prec,
	ast_t *left
) {
	const tok_t tok = state->lexer.curr;
	lexer_next(&state->lexer);
	return parser_add(state, (ast_t){
		.ty = ast_comma,
		.tok = tok,
		.info.comma.expr = left,
		.info.comma.next = parse_expression(state, prec_comma-1),
		.line = tok.line,
		.chr = tok.chr,
	});
}

static ast_t *parse_call(
	parser_t *const state,
	const prec_t prec,
	ast_t *left
) {
	const int line = state->lexer.curr.line, chr = state->lexer.curr.chr;
	lexer_next(&state->lexer);
	ast_t *params = NULL;
	ast_t **last = &params;
	while (state->lexer.curr.ty != tok_eof && state->lexer.curr.ty != tok_error) {
		ast_t *const param = parse_expression(state, prec_ternary);
		*last = param;
		last = &param->next;
		if (state->lexer.curr.ty == tok_comma) lexer_next(&state->lexer);
		else break;
	}
	parser_eat(state, tok_rparen, "\")\"");
	return parser_add(
		state,
		(ast_t){
			.ty = ast_func_call,
			.tok = (tok_t){ .ty = tok_undefined },
			.info.func_call.func = left,
			.info.func_call.params = params,
			.line = line,
			.chr = chr,
		}
	);
}

static ast_t *parse_eq(
	parser_t *const state,
	const prec_t prec,
	ast_t *left
) {
	const tok_t tok = state->lexer.curr;
	lexer_next(&state->lexer);
	ast_t *const right = parse_expression(state, prec);
	return parser_add(
		state,
		(ast_t){
			.ty = ast_eq,
			.tok = tok,
			.info.binary.left = left,
			.info.binary.right = right,
			.line = tok.line,
			.chr = tok.chr,
		}
	);
}

static ast_t *parse_binary(
	parser_t *const state,
	const prec_t prec,
	ast_t *left
) {
	const tok_t tok = state->lexer.curr;
	lexer_next(&state->lexer);
	ast_t *const right = parse_expression(state, (tok.ty == tok_minusminus || tok.ty == tok_plusplus) ? prec_lowest : prec - 1);
	return parser_add(
		state,
		(ast_t){
			.ty = ast_binary,
			.tok = tok,
			.info.binary.left = left,
			.info.binary.right = right,
			.line = tok.line,
			.chr = tok.chr,
		}
	);
}

static ast_t *parse_access_prefix(parser_t *const state) {
	const tok_t tok = state->lexer.curr;
	lexer_next(&state->lexer);

	if (tok.ty == tok_lbrack) {
		ast_t *const arridx = parse_expression(state, prec_full);
		parser_eat(state, tok_rbrack, "\"]\"");
		return parser_add(state, (ast_t){
			.ty = ast_accessor,
			.tok = tok,
			.info.accessor = {
				.accessing = arridx,
			},
			.line = tok.line,
			.chr = tok.chr,
		});
	} else {
		ast_t *const inner = parse_expression(state, prec_2);
		return parser_add(state, (ast_t){
			.ty = ast_accessor,
			.tok = tok,
			.info.inner = inner,
			.line = tok.line,
			.chr = tok.chr,
		});
	}
}
static ast_t *parse_access_post(
	parser_t *const state,
	const prec_t prec,
	ast_t *left
) {
	const tok_t tok = state->lexer.curr;
	lexer_next(&state->lexer);
	if (tok.ty == tok_dot || tok.ty == tok_arrow) {
		ast_t *const right = parse_expression(state, prec_lowest);
		return parser_add(state, (ast_t){
			.ty = ast_accessor,
			.tok = tok,
			.info.accessor = {
				.access_from = left,
				.accessing = right,
			},
			.line = tok.line,
			.chr = tok.chr,
		});
	} else {
		ast_t *const arridx = parse_expression(state, prec_full);
		parser_eat(state, tok_rbrack, "\"]\"");
		return parser_add(state, (ast_t){
			.ty = ast_accessor,
			.tok = tok,
			.info.accessor = {
				.access_from = left,
				.accessing = arridx,
			},
			.line = tok.line,
			.chr = tok.chr,
		});
	}
}
static ast_t *parse_ident(parser_t *const state) {
	const tok_t name = state->lexer.curr;
	lexer_next(&state->lexer);
	return parser_add(state, (ast_t){
		.ty = ast_ident,
		.tok = name,
		.line = name.line,
		.chr = name.chr,
	});
}

struct parser_result parse(const char *src) {
	parser_t state = (parser_t){
		.lexer = lexer_init(src),
		.errs = arena_alloc(sizeof(err_t) * MAX_ERRS),
		.tymap = arena_alloc(sizeof(ident_map_t) + sizeof(ident_ent_t) * 1024),
		.nerrs = 0,
		.root = NULL,
	};
	state.ast_pool = dynpool_init(128, sizeof(ast_t), arena_alloc);
	*state.tymap = ident_map_init(1024);
	lexer_next(&state.lexer);
	parse_toplevels(&state);
	return (struct parser_result){
		.root = state.root,
		.ast_pool = state.ast_pool,
		.errs = state.errs,
		.nerrs = state.nerrs,
	};
}

static cvrflags_t parse_cvrflags(parser_t *const state) {
	cvrflags_t cvrs = cvr_none;
	while (true) {
		switch (state->lexer.curr.ty) {
		case tok_static: cvrs |= cvr_static; break;
		case tok_const: cvrs |= cvr_const; break;
		case tok_restrict: cvrs |= cvr_restrict; break;
		case tok_volatile: cvrs |= cvr_volatile; break;
		case tok_extern: cvrs |= cvr_extern; break;
		default: return cvrs;
		}
		lexer_next(&state->lexer);
	}
}

static ast_t *parse_type_not_arr_ptr_func(parser_t *const state) {
	const int line = state->lexer.curr.line, chr = state->lexer.curr.chr;
	cvrflags_t cvrs = parse_cvrflags(state);
	switch (state->lexer.curr.ty) {
	case tok_union:
	case tok_struct:
		{
			const bool is_struct = state->lexer.curr.ty == tok_struct;
			lexer_next(&state->lexer);
			ast_t *tyname = NULL;
			if (state->lexer.curr.ty != tok_lbrace) tyname = parse_ident(state);
			ast_t *members = NULL;
			if (state->lexer.curr.ty == tok_lbrace) { // structure definition
				lexer_next(&state->lexer); // parse members
				ast_t **last = &members;
				while (state->lexer.curr.ty != tok_rbrace && state->lexer.curr.ty != tok_eof && state->lexer.curr.ty != tok_error) {
					ast_t *member = parse_type(state, true, true);
					if (!member) break;
					*last = member;
					while (member->next) member = member->next;
					last = &member->next;
					parser_eat(state, tok_semicol, "\";\"");
				}
				lexer_next(&state->lexer);
			}
			return parser_add(state, (ast_t){
				.ty = ast_type,
				.tok = (tok_t){ .ty = tok_undefined },
				.info.type = {
					.ty = is_struct ? decl_struct : decl_union,
					.tyname = tyname,
					.cvrs = cvrs,
					.inner = members,
				},
				.line = line,
				.chr = chr,
			});
		}
		break;
	case tok_enum:
		{
			lexer_next(&state->lexer);
			ast_t *const tyname = parse_ident(state);

			ast_t *fields = NULL;
			if (state->lexer.curr.ty == tok_lbrace) { // enum definition
				lexer_next(&state->lexer); // parse fields
				ast_t **last = &fields;
				while (state->lexer.curr.ty != tok_rbrace && state->lexer.curr.ty != tok_eof && state->lexer.curr.ty != tok_error) {
					const int line = state->lexer.curr.line, chr = state->lexer.curr.chr;
					const tok_t field_name = state->lexer.curr;
					ast_t *value = NULL;

					lexer_next(&state->lexer);
					if (state->lexer.curr.ty == tok_eq) {
						lexer_next(&state->lexer);
						value = parse_expression(state, prec_ternary);
					}
					if (state->lexer.curr.ty == tok_comma) lexer_next(&state->lexer);

					ast_t *const field = parser_add(state, (ast_t){
						.ty = ast_enum_field,
						.tok = field_name,
						.info.inner = value,
						.line = line,
						.chr = chr,
					});
					*last = field;
					last = &field->next;
				}
				lexer_next(&state->lexer);
			}
			return parser_add(state, (ast_t){
				.ty = ast_type,
				.tok = (tok_t){ .ty = tok_undefined },
				.info.type = {
					.ty = decl_enum,
					.tyname = tyname,
					.cvrs = cvrs,
					.inner = fields,
				},
				.line = line,
				.chr = chr,
			});
		}
		break;
	case tok_ident: // Using an already made typedef
		{
			if (!ident_map_get(state->tymap, state->lexer.curr.lit, state->lexer.curr.len, NULL)) return NULL;

			ast_t *const tyname = parse_ident(state);
			return parser_add(state, (ast_t){
				.ty = ast_type,
				.tok = (tok_t){ .ty = tok_undefined },
				.info.type = {
					.ty = decl_typedef,
					.cvrs = cvrs,
					.tyname = tyname,
				},
				.line = line,
				.chr = chr,
			});
		}
		break;
	case tok_void:
		lexer_next(&state->lexer);
		return parser_add(state, (ast_t){
			.ty = ast_type,
			.tok = (tok_t){ .ty = tok_undefined },
			.info.type = {
				.ty = decl_void,
				.cvrs = cvrs,
			},
			.line = line,
			.chr = chr,
		});
	default:
		break; 
	}

	bool has_typespec = false;
	int bits = 32;
	int f = 0, i = 0, u = 0, l = 0;
	while (true) {
		switch (state->lexer.curr.ty) {
		case tok_unsigned: u++; break;
		case tok_char: i++; bits = 8; break;
		case tok_short: i++; bits = 16; break;
		case tok_int: i++; bits = 32; break;
		case tok_long: l++; bits = 64; break;
		case tok_float: f++; bits = 32; break;
		case tok_double: f++; bits = 64; break;
		default: goto construct_pod;
		}
		has_typespec = true;
		lexer_next(&state->lexer);
	}
construct_pod:
	if (!has_typespec) return NULL;
	if ((u > 1 || f > 1 || l > 2)
		|| (u && f)
		|| (i && f)
		|| (l && f)) {
		parser_err(state, (err_t){ .ty = err_unexpected, .msg = "incorrect type spec info", .line = state->lexer.curr.line, .chr = state->lexer.curr.chr });
	}
	unitty_t pod;
	switch (bits) {
	case 8: pod = u ? unit_u8 : unit_i8; break;
	case 16: pod = u ? unit_u16 : unit_i16; break;
	case 32: pod = f ? unit_f32 : (u ? unit_u32 : unit_i32); break;
	case 64: pod = f ? unit_f64 : (u ? unit_u64 : unit_i64); break;
	default: pod = unit_undefined; break;
	}

	return parser_add(state, (ast_t){
		.ty = ast_type,
		.tok = (tok_t){ .ty = tok_undefined },
		.info.type = {
			.ty = decl_pod,
			.cvrs = cvrs,
			.pod = pod,
		},
		.line = line,
		.chr = chr,
	});
}

// Parse initializer
// Could return either expression, or ast_init_list
static ast_t *parse_initializer(parser_t *const state, const bool allow_desginator) {
	const int line = state->lexer.curr.line, chr = state->lexer.curr.chr;
	if ((state->lexer.curr.ty == tok_dot || state->lexer.curr.ty == tok_lbrack) && allow_desginator) {
		if (state->lexer.curr.ty == tok_dot) lexer_next(&state->lexer);
		ast_t *const accessor = parse_expression(state, prec_2);
		parser_eat(state, tok_eq, "\"=\"");
		ast_t *const init = parse_initializer(state, false);
		return parser_add(state, (ast_t){
			.ty = ast_init_designator,
			.tok = (tok_t){ .ty = tok_undefined },
			.info.designator = {
				.accessor = accessor,
				.init = init,
			},
			.line = line,
			.chr = chr,
		});
	}
	if (state->lexer.curr.ty != tok_lbrace) return parse_expression(state, prec_assign);
	lexer_next(&state->lexer);

	ast_t *list = NULL;
	ast_t **last = &list;
	while (state->lexer.curr.ty != tok_rbrace && state->lexer.curr.ty != tok_eof && state->lexer.curr.ty != tok_error) {
		ast_t *const node = parse_initializer(state, true);
		*last = node;
		last = &node->next;
		if (state->lexer.curr.ty == tok_comma) lexer_next(&state->lexer);
		else break;
	}

	parser_eat(state, tok_rbrace, "\"}\"");
	return parser_add(state, (ast_t){
		.ty = ast_init_list,
		.tok = (tok_t){ .ty = tok_undefined },
		.info.inner = list,
		.line = line,
		.chr = chr,
	});
}

// Multi decl's are for stuff like "int a, b, c;" (not including semicolon)
// Obviously, function parameters are a situation where you would not want
// multi_decl
static ast_t *_parse_type(parser_t *const state, const bool multi_decl, const bool allow_idents, ast_t *const tyspec) {
	const int line = state->lexer.curr.line, chr = state->lexer.curr.chr;
	ast_t *curr_node = tyspec; // currnode is the child
							// tyspec is always child of curr_node
	ast_t *root = tyspec;		// root of declaration
	ast_t **parent = &root;	// parent child pointer of curr_node
	
	// Collect tokens before the ident, then store ident for later?
	tok_t before[32];
	int btop = -1, bparens = 0;
	tok_t ident = (tok_t){ .ty = tok_undefined };
	while (state->lexer.curr.ty == tok_star // Check for anything for pointer wise
		|| state->lexer.curr.ty == tok_lparen
		|| state->lexer.curr.ty == tok_const
		|| state->lexer.curr.ty == tok_static
		|| state->lexer.curr.ty == tok_extern
		|| state->lexer.curr.ty == tok_restrict
		|| state->lexer.curr.ty == tok_volatile
		|| state->lexer.curr.ty == tok_rparen) {
		bparens += state->lexer.curr.ty == tok_lparen;
		if (state->lexer.curr.ty == tok_rparen && --bparens <= 0) break;
		before[++btop] = state->lexer.curr;
		lexer_next(&state->lexer);
	}
	if (state->lexer.curr.ty == tok_ident) { // Get identifier if we can
		if (!allow_idents) {
			parser_err(state, (err_t){ .ty = err_unexpected, .msg = "unexpected identifier", .line = state->lexer.curr.line, .chr = state->lexer.curr.chr });
			return NULL;
		}
		ident = state->lexer.curr;
		lexer_next(&state->lexer);
	} // If not its undefined identifier

	// Now parse functions, pointers, and arrays
	while (true) {
		// If function/array is here then do it!
		while (state->lexer.curr.ty == tok_lbrack
			|| state->lexer.curr.ty == tok_lparen) {
			if (state->lexer.curr.ty == tok_lbrack) {
				lexer_next(&state->lexer);
				const cvrflags_t cvrs = parse_cvrflags(state);
				curr_node = parser_add(state, (ast_t){
					.ty = ast_type,
					.tok = (tok_t){ .ty = tok_undefined },
					.info.type = {
						.ty = decl_array_of,
						.cvrs = cvrs,
						.arrlen = parse_expression(state, prec_full),
						.inner = tyspec,
					},
					.line = line,
					.chr = chr,
				});
				parser_eat(state, tok_rbrack, "\"]\"");

				*parent = curr_node;
				parent = &curr_node->info.type.inner;
			} else { // function
				lexer_next(&state->lexer);
				ast_t *params = NULL;
				ast_t **last = &params;
				while (state->lexer.curr.ty != tok_rparen) {
					ast_t *param = parse_type(state, false, true);
					*last = param;
					last = &param->next;
					if (state->lexer.curr.ty == tok_comma) lexer_next(&state->lexer);
					else break;
				}
				parser_eat(state, tok_rparen, "\")\"");
				curr_node = parser_add(state, (ast_t){
					.ty = ast_type,
					.tok = (tok_t){ .ty = tok_undefined },
					.info.type = {
						.ty = decl_function,
						.cvrs = root->info.type.cvrs,
						.inner = tyspec,
						.funcparams = params,
					},
					.line = line,
					.chr = chr,
				});

				*parent = curr_node;
				parent = &curr_node->info.type.inner;
			}
		}

		bool parened = state->lexer.curr.ty == tok_rparen && btop >= 0;

		// Get pointers and remove parenthases
		cvrflags_t cvrs = cvr_none;
		while (btop >= 0 && before[btop].ty != tok_lparen) {
			switch (before[btop].ty) {
			case tok_static: cvrs |= cvr_static; break;
			case tok_const: cvrs |= cvr_const; break;
			case tok_restrict: cvrs |= cvr_restrict; break;
			case tok_volatile: cvrs |= cvr_volatile; break;
			case tok_extern: cvrs |= cvr_extern; break;
			case tok_star:
				curr_node = parser_add(state, (ast_t){
					.ty = ast_type,
					.tok = (tok_t){ .ty = tok_undefined },
					.info.type = {
						.ty = decl_pointer_to,
						.cvrs = cvrs,
						.inner = tyspec,
					},
				});

				*parent = curr_node;
				parent = &curr_node->info.type.inner;
				cvrs = cvr_none;
				break;
			default: assert(false && "implement error handling");
			}
			btop--;
		}
		if (parened && before[btop].ty == tok_lparen) lexer_next(&state->lexer);
		else parened = false;
		btop--;

		if (!parened) break;
	}

	// Add the definition onto it now
	ast_t *def = NULL;
	if (state->lexer.curr.ty == tok_eq && allow_idents) {
		lexer_next(&state->lexer);
		def = parse_initializer(state, false);
	}
	if (allow_idents && ident.ty != tok_undefined) {
		root = parser_add(state, (ast_t){
			.ty = ast_decldef,
			.info.decldef = {
				.type = root,
				.def = def,
			},
			.tok = ident,
			.line = line,
			.chr = chr,
		});
	}

	if (multi_decl && state->lexer.curr.ty == tok_comma && allow_idents) {
		lexer_next(&state->lexer);
		ast_t *shallow_copy = parser_add(state, *tyspec);
		shallow_copy->info.type.inner = NULL;
		shallow_copy->next = NULL;
		root->next = _parse_type(state, true, true, shallow_copy);
	}

	return root;
}

static ast_t *parse_type(parser_t *const state, const bool multi_decl, const bool allow_idents) {
	bool is_typedef = false;
	if (state->lexer.curr.ty == tok_typedef) {
		is_typedef = true;
		lexer_next(&state->lexer);
	}
	// Type specifier (can be a whole struct definition!!!!!!)
	ast_t *const tyspec = parse_type_not_arr_ptr_func(state);
	if (!tyspec) return NULL;
	// Now parse pointers, functions, and arrays (and get identifier)
	ast_t *const decl = _parse_type(state, multi_decl, allow_idents, tyspec);

	if (is_typedef) {
		// Add the typedef to the list
		ident_map_add(state->tymap, decl->tok.lit, decl->tok.len, 0);

		return parser_add(state, (ast_t){
			.ty = ast_typedef,
			.tok = (tok_t){ .ty = tok_undefined },
			.info.inner = decl,
			.line = decl->line,
			.chr = decl->chr,
		});
	} else {
		return decl;
	}
}

static ast_t *parse_compound_stmt(parser_t *const state) {
	const int line = state->lexer.curr.line, chr = state->lexer.curr.chr;

	lexer_next(&state->lexer);
	ast_t *stmts = NULL;
	ast_t **last = &stmts;
	while (last && state->lexer.curr.ty != tok_rbrace && state->lexer.curr.ty != tok_eof && state->lexer.curr.ty != tok_error) {
		ast_t *stmt = parse_statement(state);
		*last = stmt;
		last = stmt ? &stmt->next : NULL;
	}
	parser_eat(state, tok_rbrace, "\"}\"");
	return parser_add(state, (ast_t){
		.ty = ast_stmt,
		.info.stmt = {
			.ty = stmt_compound,
			.inner = stmts,
		},
		.tok = (tok_t){ .ty = tok_undefined },
		.line = line,
		.chr = chr,
	});
}

static void parse_label(parser_t *const state, labelty_t *const ty, ast_t **const label) {
	// get the label
	if (state->lexer.curr.ty == tok_ident && state->lexer.peek.ty == tok_colon) {
		*ty = label_norm;
		*label = parse_ident(state);
		lexer_next(&state->lexer);
	} else if (state->lexer.curr.ty == tok_case) {
		*ty = label_case;
		lexer_next(&state->lexer);
		*label = parse_ident(state);
		parser_eat(state, tok_colon, "\":\"");
	} else if (state->lexer.curr.ty == tok_default) {
		*ty = label_default;
		*label = NULL;
		lexer_next(&state->lexer);
		parser_eat(state, tok_colon, "\":\"");
	} else {
		*ty = label_none;
		*label = NULL;
	}
}

static ast_t *parse_ifelse_stmt(parser_t *const state) {
	const int line = state->lexer.curr.line, chr = state->lexer.curr.chr;

	lexer_next(&state->lexer);
	parser_eat(state, tok_lparen, "\"(\"");
	ast_t *const cond = parse_expression(state, prec_full);
	parser_eat(state, tok_rparen, "\")\"");
	ast_t *const on_true = parse_statement(state);
	ast_t *on_false = NULL;
	if (state->lexer.curr.ty == tok_else) {
		lexer_next(&state->lexer);
		on_false = parse_statement(state);
	}
	return parser_add(state, (ast_t){
		.ty = ast_stmt,
		.info.stmt = {
			.ty = stmt_ifelse,
			.ifelse = {
				.cond = cond,
				.on_true = on_true,
				.on_false = on_false,
			},
		},
		.tok = (tok_t){ .ty = tok_undefined },
		.line = line,
		.chr = chr,
	});
}
static ast_t *parse_switch_stmt(parser_t *const state) {
	const int line = state->lexer.curr.line, chr = state->lexer.curr.chr;

	lexer_next(&state->lexer);
	parser_eat(state, tok_lparen, "\"(\"");
	ast_t *const cond = parse_expression(state, prec_full);
	parser_eat(state, tok_rparen, "\")\"");
	ast_t *const inner = parse_statement(state);
	return parser_add(state, (ast_t){
		.ty = ast_stmt,
		.info.stmt = {
			.ty = stmt_switch,
			.cond = {
				.cond = cond,
				.inner = inner,
			},
		},
		.tok = (tok_t){ .ty = tok_undefined },
		.line = line,
		.chr = chr,
	});
}
static ast_t *parse_while_stmt(parser_t *const state) {
	const int line = state->lexer.curr.line, chr = state->lexer.curr.chr;

	lexer_next(&state->lexer);
	parser_eat(state, tok_lparen, "\"(\"");
	ast_t *const cond = parse_expression(state, prec_full);
	parser_eat(state, tok_rparen, "\")\"");
	ast_t *const inner = parse_statement(state);
	return parser_add(state, (ast_t){
		.ty = ast_stmt,
		.info.stmt = {
			.ty = stmt_while,
			.cond = {
				.cond = cond,
				.inner = inner,
			},
		},
		.tok = (tok_t){ .ty = tok_undefined },
		.line = line,
		.chr = chr,
	});
}
static ast_t *parse_dowhile_stmt(parser_t *const state) {
	const int line = state->lexer.curr.line, chr = state->lexer.curr.chr;

	lexer_next(&state->lexer);
	ast_t *const inner = parse_statement(state);
	parser_eat(state, tok_while, "\"while\"");
	parser_eat(state, tok_lparen, "\"(\"");
	ast_t *const cond = parse_expression(state, prec_full);
	parser_eat(state, tok_rparen, "\")\"");
	parser_eat(state, tok_semicol, "\";\"");
	return parser_add(state, (ast_t){
		.ty = ast_stmt,
		.info.stmt = {
			.ty = stmt_while,
			.cond = {
				.cond = cond,
				.inner = inner,
			},
		},
		.tok = (tok_t){ .ty = tok_undefined },
		.line = line,
		.chr = chr,
	});
}
static ast_t *parse_for_stmt(parser_t *const state) {
	const int line = state->lexer.curr.line, chr = state->lexer.curr.chr;

	lexer_next(&state->lexer);
	parser_eat(state, tok_lparen, "\"(\"");
	ast_t *init_clause = parse_expression(state, prec_full);
	if (!init_clause) init_clause = parse_type(state, true, true);
	parser_eat(state, tok_semicol, "\";\"");
	ast_t *const cond_expr = parse_expression(state, prec_full);
	parser_eat(state, tok_semicol, "\";\"");
	ast_t *const iter_expr = parse_expression(state, prec_full);
	parser_eat(state, tok_rparen, "\")\"");
	ast_t *const stmt = parse_statement(state);
	return parser_add(state, (ast_t){
		.ty = ast_stmt,
		.info.stmt = {
			.ty = stmt_for,
			.forinfo = {
				.init_clause = init_clause,
				.cond_expr = cond_expr,
				.iter_expr = iter_expr,
				.stmt = stmt,
			},
		},
		.tok = (tok_t){ .ty = tok_undefined },
		.line = line,
		.chr = chr,
	});
}
static ast_t *parse_ret_stmt(parser_t *const state) {
	const int line = state->lexer.curr.line, chr = state->lexer.curr.chr;

	lexer_next(&state->lexer);
	ast_t *const expr = parse_expression(state, prec_full);
	parser_eat(state, tok_semicol, "\";\"");
	return parser_add(state, (ast_t){
		.ty = ast_stmt,
		.info.stmt = {
			.ty = stmt_return,
			.inner = expr,
		},
		.tok = (tok_t){ .ty = tok_undefined },
		.line = line,
		.chr = chr,
	});
}
static ast_t *parse_goto_stmt(parser_t *const state) {
	const int line = state->lexer.curr.line, chr = state->lexer.curr.chr;

	lexer_next(&state->lexer);
	ast_t *const label = parse_ident(state);
	parser_eat(state, tok_semicol, "\";\"");
	return parser_add(state, (ast_t){
		.ty = ast_stmt,
		.info.stmt = {
			.ty = stmt_goto,
			.inner = label,
		},
		.tok = (tok_t){ .ty = tok_undefined },
		.line = line,
		.chr = chr,
	});
}
static ast_t *parse_expr_decldef_stmt(parser_t *const state) {
	const int line = state->lexer.curr.line, chr = state->lexer.curr.chr;

	ast_t *const decl = parse_type(state, true, true);
	if (decl) {
		parser_eat(state, tok_semicol, "\";\"");
		return parser_add(state, (ast_t){
			.ty = ast_stmt,
			.info.stmt = {
				.ty = stmt_decldef,
				.inner = decl,
			},
			.tok = (tok_t){ .ty = tok_undefined },
			.line = line,
			.chr = chr,
		});
	}
	ast_t *const expr = parse_expression(state, prec_full);
	if (expr) {
		parser_eat(state, tok_semicol, "\";\"");
		return parser_add(state, (ast_t){
			.ty = ast_stmt,
			.info.stmt = {
				.ty = stmt_expr,
				.inner = expr,
			},
			.tok = (tok_t){ .ty = tok_undefined },
			.line = line,
			.chr = chr,
		});
	}
	return NULL;
}

static ast_t *parse_statement(parser_t *const state) {
	const int line = state->lexer.curr.line, chr = state->lexer.curr.chr;

	labelty_t labelty;
	ast_t *label;
	parse_label(state, &labelty, &label);

	ast_t *stmt = NULL;
	switch (state->lexer.curr.ty) {
	case tok_lbrace: stmt = parse_compound_stmt(state); break;
	case tok_if: stmt = parse_ifelse_stmt(state); break;
	case tok_switch: stmt = parse_switch_stmt(state); break;
	case tok_while: stmt = parse_while_stmt(state); break;
	case tok_do: stmt = parse_dowhile_stmt(state); break;
	case tok_for: stmt = parse_for_stmt(state); break;
	case tok_break:
		lexer_next(&state->lexer);
		stmt = parser_add(state, (ast_t){
			.ty = ast_stmt,
			.info.stmt = { .ty = stmt_break, },
			.tok = (tok_t){ .ty = tok_undefined },
			.line = line,
			.chr = chr,
		});
		break;
	case tok_continue:
		lexer_next(&state->lexer);
		stmt = parser_add(state, (ast_t){
			.ty = ast_stmt,
			.info.stmt = { .ty = stmt_cont, },
			.tok = (tok_t){ .ty = tok_undefined },
			.line = line,
			.chr = chr,
		});
		break;
	case tok_return: stmt = parse_ret_stmt(state); break;
	case tok_goto: stmt = parse_goto_stmt(state); break;
	case tok_semicol:
		lexer_next(&state->lexer);
		stmt = parser_add(state, (ast_t){
			.ty = ast_stmt,
			.info.stmt = { .ty = stmt_null, },
			.tok = (tok_t){ .ty = tok_undefined },
			.line = line,
			.chr = chr,
		});
		break;
	default: stmt = parse_expr_decldef_stmt(state); break;
	}

	if (stmt) {
		stmt->info.stmt.label = label;
		stmt->info.stmt.labelty = labelty;
	}
	return stmt;
}

static void parse_decl_toplvl(parser_t *const state, ast_t **const first_def, ast_t **const last_def) {
	ast_t *node = parse_type(state, true, true);

	if (node
		&& node->ty == ast_decldef
		&& !node->info.decldef.def
		&& state->lexer.curr.ty == tok_lbrace) {
		node->info.decldef.def = parse_statement(state);
	} else {
		parser_eat(state, tok_semicol, "\";\"");
	}

	if (first_def) *first_def = node;
	while (node && node->next) node = node->next;
	if (last_def) *last_def = node;
}

static void parse_toplevels(parser_t *const state) {
	ast_t **last = &state->root;
	while (state->lexer.curr.ty != tok_eof && state->lexer.curr.ty != tok_error) {
		ast_t *node = NULL;
		parse_decl_toplvl(state, last, &node);
		last = node ? &node->next : NULL;
	}
}

static ast_t *parse_expression(
	parser_t *const state,
	const prec_t prec
) {
	const parse_unary_pfn prefix = _rules[state->lexer.curr.ty].prefix;
	if (!prefix) return NULL;
	ast_t *left = prefix(state);

	// Support for (ONLY ternay operator)
	if (state->lexer.curr.ty == tok_qmark && prec >= prec_ternary) {
		const tok_t tok = state->lexer.curr;
		lexer_next(&state->lexer);
		ast_t *const middle = parse_expression(state, prec_ternary);
		parser_eat(state, tok_colon, "\":\"");
		ast_t *const right = parse_expression(state, prec_ternary);
		left = parser_add(state, (ast_t){
			.ty = ast_ternary,
			.tok = tok,
			.info.ternary = {
				.cond = left,
				.ontrue = middle,
				.onfalse = right,
			},
			.line = tok.line,
			.chr = tok.chr,
		});
	} else if (state->lexer.curr.ty == tok_colon) {
		return left;
	}

	while (prec >= _rules[state->lexer.curr.ty].prec) {
		const parse_binary_pfn infix = _rules[state->lexer.curr.ty].infix;
		left = infix(state, _rules[state->lexer.curr.ty].prec, left);
	}

	return left;
}

static typed_unit_t parse_number(parser_t *const state) {
	// TODO: bigger numbers and postfixes for numbers
	char numbuf[25];
	const tok_t tok = state->lexer.curr;
	if (tok.len > 24) {
		parser_err(state, (err_t){ .ty = err_limit, .msg = "exceeded max literal integer size", .line = state->lexer.curr.line, .chr = state->lexer.curr.chr });
		return (typed_unit_t){ .ty = unit_undefined };
	}
	memcpy(numbuf, tok.lit, tok.len);
	numbuf[tok.len] = '\0';
	lexer_next(&state->lexer);
	int64_t i = atoll(numbuf);
	if (i <= INT32_MAX) {
		return (typed_unit_t){
				.ty = unit_i32,
				.unit = (unit_t){ .i32 = i },
		};
	} else if (i <= UINT32_MAX) {
		return (typed_unit_t){
				.ty = unit_u32,
				.unit = (unit_t){ .u32 = i },
		};
	} else if (i <= INT64_MAX) {
		return (typed_unit_t){
				.ty = unit_i64,
				.unit = (unit_t){ .i64 = i },
		};
	} else {
		return (typed_unit_t){
				.ty = unit_u64,
				.unit = (unit_t){ .u64 = i },
		};
	}
}
static ast_t *parse_string_lit(parser_t *const state) {
	tok_t tok = state->lexer.curr;
	lexer_next(&state->lexer);
	return parser_add(state, (ast_t){
		.ty = ast_literal,
		.tok = tok,
		.info.literal = {
			.ty = tok.ty == tok_string ? lit_str : lit_char,
			.val.lit = tok,
		},
		.line = tok.line,
		.chr = tok.chr,
	});
}
static ast_t *parse_number_lit(parser_t *const state) {
	tok_t tok = state->lexer.curr;
	return parser_add(state, (ast_t){
		.ty = ast_literal,
		.tok = tok,
		.info.literal = {
			.ty = lit_pod,
			.val.pod = parse_number(state),
		},
		.line = tok.line,
		.chr = tok.chr,
	});
}

static void _dbg_cvr_print(cvrflags_t cvrs) {
	if (cvrs & cvr_extern) printf("extern ");
	if (cvrs & cvr_volatile) printf("volatile ");
	if (cvrs & cvr_const) printf("const ");
	if (cvrs & cvr_static) printf("static ");
	if (cvrs & cvr_restrict) printf("restrict ");
}
void dbg_ast_print(const ast_t *node, int tabs) {
#define TAB for (int i = 0; i < tabs; i++) printf("  ");
#define LENPRINT(lit, len) for (int _i = 0; _i < (len); _i++) printf("%c", (lit)[_i]);
	if (!node) return;
	switch (node->ty) {
	case ast_binary: TAB printf("binary");
	case ast_eq:
		if (node->ty == ast_eq) {
			TAB printf("equal");
		}
		printf(": %s\n", tokty_to_str(node->tok.ty));
		dbg_ast_print(node->info.binary.left, tabs+1);
		dbg_ast_print(node->info.binary.right, tabs+1);
		break;
	case ast_comma:
		TAB printf("comma expr: \n");
		dbg_ast_print(node->info.comma.expr, tabs+1);
		TAB printf("comma next: \n");
		dbg_ast_print(node->info.comma.next, tabs+1);
		break;
	case ast_ternary:
		TAB printf("ternary cond: \n");
		dbg_ast_print(node->info.ternary.cond, tabs+1);
		TAB printf("ternary on true: \n");
		dbg_ast_print(node->info.ternary.ontrue, tabs+1);
		TAB printf("ternary on false: \n");
		dbg_ast_print(node->info.ternary.onfalse, tabs+1);
		break;
	case ast_func_call:
		TAB printf("calling function:\n");
		dbg_ast_print(node->info.func_call.func, tabs+1);
		TAB printf("with params:\n");
		dbg_ast_print(node->info.func_call.params, tabs+1);
		break;
	case ast_unary:
		TAB printf("unary: %s\n", tokty_to_str(node->tok.ty));
		dbg_ast_print(node->info.inner, tabs+1);
		break;
	case ast_literal:
		switch (node->info.literal.ty) {
		case lit_pod:
			TAB printf("pod literal:\n");
			TAB printf("  "); dbg_typed_unit_print(node->info.literal.val.pod);
			break;
		case lit_str:
			TAB printf("string literal:\n");
			TAB printf("  "); LENPRINT(node->info.literal.val.lit.lit, node->info.literal.val.lit.len) printf("\n");
			break;
		case lit_char:
			TAB printf("character literal:\n");
			TAB printf("  "); LENPRINT(node->info.literal.val.lit.lit, node->info.literal.val.lit.len) printf("\n");
			break;
		case lit_compound:
			TAB printf("compound literal:\n");
			TAB printf("  type:\n");
			dbg_ast_print(node->info.literal.val.compound.type, tabs+2);
			TAB printf("  initialized to:\n");
			dbg_ast_print(node->info.literal.val.compound.init, tabs+2);
			break;
		}
		break;
	case ast_ident:
		TAB printf("ident: ");
		LENPRINT(node->tok.lit, node->tok.len)
		printf("\n");
		break;
	case ast_cast:
		TAB printf("cast: \n");
		dbg_ast_print(node->info.cast.expr, tabs+1);
		TAB printf("to type: \n");
		dbg_ast_print(node->info.cast.type, tabs+1);
		break;
	case ast_accessor:
		TAB printf("accessor: ");
		switch (node->tok.ty) {
		case tok_arrow: printf("member pointer access\n");
		case tok_dot:
		case tok_lbrack:
			if (node->tok.ty == tok_lbrack) printf("subscript\n");
			if (node->tok.ty == tok_dot) printf("member access\n");
			TAB printf("accessing from: \n");
			dbg_ast_print(node->info.accessor.access_from, tabs+1);
			TAB printf("accessing: \n");
			dbg_ast_print(node->info.accessor.accessing, tabs+1);
			break;
		case tok_star: printf("derefrence");
		case tok_bitand:
			if (node->tok.ty == tok_bitand) printf("refrence");
			printf(" of \n");
			dbg_ast_print(node->info.inner, tabs+1);
			break;
		default: break;
		}
		break;
	case ast_typedef:
		TAB printf("decl typedef\n");
		dbg_ast_print(node->info.inner, tabs+1);
		break;
	case ast_enum_field:
		TAB LENPRINT(node->tok.lit, node->tok.len)
		if (node->info.inner) {
			printf(" = \n");
			dbg_ast_print(node->info.inner, tabs+1);
		} else {
			printf(",\n");
		}
		break;
	case ast_error:
		printf("ERR");
		break;
	case ast_type:
		{
			TAB
			printf("type ");
			_dbg_cvr_print(node->info.type.cvrs);
			switch (node->info.type.ty) {
			case decl_typedef:
				printf("typedef:\n");
				dbg_ast_print(node->info.type.tyname, tabs+1);
				break;
			case decl_void: printf("void\n"); break;
			case decl_function:
				printf("function returns\n");
				dbg_ast_print(node->info.type.inner, tabs+1);
				TAB printf("and takes\n");
				dbg_ast_print(node->info.type.funcparams, tabs+1);
				break;
			case decl_pointer_to:
				printf("pointer to\n");
				dbg_ast_print(node->info.type.inner, tabs + 1);
				break;
			case decl_array_of: 
				if (node->info.type.arrlen) {
					printf("array [\n");
					dbg_ast_print(node->info.type.arrlen, tabs+1);
					TAB printf("] of \n");
				} else {
					printf("array of \n");
				}
				dbg_ast_print(node->info.type.inner, tabs + 1); 
				break;
			case decl_pod: printf("%s\n", unitty_to_str(node->info.type.pod)); break;
			case decl_enum:
			case decl_struct:
			case decl_union:
				if (node->info.type.ty == decl_struct) printf("struct:\n");
				else if (node->info.type.ty == decl_enum) printf("enum:\n");
				else printf("union:\n");
				if (node->info.type.inner) {
					dbg_ast_print(node->info.type.tyname, tabs+1);
					TAB printf(" {\n");
					if (node->info.type.inner) {
						dbg_ast_print(node->info.type.inner, tabs+1);
					}
					TAB printf("}\n");
				} else {
					dbg_ast_print(node->info.type.tyname, tabs+1);
					//TAB printf("\n");
				}
				break;
			default: printf("undefined\n"); break;
			}
			break;
		}
	case ast_decldef:
		TAB
		if (node->info.decldef.def) printf("define ");
		else printf("declare ");
		LENPRINT(node->tok.lit, node->tok.len);
		printf(": \n");
		dbg_ast_print(node->info.decldef.type, tabs + 1);
		if (node->info.decldef.def) {
			TAB printf("as: \n");
			dbg_ast_print(node->info.decldef.def, tabs + 1);
		}
		break;
	case ast_init_list:
		TAB printf("{\n");
		dbg_ast_print(node->info.inner, tabs+1);
		TAB printf("}\n");
		break;
	case ast_init_designator:
		TAB printf("desginator: \n");
		dbg_ast_print(node->info.designator.accessor, tabs+1);
		TAB printf("initialized to: \n");
		dbg_ast_print(node->info.designator.init, tabs+1);
		break;
	case ast_stmt:
		{
			TAB
			switch (node->info.stmt.labelty) {
			case label_none: break;
			case label_default: printf("label default:\n"); break;
			case label_case: printf("label case ident:\n"); break;
			case label_norm: printf("label ident:\n"); break;
			}
			dbg_ast_print(node->info.stmt.label, tabs+1);
			if (node->info.stmt.labelty != label_none) TAB
			switch (node->info.stmt.ty) {
			case stmt_decldef:
				printf("decl/def stmt:\n");
				dbg_ast_print(node->info.stmt.inner, tabs+1);
				break;
			case stmt_expr:
				printf("expression stmt:\n");
				dbg_ast_print(node->info.stmt.inner, tabs+1);
				break;
			case stmt_break: printf("break stmt\n"); break;
			case stmt_cont: printf("continue stmt\n"); break;
			case stmt_return:
				printf("return stmt:\n");
				dbg_ast_print(node->info.stmt.inner, tabs+1);
				break;
			case stmt_goto:
				printf("goto stmt:\n");
				dbg_ast_print(node->info.stmt.inner, tabs+1);
				break;
			case stmt_compound:
				printf("compound stmt:\n");
				dbg_ast_print(node->info.stmt.inner, tabs+1);
				break;
			case stmt_dowhile:
				printf("dowhile stmt:\n");
				dbg_ast_print(node->info.stmt.cond.inner, tabs+1);
				TAB printf("dowhile cond:\n");
				dbg_ast_print(node->info.stmt.cond.cond, tabs+1);
				break;
			case stmt_while:
				printf("while cond:\n");
				dbg_ast_print(node->info.stmt.cond.cond, tabs+1);
				TAB printf("while stmt:\n");
				dbg_ast_print(node->info.stmt.cond.inner, tabs+1);
				break;
			case stmt_switch:
				printf("switch cond:\n");
				dbg_ast_print(node->info.stmt.cond.cond, tabs+1);
				TAB printf("switch stmt:\n");
				dbg_ast_print(node->info.stmt.cond.inner, tabs+1);
				break;
			case stmt_ifelse:
				printf("if cond:\n");
				dbg_ast_print(node->info.stmt.ifelse.cond, tabs+1);
				TAB printf("ontrue:\n");
				dbg_ast_print(node->info.stmt.ifelse.on_true, tabs+1);
				TAB printf("onfalse:\n");
				dbg_ast_print(node->info.stmt.ifelse.on_false, tabs+1);
				break;
			case stmt_for:
				printf("for init:\n");
				dbg_ast_print(node->info.stmt.forinfo.init_clause, tabs+1);
				TAB printf("for cond:\n");
				dbg_ast_print(node->info.stmt.forinfo.cond_expr, tabs+1);
				TAB printf("for iter:\n");
				dbg_ast_print(node->info.stmt.forinfo.iter_expr, tabs+1);
				TAB printf("for stmt:\n");
				dbg_ast_print(node->info.stmt.forinfo.stmt, tabs+1);
				break;
			case stmt_null: printf("null stmt:\n"); break;
			default: printf("stmt undefined\n"); break;
			}
		}
		break;
	default: break;
	}
	dbg_ast_print(node->next, tabs);
#undef TAB
}

