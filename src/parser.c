#include <string.h>

#include "lexer.h"
#include "parser.h"

#define PREC_FULL 16

struct state
{
	const char *src;
	struct ast_node *buffer;
	struct ast_error *errors;
	struct token token;
	size_t errsz, bufsz, currbuf, currerr;
};

typedef struct ast_node *(*expr_rule_unary_t)(struct state *, int);
typedef struct ast_node *(*expr_rule_binary_t)(struct state *,
						struct ast_node *,
						int);

struct expr_rule
{
	expr_rule_unary_t unary;
	expr_rule_binary_t binary;
	int binary_prec;
};


static struct ast_node *parse_literal(struct state *state, int prec);

static struct ast_node *parse_unary(struct state *state, int prec);

static struct ast_node *parse_binary(struct state *state,
					struct ast_node *left,
					int prec);

static const struct expr_rule expr_rules[TOK_MAX] = {
	// unary binary binary_prec
	{NULL, NULL, PREC_FULL+1}, // TOK_EOF
	{parse_literal, NULL, 0}, // TOK_IDENT
	{parse_literal, NULL, 0}, // TOK_LIT_STRING
	{parse_literal, NULL, 0}, // TOK_LIT_INTEGER
	{parse_literal, NULL, 0}, // TOK_LIT_DECIMAL
	{NULL, parse_binary, PREC_FULL+1}, // TOK_AS
	{NULL, NULL, PREC_FULL+1}, // TOK_FN
	{NULL, NULL, PREC_FULL+1}, // TOK_IF
	{NULL, NULL, PREC_FULL+1}, // TOK_ELSE
	{NULL, NULL, PREC_FULL+1}, // TOK_WHILE
	{NULL, NULL, PREC_FULL+1}, // TOK_LET
	{NULL, NULL, PREC_FULL+1}, // TOK_TYPEDEF
	{NULL, NULL, PREC_FULL+1}, // TOK_STRUCT
	{NULL, NULL, PREC_FULL+1}, // TOK_CONTINUE
	{NULL, NULL, PREC_FULL+1}, // TOK_BREAK
	{NULL, NULL, PREC_FULL+1}, // TOK_RETURN
	{NULL, NULL, PREC_FULL+1}, // TOK_CONST
	{NULL, NULL, PREC_FULL+1}, // TOK_EXTERN
	{parse_literal, NULL, 0}, // TOK_TYPE_INT
	{parse_literal, NULL, 0}, // TOK_TYPE_UINT
	{parse_literal, NULL, 0}, // TOK_TYPE_FLOAT
	{parse_literal, NULL, 0}, // TOK_TYPE_CHAR
	{parse_literal, NULL, 0}, // TOK_TYPE_VOID
	{NULL, parse_binary, 14}, // TOK_EQ
	{NULL, NULL, PREC_FULL+1}, // TOK_SEMICOL
	{NULL, NULL, PREC_FULL+1}, // TOK_COLON
	{NULL, parse_binary, 15}, // TOK_COMMA
	{NULL, parse_binary, 4}, // TOK_PLUS
	{NULL, parse_binary, 4}, // TOK_DASH
	{NULL, parse_binary, 3}, // TOK_STAR
	{NULL, parse_binary, 3}, // TOK_SLASH
	{NULL, parse_binary, 3}, // TOK_MODULO
	{NULL, parse_binary, 1}, // TOK_DOT
	{NULL, NULL, PREC_FULL+1}, // TOK_ARROW
	{NULL, parse_binary, 8}, // TOK_BIT_AND
	{NULL, parse_binary, 10}, // TOK_BIT_OR
	{NULL, parse_binary, 9}, // TOK_BIT_XOR
	{NULL, parse_binary, 5}, // TOK_LSHIFT
	{NULL, parse_binary, 5}, // TOK_RSHIFT
	{parse_unary, NULL, 2}, // TOK_NOT
	{parse_unary, NULL, 2}, // TOK_BIT_NOT
	{NULL, parse_binary, 7}, // TOK_NOTEQ
	{NULL, parse_binary, 7}, // TOK_EQEQ
	{NULL, parse_binary, 6}, // TOK_GT
	{NULL, parse_binary, 6}, // TOK_LT
	{NULL, parse_binary, 6}, // TOK_GE
	{NULL, parse_binary, 6}, // TOK_LE
	{NULL, parse_binary, 11}, // TOK_AND
	{NULL, parse_binary, 12}, // TOK_OR
	{parse_unary, NULL, 1}, // TOK_LPAREN
	{NULL, NULL, PREC_FULL+1}, // TOK_RPAREN
	{NULL, NULL, PREC_FULL+1}, // TOK_LBRACK
	{NULL, NULL, PREC_FULL+1}, // TOK_RBRACK
	{NULL, NULL, PREC_FULL+1}, // TOK_LBRACE
	{NULL, NULL, PREC_FULL+1} // TOK_RBRACE
};


