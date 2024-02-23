#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "parser.h"

static typed_unit_t parse_number(parser_t *const state);

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
	[tok_sizeof] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
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
	[tok_plus] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_additive },
	[tok_pluseq] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_minus] = (parse_rule_t){ .prefix = parse_unary, .infix = parse_binary, .prec = prec_additive },
	[tok_minuseq] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_star] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_multiplicative },
	[tok_dot] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_eq] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_eqeq] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_eqeq },
	[tok_bitand] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_bitand },
	[tok_and] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_and },
	[tok_bitor] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_bitor },
	[tok_or] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_or },
	[tok_bitnot] = (parse_rule_t){ .prefix = parse_unary, .infix = NULL, .prec = prec_null },
	[tok_noteq] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_eqeq },
	[tok_not] = (parse_rule_t){ .prefix = parse_unary, .infix = NULL, .prec = prec_null },
	[tok_xor] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_bitxor },
	[tok_lparen] = (parse_rule_t){ .prefix = parse_unary, .infix = NULL, .prec = prec_null },
	[tok_rparen] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_lbrack] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_rbrack] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_lbrace] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_rbrace] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_gt] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_relational },
	[tok_ge] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_relational },
	[tok_shr] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_bitshifting },
	[tok_lt] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_relational },
	[tok_le] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_relational },
	[tok_shl] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_bitshifting },
	[tok_comma] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_char_lit] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_semicol] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_slash] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_multiplicative },
	[tok_percent] = (parse_rule_t){ .prefix = NULL, .infix = parse_binary, .prec = prec_multiplicative },

	// Misc tokens
	[tok_string] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_integer] = (parse_rule_t){ .prefix = parse_number_lit, .infix = NULL, .prec = prec_null },
	[tok_floating] = (parse_rule_t){ .prefix = parse_number_lit, .infix = NULL, .prec = prec_null },
	[tok_ident] = (parse_rule_t){ .prefix = parse_ident, .infix = NULL, .prec = prec_null },
	[tok_error] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
	[tok_undefined] = (parse_rule_t){ .prefix = NULL, .infix = NULL, .prec = prec_null },
};

