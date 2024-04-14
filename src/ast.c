#include "ast.h"


ast_t *ast_build(ast_state_t *state) {
    return NULL;
}


#define X(ENUM) case ENUM: return #ENUM;
static const char *ast_type_to_str(ast_type_t type) {
    switch (type) {
    AST_TYPES
    default: return "[null ast]";
    }
}
#undef X

static void print_tabs(FILE *out, int32_t tabs) {
    for (int32_t i = 0; i < tabs; i++) fprintf(out, "  ");
}

static void ast_log_recurrsive(const ast_t *ast, FILE *out, int32_t tabs) {
    if (!ast) {
        fprintf(out, "[null]");
        return;
    }
 
    fprintf(out, "%s(token: ", ast_type_to_str(ast->type));
    lex_token_log(&ast->token, out);
    if (!ast->a && !ast->b && !ast->child && !ast->next) {
        fprintf(out, ")");
        return;
    }
    fprintf(out, "){\n");
    if (ast->child) {
        print_tabs(out, tabs);
        fprintf(out, "child: ");
        ast_log_recurrsive(ast->child, out, tabs + 1);
        fprintf(out, ",\n");
    }
    if (ast->a) {
        print_tabs(out, tabs);
        fprintf(out, "a: ");
        ast_log_recurrsive(ast->a, out, tabs + 1);
        fprintf(out, ",\n");
    }
    if (ast->b) {
        print_tabs(out, tabs);
        fprintf(out, "b: ");
        ast_log_recurrsive(ast->b, out, tabs + 1);
        fprintf(out, ",\n");
    }
    print_tabs(out, tabs);
    fprintf(out, "}%s", ast->next ? ",\n" : "");
    ast_log_recurrsive(ast, out, tabs);
}

void ast_log(const ast_t *ast, FILE *out) {
    ast_log_recurrsive(ast, out, 1);
}

void ast_error_log(const ast_error_t *err, FILE *out) {
    fprintf(out, "ast_error_t{\n");
    fprintf(out, "    msg: \"%s\",\n", err->msg);
    fprintf(out, "    line: %d,\n", err->line);
    fprintf(out, "    chr: %d,\n", err->chr);
    fprintf(out, "    is_lex_error: %s,\n", err->is_lex_error ? "true" : "false");
    fprintf(out, "}\n");
}

