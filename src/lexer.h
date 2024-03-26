#ifndef _lexer_h_
#define _lexer_h_

#include <stddef.h>

enum token_type
{
	// Special tokens
	TOK_EOF,
	TOK_IDENT,
	TOK_LIT_STRING,
	TOK_LIT_INTEGER,
	TOK_LIT_DECIMAL,

	// Keywords
	TOK_AS,
	TOK_FN,
	TOK_IF,
	TOK_ELSE,
	TOK_WHILE,
	TOK_LET,
	TOK_TYPEDEF,
	TOK_STRUCT,
	TOK_CONTINUE,
	TOK_BREAK,
	TOK_RETURN,
	TOK_CONST,

	// Operators
	TOK_EQ,
	TOK_SEMICOL,
	TOK_COLON,
	TOK_COMMA,
	TOK_PLUS,
	TOK_DASH,
	TOK_STAR,
	TOK_SLASH,
	TOK_DOT,
	TOK_ARROW,
	TOK_BIT_AND,
	TOK_BIT_OR,
	TOK_BIT_XOR,
	TOK_LSHIFT,
	TOK_RSHIFT,
	TOK_EQEQ,
	TOK_GT,
	TOK_LT,
	TOK_GE,
	TOK_LE,
	TOK_AND,
	TOK_OR,

	TOK_LPAREN,
	TOK_RPAREN,
	TOK_LBRACK,
	TOK_RBRACK,
	TOK_LBRACE,
	TOK_RBRACE,
};

struct token
{
	enum token_type type;
	const char *str;
	size_t strlen;
	int line;
};

void token_iter_init(struct token *token);
int token_iter_next(const char **src, struct token *iter);

#endif

