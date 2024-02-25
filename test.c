//int a = b = c = d == 5;
//int f(void), *fip(), (*pfi)(), *ap[3] = {0, .r = { [5] = 6 }};
typedef struct parser parser_t;
typedef struct tok tok_t;
typedef struct ast ast_t;
typedef struct err err_t;
typedef struct typed_unit typed_unit_t;
typedef unsigned long long int64_t;
typedef union unit unit_t;
static int parse_string_lit(parser_t *const state) {
	tok_t tok = state->lexer.curr;
	lexer_next(&state->lexer);
	return parser_add(state, (ast_t){
		.ty = ast_literal,
		.tok = tok,
		.info.literal = {
			.ty = lit_str,
			.val.str = tok,
		},
		.next = AST_SENTINAL,
	});
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
//void test() {
//	int a = 4;
//	a += 5;
//}
//struct foo f(void);
//int main(int argc, char **argv) {
//
//}
//  a = (b = (c = d));
//int isz = sizeof(int);
//int a = sizeof(((float){ .a[2]->ptr = 5 }++).a[2]);

//(1 ? "t" : ("f" ? "5" : "6"))