void parser_err(
	parser_t *const state,
	err_t err
) {
	if (state->nerrs == state->buflen - 1) {
		state->errs[state->nerrs++] = (err_t){ .ty = err_toomany, .msg = "Too many errors, stopping.", .line = err.line, .chr = err.chr };
	} else if (state->nerrs < state->buflen) {
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

static tok_t parser_eat(parser_t *const state, const tokty_t tokty, const char *const expect_msg) {
	if (state->lexer.curr.ty == tokty) {
		lexer_next(&state->lexer);
		return state->lexer.prev[2];
	} else {
		const err_t err = (err_t){ .ty = err_expected, .msg = expect_msg, .line = state->lexer.curr.line, .chr = state->lexer.curr.chr };
		parser_err(state, err);
		return (tok_t){ .ty = tok_error, .err = err, .lit = "", .len = 0, .line = err.line, .chr = err.chr, };
	}
}

int parse_unary(parser_t *const state) {
	const tok_t tok = state->lexer.curr;
	int val = AST_SENTINAL;
	lexer_next(&state->lexer);
	switch (tok.ty) {
	case tok_lparen:
		val = parse_expression(state, prec_assign);
		parser_eat(state, tok_rparen, "right paren");
		break;
	default:
		val = parse_expression(state, prec_grouping);
		break;
	}
	return parser_add(state, (ast_t){
		.ty = ast_unary,
		.tok = tok,
		.info.inner = val,
		.next = AST_SENTINAL,
	});
}

int parse_binary(
	parser_t *const state,
	const prec_t prec,
	int left
) {
	const tok_t tok = state->lexer.curr;
	lexer_next(&state->lexer);
	const int right = parse_expression(state, prec - 1);
	return parser_add(
		state,
		(ast_t){
			.ty = ast_binary,
			.tok = tok,
			.info.binary.left = left,
			.info.binary.right = right,
			.next = AST_SENTINAL,
		}
	);
}

parser_t parse(const char *src, ast_t *astbuf, size_t buflen) {
	parser_t state = (parser_t){
		.buflen = buflen,
		.buf = astbuf,
		.nnodes = 0,
		.lexer = lexer_init(src),
		.nerrs = 0,
		.root = AST_SENTINAL,
	};
	lexer_next(&state.lexer);
	parse_toplevels(&state);
	//state.root = parse_expression(&state, prec_full);
	return state;
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

static int parse_decl(parser_t *const state, const bool multi_decl);
int parse_decl_not_arr_ptr_func(parser_t *const state) {
	cvrflags_t cvrs = parse_cvrflags(state);
	switch (state->lexer.curr.ty) {
	case tok_struct:
		{
			lexer_next(&state->lexer);
			tok_t tyname = state->lexer.curr;
			lexer_next(&state->lexer);
			int members = AST_SENTINAL;
			if (state->lexer.curr.ty == tok_lbrace) { // structure definition
				lexer_next(&state->lexer); // parse members
				int *last = &members;
				while (state->lexer.curr.ty != tok_rbrace && state->lexer.curr.ty != tok_eof) {
					int member = parse_decl(state, true);
					*last = member;
					while (state->buf[member].next != AST_SENTINAL) member = state->buf[member].next;
					last = &state->buf[member].next;
					parser_eat(state, tok_semicol, "\";\"");
				}
				lexer_next(&state->lexer);
			}
			return parser_add(state, (ast_t){
				.ty = ast_decl,
				.tok = (tok_t){ .ty = tok_undefined },
				.next = AST_SENTINAL,
				.info.decl = {
					.ty = decl_struct,
					.tyname = tyname,
					.cvrs = cvrs,
					.inner = members,
				},
			});
			break;
		}
	case tok_enum: assert(false && "unimplemented");
	case tok_union: assert(false && "unimplemented");
	case tok_ident: assert(false && "unimplemented");
	case tok_void:
		lexer_next(&state->lexer);
		return parser_add(state, (ast_t){
			.ty = ast_decl,
			.tok = (tok_t){ .ty = tok_undefined },
			.next = AST_SENTINAL,
			.info.decl = {
				.ty = decl_void,
				.cvrs = cvrs,
			},
		});
	default: break; // we are parsing a POD
	}

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
		lexer_next(&state->lexer);
	}
construct_pod:
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
		.ty = ast_decl,
		.tok = (tok_t){ .ty = tok_undefined },
		.next = AST_SENTINAL,
		.info.decl = {
			.ty = decl_pod,
			.cvrs = cvrs,
			.pod = pod,
		},
	});
}

