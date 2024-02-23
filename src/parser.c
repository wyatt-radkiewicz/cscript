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

static void parse_decl_data(
	parser_t *const state,
	cvrflags_t *const cvrs,
	lvldata_t *const data
) {
	*data = (lvldata_t){
		.ty = dataty_pod,
		.pod = unit_i32,
	};
	*cvrs = cvr_none;

	int bits = -1, toks = 0;
	int u = 0, f = 0, l = 0;
	while (true) {
		switch (state->lexer.curr.ty) {
		case tok_const: *cvrs |= cvr_const; break;
		case tok_static: *cvrs |= cvr_static; break;
		case tok_extern: *cvrs |= cvr_extern; break;
		case tok_volatile: *cvrs |= cvr_volatile; break;
		case tok_restrict: *cvrs |= cvr_restrict; break;
		case tok_char:
			if (bits == -1) bits = 8;
			else parser_err(state, (err_t){ .ty = err_unexpected, .msg = "char", .line = state->lexer.curr.line, .chr = state->lexer.curr.chr });
			break;
		case tok_short:
			if (bits == -1) bits = 16;
			else parser_err(state, (err_t){ .ty = err_unexpected, .msg = "short", .line = state->lexer.curr.line, .chr = state->lexer.curr.chr });
			break;
		case tok_int:
			if (bits == -1) bits = 32;
			else parser_err(state, (err_t){ .ty = err_unexpected, .msg = "int", .line = state->lexer.curr.line, .chr = state->lexer.curr.chr });
			break;
		case tok_long:
			if (bits == -1 || l == 1) {
				l++;
				bits = 64;
			} else {
				parser_err(state, (err_t){ .ty = err_unexpected, .msg = "long", .line = state->lexer.curr.line, .chr = state->lexer.curr.chr });
			}
			break;
		case tok_unsigned:
			if (u == 0) u++;
			else parser_err(state, (err_t){ .ty = err_unexpected, .msg = "unsigned", .line = state->lexer.curr.line, .chr = state->lexer.curr.chr });
			break;
		case tok_float:
			if (f == 0 && bits == -1) {
				bits = 32;
				f++;
			} else {
				parser_err(state, (err_t){ .ty = err_unexpected, .msg = "unsigned", .line = state->lexer.curr.line, .chr = state->lexer.curr.chr });
			}
			break;
		case tok_double:
			if (f == 0 && bits == -1) {
				bits = 64;
				f++;
			} else {
				parser_err(state, (err_t){ .ty = err_unexpected, .msg = "unsigned", .line = state->lexer.curr.line, .chr = state->lexer.curr.chr });
			}
		case tok_struct:
			if (bits != -1 || l != 0 || f != 0 || u != 0) {
				parser_err(state, (err_t){ .ty = err_unexpected, .msg = "identifier", .line = state->lexer.curr.line, .chr = state->lexer.curr.chr });
				return;
			}
			data->ty = dataty_struct;
			lexer_next(&state->lexer);
			data->tyname = state->lexer.curr;
			lexer_next(&state->lexer);
			return;
		case tok_enum:
			if (bits != -1 || l != 0 || f != 0 || u != 0) {
				parser_err(state, (err_t){ .ty = err_unexpected, .msg = "identifier", .line = state->lexer.curr.line, .chr = state->lexer.curr.chr });
				return;
			}
			data->ty = dataty_enum;
			lexer_next(&state->lexer);
			data->tyname = state->lexer.curr;
			lexer_next(&state->lexer);
			return;
		case tok_union:
			if (bits != -1 || l != 0 || f != 0 || u != 0) {
				parser_err(state, (err_t){ .ty = err_unexpected, .msg = "identifier", .line = state->lexer.curr.line, .chr = state->lexer.curr.chr });
				return;
			}
			data->ty = dataty_union;
			lexer_next(&state->lexer);
			data->tyname = state->lexer.curr;
			lexer_next(&state->lexer);
			return;
		case tok_void:
			data->ty = dataty_void;
			lexer_next(&state->lexer);
			return;
		default: // typedef
			if (toks) goto set_pod;
			if (bits != -1 || l != 0 || f != 0 || u != 0) {
				parser_err(state, (err_t){ .ty = err_unexpected, .msg = "identifier", .line = state->lexer.curr.line, .chr = state->lexer.curr.chr });
				return;
			}
			data->ty = dataty_typedef;
			data->tyname = state->lexer.curr;
			lexer_next(&state->lexer);
			return;
		}

		toks++;
		lexer_next(&state->lexer);
	}

set_pod:
	switch (bits) {
	case 8: data->pod = u ? unit_u8 : unit_i8; break;
	case 16: data->pod = u ? unit_u16 : unit_i16; break;
	case 32: data->pod = f ? unit_f32 : (u ? unit_u32 : unit_i32); break;
	case 64: data->pod = f ? unit_f64 : (u ? unit_u64 : unit_i64); break;
	default: break;
	}
}
static void _parse_decl(parser_t *const state, metadecl_t *const meta, size_t *const ndecls, decl_t *decl) {
	cvrflags_t final_cvrs;
	lvldata_t final_data;
	parse_decl_data(state, &final_cvrs, &final_data);

	tok_t buf[32];
	int top = -1;
	while (state->lexer.curr.ty != tok_ident) {
		if (final_data.ty == dataty_void && state->lexer.curr.ty == tok_rparen) {
			// this is a void parameter
			*decl = (decl_t){
				.lvls = {
					[0] = (declvl_t){
						.ty = lvlty_data,
						.cvr = final_cvrs,
						.data = final_data,
					},
				},
				.nlvls = 1,
				.ident = (tok_t){ .ty = tok_undefined },
			};
			return;
		}
		buf[++top] = state->lexer.curr;
		lexer_next(&state->lexer);
	}
	tok_t ident = state->lexer.curr;
	lexer_next(&state->lexer);

	*decl = (decl_t){
		.nlvls = 0,
		.ident = ident,
	};

	int *last_func_ret = NULL;
	while (true) {
		bool parened = false, isfunc = false;
		// Arrays and functions
		while (state->lexer.curr.ty == tok_lparen
			|| state->lexer.curr.ty == tok_lbrack) {
			lexer_next(&state->lexer);
			if (state->lexer.prev[2].ty == tok_lbrack) {
				decl->lvls[decl->nlvls].ty = lvlty_array;

				cvrflags_t cvrs = cvr_none;
				while (state->lexer.curr.ty != tok_rbrack) {
					if (state->lexer.curr.ty == tok_semicol || state->lexer.curr.ty == tok_eof) {
						parser_err(state, (err_t){ .ty = err_expected, .msg = "array things", .line = state->lexer.curr.line, .chr = state->lexer.curr.chr });
					}

					if (state->lexer.curr.ty == tok_const) cvrs |= cvr_const;
					if (state->lexer.curr.ty == tok_static) cvrs |= cvr_static;
					if (state->lexer.curr.ty == tok_volatile) cvrs |= cvr_volatile;
					if (state->lexer.curr.ty == tok_restrict) cvrs |= cvr_restrict;

					if (state->lexer.curr.ty == tok_integer) {
						decl->lvls[decl->nlvls].array_len = parse_number(state).unit.u64;
					} else {
						lexer_next(&state->lexer);
					}
				}
				lexer_next(&state->lexer);
				decl->lvls[decl->nlvls].cvr = cvrs;
				decl->nlvls++;
			} else {
				decl->lvls[decl->nlvls].ty = lvlty_func;
				isfunc = true;
			}
		}
		if (!isfunc && state->lexer.curr.ty == tok_rparen) {
			parened = true;
		}
	
		// Return for function
		decl_t *ret = NULL;
		if (isfunc) {
			ret = meta->decls + (*ndecls)++;
			*ret = (decl_t){
				.nlvls = 0,
				.ident = (tok_t){ .ty = tok_undefined, .lit = "", .len = 0, .line = decl->ident.line, .chr = decl->ident.chr },
			};
		}

		// Pointers
		decl_t *const ptr_decl = isfunc ? ret : decl;
		cvrflags_t cvrs = cvr_none;
		while (top >= 0) {
			if (buf[top].ty == tok_lparen) {
				if (parened) top--;
				break;
			}
			if (buf[top].ty == tok_const) cvrs |= cvr_const;
			if (buf[top].ty == tok_volatile) cvrs |= cvr_volatile;
			if (buf[top].ty == tok_restrict) cvrs |= cvr_restrict;
			if (buf[top].ty == tok_star) {
				ptr_decl->lvls[ptr_decl->nlvls].cvr = cvrs;
				ptr_decl->lvls[ptr_decl->nlvls].ty = lvlty_pointer;
				cvrs = cvr_none;
				ptr_decl->nlvls++;
			}
			top--;
		}
		
		if ((state->lexer.peek[0].ty == tok_lparen || state->lexer.peek[0].ty == tok_lbrack) && parened) {
			lexer_next(&state->lexer);
			continue;
		}
		if (top < 0 && !isfunc) break;

		// Parse parameters
		decl->lvls[decl->nlvls].func.ret = ret - meta->decls;
		last_func_ret = &decl->lvls[decl->nlvls].func.ret;
		decl->lvls[decl->nlvls].func.nparams = 0;
		while (state->lexer.curr.ty != tok_rparen) {
			const size_t mi = (*ndecls)++;
			_parse_decl(state, meta, ndecls, meta->decls + mi);
			decl->lvls[decl->nlvls].func.params[decl->lvls[decl->nlvls].func.nparams++] = mi;

			if (state->lexer.curr.ty == tok_comma) lexer_next(&state->lexer);
			else break;
		}
		decl->nlvls++;
		decl = ret;
		parser_eat(state, tok_rparen, "\")\"");
	}

	// Add 1 more
	if (last_func_ret) {
		meta->decls[*last_func_ret].lvls[meta->decls[*last_func_ret].nlvls++] = (declvl_t){
			.ty = lvlty_data,
			.cvr = final_cvrs,
			.data = final_data,
		};
	} else {
		decl->lvls[decl->nlvls++] = (declvl_t){
			.ty = lvlty_data,
			.cvr = final_cvrs,
			.data = final_data,
		};
	}
}
static metadecl_t parse_decl(parser_t *const state) {
	metadecl_t meta;
	size_t ndecls = 1;
	_parse_decl(state, &meta, &ndecls, &meta.root);
	return meta;
}
static int parse_decl_ast(parser_t *const state) {
	const metadecl_t decl = parse_decl(state);
	return parser_add(state, (ast_t){
		.ty = ast_decl,
		.tok = decl.root.ident,
		.info.decl = decl,
		.next = AST_SENTINAL,
	});
}
static int parse_struct(parser_t *const state) {
	parser_eat(state, tok_struct, "struct keyword");
	ast_t *const struct_ast = state->buf + parser_add(state, (ast_t){
		.ty = ast_struct,
		.tok = parser_eat(state, tok_ident, "identifier"),
		.next = AST_SENTINAL,
	});
	parser_eat(state, tok_lbrace, "\"{\"");
	int *last = &struct_ast->info.struct_members;

	while (state->lexer.curr.ty != tok_rbrace) {
		const int val = parse_decl_ast(state);
		parser_eat(state, tok_semicol, "\";\"");
		*last = val;
		last = &state->buf[val].next;
	}

	parser_eat(state, tok_rbrace, "\"}\"");
	parser_eat(state, tok_semicol, "\";\"");

	return state->buf - struct_ast;
}
static int parse_union(parser_t *const state) {
	lexer_next(&state->lexer);
	return AST_SENTINAL;
}
static int parse_enum(parser_t *const state) {
	lexer_next(&state->lexer);
	return AST_SENTINAL;
}
static int parse_typedef(parser_t *const state) {
	lexer_next(&state->lexer);
	return AST_SENTINAL;
}
static int parse_function(parser_t *const state) {
	lexer_next(&state->lexer);
	return AST_SENTINAL;
}

