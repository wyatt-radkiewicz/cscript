#ifndef _ast_h_
#define _ast_h_

#include "common.h"
#include "lexer.h"

typedef struct ast ast_t;

#define AST_TYPES \
    X(ast_var) \
    X(ast_type_pod) \
     \
    X(ast_stmt_group) \
    X(ast_stmt_let) \
    X(ast_stmt_expr) \
    X(ast_stmt_return) \
     \
    X(ast_op_binary) \
    X(ast_op_unary) \
    X(ast_op_call) \
    X(ast_literal) \
     \
    X(ast_def_func)

#define X(ENUM) ENUM,
typedef enum ast_type {
    AST_TYPES
} ast_type_t;
#undef X

struct ast {
    ast_type_t type;
    lex_token_t token;
    ast_t *child, *next;
    ast_t *a, *b;
};

typedef struct ast_error {
    char msg[512];

    bool is_lex_error;
    int32_t line, chr;
} ast_error_t;

typedef struct ast_state {
    lex_state_t lexer;

    ast_t *buf;
    size_t nused, buflen;

    ast_error_t *errbuf;
    size_t nerrs, errbuflen;
} ast_state_t;

// Returns number of errors encountered while parsing
ast_t *ast_build(ast_state_t *state);

#include <stdio.h>
void ast_log(const ast_t *ast, FILE *out);
void ast_error_log(const ast_error_t *err, FILE *out);

#endif

