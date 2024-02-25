//int a = b = c = d == 5;
//int f(void), *fip(), (*pfi)(), *ap[3] = {0, .r = { [5] = 6 }};
typedef struct parser parser_t;
typedef struct tok tok_t;
typedef struct ast ast_t;
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