// Multi decl's are for stuff like "int a, b, c;" (not including semicolon)
// Obviously, function parameters are a situation where you would not want
// multi_decl
static int _parse_decl(parser_t *const state, const bool multi_decl, const int tyspec) {
	int curr_node = tyspec; // currnode is the child
							// tyspec is always child of curr_node
	int root = tyspec;		// root of declaration
	int *parent = &root;	// parent child pointer of curr_node
	
	// Collect tokens before the ident, then store ident for later?
	tok_t before[32];
	int btop = -1;
	tok_t ident = (tok_t){ .ty = tok_undefined };
	while (state->lexer.curr.ty == tok_star // Check for anything for pointer wise
		|| state->lexer.curr.ty == tok_lparen
		|| state->lexer.curr.ty == tok_const
		|| state->lexer.curr.ty == tok_static
		|| state->lexer.curr.ty == tok_extern
		|| state->lexer.curr.ty == tok_restrict
		|| state->lexer.curr.ty == tok_volatile) {
		before[++btop] = state->lexer.curr;
		lexer_next(&state->lexer);
	}
	if (state->lexer.curr.ty == tok_ident) { // Get identifier if we can
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
					.ty = ast_decl,
					.tok = (tok_t){ .ty = tok_undefined },
					.info.decl = {
						.ty = decl_array_of,
						.cvrs = cvrs,
						.arrlen = parse_expression(state, prec_full),
						.inner = tyspec,
					},
					.next = AST_SENTINAL,
				});

				*parent = curr_node;
				parent = &state->buf[curr_node].info.decl.inner;
			} else { // function
				lexer_next(&state->lexer);
				int params = AST_SENTINAL;
				int *last = &params;
				while (state->lexer.curr.ty != tok_rparen) {
					int param = parse_decl(state, false);
					*last = param;
					last = &state->buf[param].next;
					if (state->lexer.curr.ty == tok_comma) lexer_next(&state->lexer);
					else break;
				}
				parser_eat(state, tok_rparen, "\")\"");
				curr_node = parser_add(state, (ast_t){
					.ty = ast_decl,
					.tok = (tok_t){ .ty = tok_undefined },
					.info.decl = {
						.ty = decl_function,
						.cvrs = state->buf[root].info.decl.cvrs,
						.inner = tyspec,
						.funcparams = params,
					},
					.next = AST_SENTINAL,
				});

				*parent = curr_node;
				parent = &state->buf[curr_node].info.decl.inner;
			}
		}

		const bool parened = state->lexer.curr.ty == tok_rparen && btop >= 0;
		if (parened) lexer_next(&state->lexer);

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
					.ty = ast_decl,
					.next = AST_SENTINAL,
					.tok = (tok_t){ .ty = tok_undefined },
					.info.decl = {
						.ty = decl_pointer_to,
						.cvrs = cvrs,
						.inner = tyspec,
					},
				});

				*parent = curr_node;
				parent = &state->buf[curr_node].info.decl.inner;
				cvrs = cvr_none;
				break;
			default: assert(false && "implement error handling");
			}
			btop--;
		}
		btop--;

		if (!parened) break;
	}

	state->buf[root].tok = ident;

	if (multi_decl && state->lexer.curr.ty == tok_comma) {
		lexer_next(&state->lexer);
		int shallow_copy = parser_add(state, state->buf[tyspec]);
		state->buf[shallow_copy].info.decl.inner = AST_SENTINAL;
		state->buf[shallow_copy].next = AST_SENTINAL;
		state->buf[root].next = _parse_decl(state, true, shallow_copy);
	}

	return root;
}

static int parse_decl(parser_t *const state, const bool multi_decl) {
	// Type specifier (can be a whole struct definition!!!!!!)
	const int tyspec = parse_decl_not_arr_ptr_func(state);
	// Now parse pointers, functions, and arrays (and get identifier)
	return _parse_decl(state, multi_decl, tyspec);
}

void parse_toplevels(parser_t *const state) {
	int *last = &state->root;
	while (state->lexer.curr.ty != tok_eof) {
		int node = parse_decl(state, true);
		if (last) *last = node;
		while (state->buf[node].next != AST_SENTINAL) node = state->buf[node].next;
		last = node == AST_SENTINAL ? NULL : &state->buf[node].next;
		parser_eat(state, tok_semicol, "\";\"");
	}
}

int parse_expression(
	parser_t *const state,
	const prec_t prec
) {
	const parse_unary_pfn prefix = _rules[state->lexer.curr.ty].prefix;
	if (!prefix) {
		parser_err(state, (err_t){ .ty = err_expected, .msg = "unary expression", .line = state->lexer.curr.line, .chr = state->lexer.curr.chr });
		return AST_SENTINAL;
	}
	int left = prefix(state);

	while (_rules[state->lexer.curr.ty].prec <= prec) {
		const parse_binary_pfn infix = _rules[state->lexer.curr.ty].infix;
		left = infix(state, _rules[state->lexer.curr.ty].prec, left);
	}
	return left;
}

