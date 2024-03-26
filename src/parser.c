#include <string.h>

#include "lexer.h"
#include "parser.h"

struct state
{
	const char *src;
	struct ast_node *buffer;
	struct ast_error *errors;
	struct token token;
	size_t errsz, bufsz, currbuf, currerr;
};


static struct ast_node *parse_type(struct state *state);

static void print_ast(struct ast_node *node, int tabs);


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
	token_iter_next(&state->src, &state->token);			\
	if (state->token.type != TYPE)					\
	{								\
		add_error(state, state->token.line, "expected "#TYPE);	\
		return NULL;						\
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
		case TOK_BIT_AND:
			curr = alloc_node(state, AST_REF_OF);
			break;
		case TOK_LBRACK:
			// Start an array
			curr = alloc_node(state, AST_ARRAY_OF);
			token_iter_next(&state->src, &state->token);
			curr->alt1 = parse_type(state);
			curr->alt2 = NULL; // TODO: do parse_expr() here
			if (state->token.type == TOK_SEMICOL)
			{
				while (state->token.type != TOK_RBRACK)
				{
					if (state->token.type == TOK_EOF) break;
					token_iter_next(&state->src, &state->token);
				}
			}
			break;
		case TOK_LPAREN:
			curr = parse_type_func(state);
			break;
		case TOK_TYPE_INT:
		case TOK_TYPE_UINT:
		case TOK_TYPE_STR:
		case TOK_TYPE_VOID:
		case TOK_TYPE_FLOAT:
			curr = alloc_node(state, AST_BASE_TYPE);
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
	EAT_TOKEN(TOK_IDENT);
	node->token = state->token;
	EAT_TOKEN(TOK_COLON);
	token_iter_next(&state->src, &state->token);
	node->inner = parse_type(state);
	return node;
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
	while (token_iter_next(&src, &state.token))
	{
		switch (state.token.type)
		{
		case TOK_LET:
			curr = parse_let(&state);
			break;
		default:
			print_ast(root, 0);
			return root;
		}

		*last = curr;
		last = &curr->next;
	}

	print_ast(root, 0);

	return root;
}


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

static void print_ast(struct ast_node *node, int tabs)
{
	if (node == NULL) return;

	switch (node->type)
	{
	case AST_CONST_OF:
		printf("const ");
		print_ast(node->inner, tabs);
		break;
	case AST_BASE_TYPE:
		switch (node->token.type)
		{
		case TOK_TYPE_INT: printf("int "); break;
		case TOK_TYPE_UINT: printf("uint "); break;
		case TOK_TYPE_FLOAT: printf("float "); break;
		case TOK_TYPE_STR: printf("str "); break;
		case TOK_TYPE_VOID: printf("void "); break;
		default: printf("ERR "); break;
		}
		break;
	case AST_LET:
		print_tabs(tabs);
		printf("LET ");
		print_len(node->token.str, node->token.strlen);
		printf(": ");
		print_ast(node->inner, tabs+1);
		printf("\n");
		break;
	default:
		printf("unimplemented \n");
		break;
	}

	print_ast(node->next, tabs);
}

