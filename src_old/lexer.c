#include <ctype.h>
#include <string.h>

#include "lexer.h"

static void eat_line(const char **src, struct token *iter)
{
	while (**src != '\0' && **src != '\n')
	{
		(*src)++;
	}
	iter->line++;
}

static void eat_whitespace(const char **src, struct token *iter)
{
	while (isspace(**src) || **src == '/')
	{
		if (**src == '\n')
		{
			iter->line++;
		}
		if (**src == '/')
		{
			if (*(*src + 1) == '/')
			{
				eat_line(src, iter);
				continue;
			}
			else
			{
				return;
			}
		}
		(*src)++;
	}
}

static void do_string(const char **src, struct token *iter)
{
	iter->strlen = 0;
	while (**src != '"')
	{
		if (**src == '\0')
		{
			iter->type = TOK_EOF;
			iter->strlen = 1;
			return;
		}
		iter->strlen++;
		(*src)++;
	}
	iter->type = TOK_LIT_STRING;
	(*src)++;
}

static void do_number(char first, const char **src, struct token *iter)
{
	iter->type = TOK_LIT_INTEGER;
	while (isdigit(**src) || **src == '.')
	{
		if (**src == '.')
		{
			if (iter->type == TOK_LIT_DECIMAL)
			{
				iter->type = TOK_EOF;
				return;
			}
			iter->type = TOK_LIT_DECIMAL;
		}
		iter->strlen++;
		(*src)++;
	}
}

#define DO_KWRD(FIRST, KWRD) (first == FIRST && strcmp(iter->str, KWRD))

static void do_ident(char first, const char **src, struct token *iter)
{
	while (isalnum(**src) || **src == '_')
	{
		iter->strlen++;
		(*src)++;
	}

	iter->type = TOK_IDENT;
	// Keywords
	switch (iter->strlen)
	{
	case 2:
		if (DO_KWRD('a', "as")) iter->type = TOK_AS;
		if (DO_KWRD('f', "fn")) iter->type = TOK_FN;
		if (DO_KWRD('i', "if")) iter->type = TOK_IF;
		break;
	case 3:
		if (DO_KWRD('l', "let")) iter->type = TOK_LET;
		if (DO_KWRD('i', "int")) iter->type = TOK_TYPE_INT;
		break;
	case 4:
		if (DO_KWRD('e', "else")) iter->type = TOK_ELSE;
		if (DO_KWRD('u', "uint")) iter->type = TOK_TYPE_UINT;
		if (DO_KWRD('v', "void")) iter->type = TOK_TYPE_VOID;
		if (DO_KWRD('c', "char")) iter->type = TOK_TYPE_CHAR;
		break;
	case 5:
		if (DO_KWRD('w', "while")) iter->type = TOK_WHILE;
		if (DO_KWRD('b', "break")) iter->type = TOK_BREAK;
		if (DO_KWRD('c', "const")) iter->type = TOK_CONST;
		if (DO_KWRD('f', "float")) iter->type = TOK_TYPE_FLOAT;
		break;
	case 6:
		if (DO_KWRD('s', "struct")) iter->type = TOK_STRUCT;
		if (DO_KWRD('r', "return")) iter->type = TOK_RETURN;
		if (DO_KWRD('e', "extern")) iter->type = TOK_EXTERN;
		break;
	case 7:
		if (DO_KWRD('t', "typedef")) iter->type = TOK_TYPEDEF;
		break;
	case 8:
		if (DO_KWRD('c', "continue")) iter->type = TOK_CONTINUE;
		break;
	}

}

void token_iter_init(struct token *token)
{
	token->str = NULL;
	token->type = TOK_DOT;
	token->strlen = 0;
	token->line = 1;
}

#define DO_DOUBLETOK(CHR2, TY2, TY1)		\
	if (**src == CHR2)			\
	{					\
		(*src)++;			\
		iter->strlen++;			\
		iter->type = TY2;		\
	}					\
	else					\
	{					\
		iter->type = TY1;		\
	}					\
	break;
#define DO_2CHRTOK(CHR2, TY)			\
	if (**src == CHR2)			\
	{					\
		(*src)++;			\
		iter->strlen++;			\
		iter->type = TY;		\
		break;				\
	}
#define DO_TRIPLETOK(CHR1, CHR2, TY2, TY1, TY3)	\
	if (**src == CHR1)			\
	{					\
		(*src)++;			\
		iter->strlen++;			\
		iter->type = TY1;		\
	}					\
	else if (**src == CHR2)			\
	{					\
		(*src)++;			\
		iter->strlen++;			\
		iter->type = TY2;		\
	}					\
	else					\
	{					\
		iter->type = TY3;		\
	}					\
	break;

int token_iter_next(const char **src, struct token *iter)
{
	char c;

	if (iter->type == TOK_EOF) return 0;

	eat_whitespace(src, iter);

	iter->type = TOK_EOF;
	iter->str = *src;
	iter->strlen = 1;
	c = *(*src)++;

	switch (c)
	{
	// Operators
	case ';': iter->type = TOK_SEMICOL; break;
	case ':': iter->type = TOK_COLON; break;
	case '=': DO_DOUBLETOK('=', TOK_EQEQ, TOK_EQ)
	case '-': DO_DOUBLETOK('>', TOK_ARROW, TOK_DASH)
	case '+': iter->type = TOK_PLUS; break;
	case '*': iter->type = TOK_STAR; break;
	case '/': iter->type = TOK_SLASH; break;
	case '%': iter->type = TOK_MODULO; break;
	case '.': iter->type = TOK_DOT; break;
	case '^': iter->type = TOK_BIT_XOR; break;
	case '~': iter->type = TOK_BIT_NOT; break;
	case '!': DO_DOUBLETOK('=', TOK_NOTEQ, TOK_NOT)
	case '&': DO_DOUBLETOK('&', TOK_AND, TOK_BIT_AND)
	case '|': DO_DOUBLETOK('|', TOK_OR, TOK_BIT_OR)
	case '>': DO_TRIPLETOK('>', '=', TOK_RSHIFT, TOK_GE, TOK_GT)
	case '<': DO_TRIPLETOK('<', '=', TOK_LSHIFT, TOK_LE, TOK_LT)
	case ',': iter->type = TOK_COMMA; break;

	case '(': iter->type = TOK_LPAREN; break;
	case ')': iter->type = TOK_RPAREN; break;
	case '[': iter->type = TOK_LBRACK; break;
	case ']': iter->type = TOK_RBRACK; break;
	case '{': iter->type = TOK_LBRACE; break;
	case '}': iter->type = TOK_RBRACE; break;

	// Special tokens
	case '"':
		do_string(src, iter);
		break;
	default:
		if (isdigit(c))
		{
			do_number(c, src, iter);
		}
		else if (isalpha(c) || c == '_')
		{
			do_ident(c, src, iter);
		}
		break;
	}

	return iter->type;
}