int parser_add(parser_t *const state, const ast_t newnode) {
	if (state->nnodes == state->buflen) {
		parser_err(state, (err_t){ .ty = err_limit, .msg = "max ast nodes reached", .line = state->lexer.curr.line, .chr = state->lexer.curr.chr });
		return AST_SENTINAL;
	}
	state->buf[state->nnodes] = newnode;
	return state->nnodes++;
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
int parse_number_lit(parser_t *const state) {
	tok_t tok = state->lexer.curr;
	return parser_add(state, (ast_t){
		.ty = ast_literal,
		.tok = tok,
		.info.val = parse_number(state),
		.next = AST_SENTINAL,
	});
}
int parse_ident(parser_t *const state) {
	lexer_next(&state->lexer);
	return parser_add(state, (ast_t){
		.ty = ast_ident,
		.tok = state->lexer.prev[2],
		.next = AST_SENTINAL,
	});
}

static void _dbg_cvr_print(cvrflags_t cvrs) {
	if (cvrs & cvr_extern) printf("extern ");
	if (cvrs & cvr_volatile) printf("volatile ");
	if (cvrs & cvr_const) printf("const ");
	if (cvrs & cvr_static) printf("static ");
	if (cvrs & cvr_restrict) printf("restrict ");
}
static void _dbg_ast_print(const ast_t *const ast, int idx, int tabs) {
#define TAB for (int i = 0; i < tabs; i++) printf("\t");
#define LENPRINT(lit, len) for (int _i = 0; _i < (len); _i++) printf("%c", (lit)[_i]);
	if (idx == AST_SENTINAL) return;
	switch (ast[idx].ty) {
	case ast_binary:
		TAB printf("node: %s\n", tokty_to_str(ast[idx].tok.ty));
		_dbg_ast_print(ast, ast[idx].info.binary.left, tabs+1);
		_dbg_ast_print(ast, ast[idx].info.binary.right, tabs+1);
		break;
	case ast_unary:
		TAB printf("node: %s\n", tokty_to_str(ast[idx].tok.ty));
		_dbg_ast_print(ast, ast[idx].info.inner, tabs+1);
		break;
	case ast_literal:
		TAB printf("node: %s\n", tokty_to_str(ast[idx].tok.ty));
		TAB dbg_typed_unit_print(ast[idx].info.val);
		break;
	case ast_ident:
		{
			TAB printf("node: %s\n", tokty_to_str(ast[idx].tok.ty));
			TAB LENPRINT(ast[idx].tok.lit, ast[idx].tok.len)
			printf("name: ");
			printf("\n");
		}
		break;
	case ast_decl:
		{
			TAB
			printf("decl ");
			const ast_t *node = ast + idx;
			_dbg_cvr_print(node->info.decl.cvrs);
			switch (node->info.decl.ty) {
			case decl_void: printf("void "); break;
			case decl_function:
				printf("function returns\n");
				_dbg_ast_print(ast, node->info.decl.inner, tabs+1);
				TAB printf("and takes\n");
				_dbg_ast_print(ast, node->info.decl.funcparams, tabs+1);
				break;
			case decl_pointer_to: printf("pointer to\n"); _dbg_ast_print(ast, node->info.decl.inner, tabs + 1); TAB break;
			case decl_array_of: 
				if (node->info.decl.arrlen != AST_SENTINAL) {
					printf("array ");
					_dbg_ast_print(ast, node->info.decl.inner, tabs+1);
					TAB printf("of ");
				} else {
					printf("array of \n");
					_dbg_ast_print(ast, node->info.decl.inner, tabs + 1); 
				}
				break;
			case decl_pod: printf("%s ", unitty_to_str(node->info.decl.pod)); break;
			case decl_struct:
				printf("struct ");
				if (node->info.decl.inner != AST_SENTINAL) {
					LENPRINT(node->info.decl.tyname.lit, node->info.decl.tyname.len)
					printf(" {\n");
					if (node->info.decl.inner != AST_SENTINAL) {
						_dbg_ast_print(ast, node->info.decl.inner, tabs+1);
					}
					TAB printf("} ");
				} else {
					LENPRINT(node->info.decl.tyname.lit, node->info.decl.tyname.len)
					printf(" ");
				}
				break;
			default: printf("undefined "); break;
			}
			LENPRINT(node->tok.lit, node->tok.len)
			printf("\n");
			break;
		}
	default: break;
	}
	_dbg_ast_print(ast, ast[idx].next, tabs);
#undef TAB
}
void dbg_ast_print(const ast_t *const astbuf, int root) {
	_dbg_ast_print(astbuf, root, 0);
}