static struct ast_node *parse_type(struct state *state);
static struct ast_node *parse_expr(struct state *state, int prec);
static struct ast_node *parse_stmt(struct state *state);

static void print_ast(struct ast_node *node, int tabs, int donext);


static struct ast_node *alloc_node(struct state *state, enum ast_type type)
{
	struct ast_node *node = NULL;
	if (state->currbuf < state->bufsz)
	{
		node = state->buffer + state->currbuf++;
		memset(node, 0, sizeof(*node));
		node->type = type;
	}
	return node;
}

static void add_error(struct state *state, int line, const char *msg)
{
	struct ast_error *err = NULL;
	if (state->currerr >= state->errsz) return;
	err = state->errors + state->currerr++;
	err->line = line;
	err->msg = msg;
}

#define EAT_TOKEN(TYPE)							\
	if (state->token.type != TYPE)					\
	{								\
		add_error(state, state->token.line, "expected "#TYPE);	\
		return NULL;						\
	}								\
	token_iter_next(&state->src, &state->token);

static struct ast_node *parse_literal(struct state *state, int prec)
{
	struct ast_node *node = alloc_node(state, AST_LITERAL);
	node->token = state->token;
	token_iter_next(&state->src, &state->token);
	return node;
}

static struct ast_node *parse_unary(struct state *state, int prec)
{
	struct ast_node *node = alloc_node(state, AST_UNARY);
	node->token = state->token;
	token_iter_next(&state->src, &state->token);
	if (node->token.type == TOK_LPAREN)
	{
		node->inner = parse_expr(state, PREC_FULL);
		EAT_TOKEN(TOK_RPAREN);
	}
	else
	{
		node->inner = parse_expr(state, prec - 1);
	}
	return node;
}

static struct ast_node *parse_binary(struct state *state,
					struct ast_node *left,
					int prec)
{
	struct ast_node *node = alloc_node(state, AST_BINARY);
	node->token = state->token;
	token_iter_next(&state->src, &state->token);
	node->alt1 = left;
	// This makes all of these into left-right precedence
	node->alt2 = parse_expr(state, prec - 1);
	return node;
}

static struct ast_node *parse_expr(struct state *state, int prec)
{
	const struct expr_rule *rule = expr_rules + state->token.type;
	struct ast_node *left;

	if (!rule->unary) return NULL;
	left = rule->unary(state, prec);

	rule = expr_rules + state->token.type;
	while (rule->binary && prec >= rule->binary_prec)
	{
		left = rule->binary(state, left, rule->binary_prec);
		rule = expr_rules + state->token.type;
	}

	return left;
}

static struct ast_node *parse_type_func(struct state *state)
{
	struct ast_node *func = NULL, **last = NULL, *curr = NULL;

	func = alloc_node(state, AST_FUNC_OF);
	last = &func->alt1;
	token_iter_next(&state->src, &state->token); // Get the lparen out
	while (state->token.type != TOK_RPAREN)
	{
		curr = parse_type(state);
		*last = curr;
		last = &curr->next;
		if (state->token.type == TOK_COMMA)
		{
			token_iter_next(&state->src, &state->token);
		}
	}
	token_iter_next(&state->src, &state->token);
	if (state->token.type == TOK_ARROW)
	{
		func->inner = parse_type(state);
	}
	else
	{
		func->inner = alloc_node(state, AST_BASE_TYPE);
		func->inner->token.type = TOK_TYPE_VOID;
	}

	return func;
}

static struct ast_node *parse_type(struct state *state)
{
	struct ast_node *root = NULL, **last = &root, *curr = NULL;
	while (1)
	{
		switch (state->token.type)
		{
		case TOK_CONST:
			curr = alloc_node(state, AST_CONST_OF);
			break;
		case TOK_EXTERN:
			token_iter_next(&state->src, &state->token);
			curr = alloc_node(state, AST_CPTR_OF);
			break;
		case TOK_STAR:
			curr = alloc_node(state, AST_PTR_OF);
			break;
		case TOK_BIT_AND:
			curr = alloc_node(state, AST_REF_OF);
			break;
		case TOK_LBRACK:
			// Start an array
			curr = alloc_node(state, AST_ARRAY_OF);
			token_iter_next(&state->src, &state->token);
			curr->inner = parse_type(state);
			if (state->token.type == TOK_SEMICOL)
			{
				token_iter_next(&state->src, &state->token);
				curr->alt1 = parse_expr(state, PREC_FULL);
			}
			break;
		case TOK_LPAREN:
			curr = parse_type_func(state);
			break;
		case TOK_TYPE_INT:
		case TOK_TYPE_UINT:
		case TOK_TYPE_CHAR:
		case TOK_TYPE_VOID:
		case TOK_TYPE_FLOAT:
			curr = alloc_node(state, AST_BASE_TYPE);
			curr->token = state->token;
			break;
		case TOK_IDENT:
			curr = alloc_node(state, AST_IDENT);
			curr->token = state->token;
			break;
		default:
			return root;
		}
		*last = curr;
		last = &curr->inner;
		token_iter_next(&state->src, &state->token);
	}
}

static struct ast_node *parse_let(struct state *state)
{
	struct ast_node *node = alloc_node(state, AST_LET);
	token_iter_next(&state->src, &state->token);
	node->token = state->token;
	EAT_TOKEN(TOK_IDENT);
	EAT_TOKEN(TOK_COLON);
	node->inner = parse_type(state);
	if (state->token.type == TOK_EQ)
	{
		token_iter_next(&state->src, &state->token);
		node->alt1 = parse_expr(state, PREC_FULL);
	}
	return node;
}

static struct ast_node *parse_typedef(struct state *state)
{
	struct ast_node *node = alloc_node(state, AST_TYPE_DEF);
	token_iter_next(&state->src, &state->token);
	node->token = state->token;
	EAT_TOKEN(TOK_IDENT);
	EAT_TOKEN(TOK_EQ);
	node->inner = parse_type(state);
	return node;
}

static struct ast_node *parse_struct(struct state *state)
{
	struct ast_node **last = NULL, *curr = NULL;
	struct ast_node *node = alloc_node(state, AST_STRUCT_DEF);

	token_iter_next(&state->src, &state->token);
	node->token = state->token;
	EAT_TOKEN(TOK_IDENT);
	EAT_TOKEN(TOK_LBRACE);
	
	last = &node->inner;
	while (state->token.type != TOK_RBRACE)
	{
		curr = alloc_node(state, AST_SCOPE_VAR);
		curr->token = state->token;
		token_iter_next(&state->src, &state->token);
		EAT_TOKEN(TOK_COLON);
		curr->inner = parse_type(state);
		if (state->token.type == TOK_COMMA)
		{
			token_iter_next(&state->src, &state->token);
		}

		*last = curr;
		last = &curr->next;
	}

	return node;
}

static struct ast_node *parse_stmt_compound(struct state *state)
{
	struct ast_node *node, **last, *curr;

	node = alloc_node(state, AST_COMPOUND);
	last = &node->inner;
	
	EAT_TOKEN(TOK_LBRACE);
	while (state->token.type != TOK_RBRACE)
	{
		curr = parse_stmt(state);

		*last = curr;
		last = &curr->next;
	}
	EAT_TOKEN(TOK_RBRACE);

	return node;
}

static struct ast_node *parse_stmt_if(struct state *state)
{
	struct ast_node *node;

	node = alloc_node(state, AST_IF);
	token_iter_next(&state->src, &state->token);
	node->inner = parse_expr(state, PREC_FULL);
	node->alt1 = parse_stmt_compound(state);

	if (state->token.type != TOK_ELSE) return node;
	token_iter_next(&state->src, &state->token);
	if (state->token.type == TOK_IF) node->alt2 = parse_stmt_if(state);
	else node->alt2 = parse_stmt_compound(state);

	return node;
}

static struct ast_node *parse_stmt_while(struct state *state)
{
	struct ast_node *node;

	node = alloc_node(state, AST_WHILE);
	node->inner = parse_expr(state, PREC_FULL);
	node->alt1 = parse_stmt_compound(state);

	return node;
}

static struct ast_node *parse_stmt(struct state *state)
{
	struct ast_node *node;

	switch (state->token.type)
	{
	case TOK_LBRACE: return parse_stmt_compound(state);
	case TOK_IF: return parse_stmt_if(state);
	case TOK_WHILE: return parse_stmt_while(state);
	case TOK_LET:
		node = parse_let(state);
		EAT_TOKEN(TOK_SEMICOL);
		return node;
	case TOK_RETURN:
		token_iter_next(&state->src, &state->token);
		node = alloc_node(state, AST_RETURN);
		node->inner = parse_expr(state, PREC_FULL);
		EAT_TOKEN(TOK_SEMICOL);
		return node;
	case TOK_BREAK:
		token_iter_next(&state->src, &state->token);
		node = alloc_node(state, AST_BREAK);
		EAT_TOKEN(TOK_SEMICOL);
		return node;
	case TOK_CONTINUE:
		token_iter_next(&state->src, &state->token);
		node = alloc_node(state, AST_CONTINUE);
		EAT_TOKEN(TOK_SEMICOL);
		return node;
	default:
		node = alloc_node(state, AST_EXPR);
		node->inner = parse_expr(state, PREC_FULL);
		EAT_TOKEN(TOK_SEMICOL);
		return node;
	}
}

static struct ast_node *parse_func(struct state *state)
{
	struct ast_node **last, *curr;
	struct ast_node *fn = alloc_node(state, AST_FUNC_DEF);
	token_iter_next(&state->src, &state->token);
	fn->token = state->token;
	EAT_TOKEN(TOK_IDENT);
	EAT_TOKEN(TOK_LPAREN);

	last = &fn->alt1;
	while (state->token.type != TOK_RPAREN)
	{
		curr = alloc_node(state, AST_SCOPE_VAR);
		curr->token = state->token;
		token_iter_next(&state->src, &state->token);
		EAT_TOKEN(TOK_COLON);
		curr->inner = parse_type(state);
		if (state->token.type == TOK_COMMA)
		{
			token_iter_next(&state->src, &state->token);
		}

		*last = curr;
		last = &curr->next;
	}

	EAT_TOKEN(TOK_RPAREN);
	if (state->token.type == TOK_ARROW)
	{
		token_iter_next(&state->src, &state->token);
		fn->alt2 = parse_type(state);
	}

	fn->inner = parse_stmt_compound(state);

	return fn;
}

struct ast_node *ast_construct(const char *src,
				struct ast_node *buffer,
				size_t bufsz,
				struct ast_error *err_buffer,
				size_t err_bufsz)
{
	struct state state =
	{
		src,
		buffer,
		err_buffer,
		{ 0 },
		err_bufsz,
		bufsz,
		0,
		0
	};
	struct ast_node *root = NULL, **last = &root, *curr = NULL;

	token_iter_init(&state.token);
	while (token_iter_next(&state.src, &state.token))
	{
		switch (state.token.type)
		{
		case TOK_LET:
			curr = parse_let(&state);
			if (state.token.type != TOK_SEMICOL)
			{
				add_error(&state, state.token.line, "expected ;");
				return root;
			}
			break;
		case TOK_STRUCT:
			curr = parse_struct(&state);
			break;
		case TOK_TYPEDEF:
			curr = parse_typedef(&state);
			if (state.token.type != TOK_SEMICOL)
			{
				add_error(&state, state.token.line, "expected ;");
				return root;
			}
			break;
		case TOK_FN:
			curr = parse_func(&state);
			break;
		default:
			//print_ast(root, 0, 1);
			return root;
		}

		*last = curr;
		last = &curr->next;
	}

	//print_ast(root, 0, 1);

	return root;
}


#if 0
#include <stdio.h>

static void print_tabs(int tabs)
{
	int i;
	for (i = 0; i < tabs; i++)
	{
		putchar(' ');
		putchar(' ');
	}
}
static void print_len(const char *str, int len)
{
	int i;
	for (i = 0; i < len; i++)
	{
		putchar(str[i]);
	}
}

static void print_ast(struct ast_node *node, int tabs, int donext)
{
	struct ast_node *curr;
	if (node == NULL) return;

	switch (node->type)
	{
	case AST_LITERAL:
		print_len(node->token.str, node->token.strlen);
		break;
	case AST_UNARY:
		print_len(node->token.str, node->token.strlen);
		print_ast(node->inner, tabs, 1);
		if (node->token.type == TOK_LPAREN) printf(")");
		break;
	case AST_BINARY:
		print_ast(node->alt1, tabs, 1);
		printf(" ");
		print_len(node->token.str, node->token.strlen);
		printf(" ");
		print_ast(node->alt2, tabs, 1);
		break;
	case AST_IDENT:
		print_len(node->token.str, node->token.strlen);
		printf(" ");
		break;
	case AST_CONST_OF:
		printf("const ");
		print_ast(node->inner, tabs, 1);
		break;
	case AST_REF_OF:
		printf("&");
		print_ast(node->inner, tabs, 1);
		break;
	case AST_CPTR_OF:
		printf("extern ");
	case AST_PTR_OF:
		printf("*");
		print_ast(node->inner, tabs, 1);
		break;
	case AST_ARRAY_OF:
		printf("[");
		print_ast(node->inner, tabs, 1);
		printf("]");
		break;
	case AST_BASE_TYPE:
		switch (node->token.type)
		{
		case TOK_TYPE_INT: printf("int "); break;
		case TOK_TYPE_UINT: printf("uint "); break;
		case TOK_TYPE_FLOAT: printf("float "); break;
		case TOK_TYPE_CHAR: printf("char "); break;
		case TOK_TYPE_VOID: printf("void "); break;
		default: printf("err "); break;
		}
		break;
	case AST_LET:
		print_tabs(tabs);
		printf("let ");
		print_len(node->token.str, node->token.strlen);
		printf(": ");
		print_ast(node->inner, tabs+1, 1);
		if (node->alt1)
		{
			printf(" = ");
			print_ast(node->alt1, tabs+1, 1);
		}
		printf(";\n");
		break;
	case AST_TYPE_DEF:
		print_tabs(tabs);
		printf("typedef ");
		print_len(node->token.str, node->token.strlen);
		printf(" = ");
		print_ast(node->inner, tabs+1, 1);
		printf("\n");
		break;
	case AST_STRUCT_DEF:
		print_tabs(tabs);
		printf("struct ");
		print_len(node->token.str, node->token.strlen);
		printf("\n");

		curr = node->inner;
		while (curr) {
			print_tabs(tabs + 1);
			print_ast(curr, tabs+1, 0);
			printf("\n");
			curr = curr->next;
		}
		break;
	case AST_FUNC_DEF:
		printf("fn ");
		print_len(node->token.str, node->token.strlen);
		printf("(");
		print_ast(node->alt1, tabs+1, 1);
		printf(")");
		if (node->alt2)
		{
			printf(" -> ");
			print_ast(node->alt2, tabs+1, 1);
		}
		printf("\n");
		print_ast(node->inner, tabs+1, 1);
		break;
	case AST_SCOPE_VAR:
		print_len(node->token.str, node->token.strlen);
		printf(": ");
		print_ast(node->inner, tabs+1, 1);
		printf(", ");
		break;
	case AST_IF:
		print_tabs(tabs);
		printf("if ");
		print_ast(node->inner, tabs+1, 1);
		printf("\n");
		print_ast(node->alt1, tabs+1, 1);
		if (!node->alt2) break;
		print_tabs(tabs);
		printf("else ");
		if (node->alt2->type != AST_IF) printf("\n");
		print_ast(node->alt2, tabs+1, 1);
		break;
	case AST_WHILE:
		print_tabs(tabs);
		printf("while ");
		print_ast(node->inner, tabs+1, 1);
		printf("\n");
		print_ast(node->alt1, tabs+1, 1);
		printf("\n");
		break;
	case AST_EXPR:
		print_ast(node->inner, tabs+1, 1);
		break;
	case AST_COMPOUND:
		print_tabs(tabs-1);
		printf("{\n");
		print_ast(node->inner, tabs, 1);
		print_tabs(tabs-1);
		printf("}\n");
		break;
	case AST_BREAK:
		print_tabs(tabs);
		printf("break;\n");
		break;
	case AST_CONTINUE:
		print_tabs(tabs);
		printf("continue;\n");
		break;
	case AST_RETURN:
		print_tabs(tabs);
		printf("return ");
		print_ast(node->inner, tabs+1, 1);
		printf(";\n");
		break;
	default:
		printf("unimplemented \n");
		break;
	}

	if (donext) print_ast(node->next, tabs, 1);
}
#endif