void parse_toplevels(parser_t *const state) {
	int *last = &state->root;
	while (state->lexer.curr.ty != tok_eof) {
		int node = AST_SENTINAL;
		switch (state->lexer.curr.ty) {
		case tok_union:
			node = parse_union(state);
			break;
		case tok_enum:
			node = parse_enum(state);
			break;
		case tok_struct:
			node = parse_struct(state);
			break;
		case tok_typedef:
			node = parse_typedef(state);
			break;
		case tok_const: case tok_static: case tok_extern: case tok_volatile:
		case tok_int: case tok_unsigned: case tok_signed:
		case tok_char: case tok_short: case tok_long:
		case tok_float: case tok_double:
			node = parse_function(state);
			break;
		default:
			parser_err(state, (err_t){ .ty = err_unexpected, .msg = "not top level", .line = state->lexer.curr.line, .chr = state->lexer.curr.chr });
			break;
		}
		if (last) *last = node;
		last = node == AST_SENTINAL ? NULL : &state->buf[node].next;
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

static void _dbg_decl_print(const decl_t *const decl, const metadecl_t *const meta) {
#define LENPRINT(lit, len) for (int _i = 0; _i < (len); _i++) printf("%c", (lit)[_i]);
	for (int i = 0; i < decl->nlvls; i++) {
		if (decl->lvls[i].cvr & cvr_const) printf("const ");
		if (decl->lvls[i].cvr & cvr_extern) printf("extern ");
		if (decl->lvls[i].cvr & cvr_static) printf("static ");
		if (decl->lvls[i].cvr & cvr_restrict) printf("restrict ");
		if (decl->lvls[i].cvr & cvr_volatile) printf("volatile ");

		switch (decl->lvls[i].ty) {
		case lvlty_data:
			switch (decl->lvls[i].data.ty) {
			case dataty_pod:
				printf("%s ", unitty_to_str(decl->lvls[i].data.pod));
				break;
			case dataty_struct:
				printf("struct ");
				LENPRINT(decl->lvls[i].data.tyname.lit, decl->lvls[i].data.tyname.len)
				printf(" ");
				break;
			case dataty_typedef:
				LENPRINT(decl->lvls[i].data.tyname.lit, decl->lvls[i].data.tyname.len)
				printf(" ");
				break;
			case dataty_union:
				printf("union ");
				LENPRINT(decl->lvls[i].data.tyname.lit, decl->lvls[i].data.tyname.len)
				printf(" ");
				break;
			case dataty_enum:
				printf("enum ");
				LENPRINT(decl->lvls[i].data.tyname.lit, decl->lvls[i].data.tyname.len)
				printf(" ");
				break;
			case dataty_void:
				printf("void ");
				break;
			default:
				printf("unknown ");
				break;
			}
			break;
		case lvlty_pointer:
			printf("pointer to ");
			break;
		case lvlty_array:
			if (decl->lvls[i].array_len) printf("array of %zu ", decl->lvls[i].array_len);
			else printf("array of ");
			break;
		case lvlty_func:
			printf("function returning ");
			_dbg_decl_print(&meta->decls[decl->lvls[i].func.ret], meta);
			printf("which takes (");
			for (int j = 0; j < decl->lvls[i].func.nparams; j++) {
				_dbg_decl_print(&meta->decls[decl->lvls[i].func.params[j]], meta);
				if (j + 1 < decl->lvls[i].func.nparams) printf(", ");
			}
			printf(") ");
			break;
		default:
			printf("unimplemented ");
			break;
		}
	}
	LENPRINT(decl->ident.lit, decl->ident.len)
}

static void _dbg_ast_print(const ast_t *const ast, int idx, int tabs) {
	if (idx == AST_SENTINAL) return;
#define TAB for (int i = 0; i < tabs; i++) printf("\t");
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
	case ast_struct:
		TAB printf("struct ");
		LENPRINT(ast[idx].tok.lit, ast[idx].tok.len)
		printf("\n");
		_dbg_ast_print(ast, ast[idx].info.struct_members, tabs + 1);
		break;
	case ast_decl:
		TAB _dbg_decl_print(&ast[idx].info.decl.root, &ast[idx].info.decl);
		printf("\n");
		break;
	default: break;
	}
	_dbg_ast_print(ast, ast[idx].next, tabs);
#undef TAB
}
void dbg_ast_print(const ast_t *const astbuf, int root) {
	_dbg_ast_print(astbuf, root, 0);
}

