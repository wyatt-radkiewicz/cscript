#ifndef _ast_h_
#define _ast_h_

#include "common.h"
#include "lexer.h"

typedef struct ast ast_t;

#define AST_TYPES \
    X(ast_var) \
    X(ast_namedexpr) \
    X(ast_type_pod) \
    X(ast_type_array) \
    X(ast_type_ident) \
    X(ast_type_extern) \
    X(ast_type_static) \
    X(ast_type_const) \
    X(ast_type_ptr) \
    X(ast_type_ref) \
    X(ast_type_pfn) \
     \
    X(ast_stmt_group) \
    X(ast_stmt_let) \
    X(ast_stmt_expr) \
    X(ast_stmt_return) \
    X(ast_stmt_void) \
     \
    X(ast_op_ternary) \
    X(ast_op_binary) \
    X(ast_op_unary) \
    X(ast_op_call) \
    X(ast_literal) \
    X(ast_ident) \
    X(ast_init_pod) \
    X(ast_init_array) \
    X(ast_init_struct) \
     \
    X(ast_def_func) \
    X(ast_def_typedef) \
    X(ast_def_struct)

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

typedef struct ast_state {
    lex_state_t lexer;
    lex_token_t lasttok;

    ast_t *buf;
    size_t nused, buflen;

    error_t *errbuf;
    size_t nerrs, errbuflen;
} ast_state_t;

// Returns number of errors encountered while parsing
ast_t *ast_build(ast_state_t *state);

#include <stdio.h>
void ast_log(const ast_t *ast, FILE *out);

#endif

