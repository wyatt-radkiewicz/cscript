#include <stdarg.h>
#include <string.h>

#include "ast.h"

typedef enum parse_prec {
    prec_none,
    prec_zero,
    prec_postfix,
    prec_prefix,
    prec_as,
    prec_mulpli,
    prec_addit,
    prec_shift,
    prec_band,
    prec_bxor,
    prec_bor,
    prec_relation,
    prec_equality,
    prec_land,
    prec_lor,
    prec_ternary,
    prec_assign,
    prec_comma,
    prec_full
} parse_prec_t;

typedef ast_t *(*parse_prefix_t)(ast_state_t *state, parse_prec_t prec);
typedef ast_t *(*parse_infix_t)(ast_state_t *state, ast_t *left, parse_prec_t prec);

typedef struct parser_rule {
    parse_prefix_t prefix;
    parse_infix_t infix;

    // Precedence level of infix operation
    parse_prec_t prec;
} parser_rule_t;

static ast_t *parse_pod(ast_state_t *state, parse_prec_t prec);
static ast_t *parse_literal(ast_state_t *state, parse_prec_t prec);
static ast_t *parse_ident(ast_state_t *state, parse_prec_t prec);
static ast_t *parse_prefix(ast_state_t *state, parse_prec_t prec);
static ast_t *parse_postfix(ast_state_t *state, ast_t *left, parse_prec_t prec);
static ast_t *parse_infix(ast_state_t *state, ast_t *left, parse_prec_t prec);
static ast_t *parse_expr(ast_state_t *state, parse_prec_t prec);

static const parser_rule_t parser_rules[tok_max+1] = {
    [tok_i8]            = { .prefix = parse_pod, .infix = NULL, .prec = prec_zero },
    [tok_i16]           = { .prefix = parse_pod, .infix = NULL, .prec = prec_zero },
    [tok_i32]           = { .prefix = parse_pod, .infix = NULL, .prec = prec_zero },
    [tok_i64]           = { .prefix = parse_pod, .infix = NULL, .prec = prec_zero },
    [tok_u8]            = { .prefix = parse_pod, .infix = NULL, .prec = prec_zero },
    [tok_u16]           = { .prefix = parse_pod, .infix = NULL, .prec = prec_zero },
    [tok_u32]           = { .prefix = parse_pod, .infix = NULL, .prec = prec_zero },
    [tok_u64]           = { .prefix = parse_pod, .infix = NULL, .prec = prec_zero },
    [tok_f32]           = { .prefix = parse_pod, .infix = NULL, .prec = prec_zero },
    [tok_f64]           = { .prefix = parse_pod, .infix = NULL, .prec = prec_zero },
    [tok_c8]            = { .prefix = parse_pod, .infix = NULL, .prec = prec_zero },
    [tok_c16]           = { .prefix = parse_pod, .infix = NULL, .prec = prec_zero },
    [tok_c32]           = { .prefix = parse_pod, .infix = NULL, .prec = prec_zero },
    [tok_b8]            = { .prefix = parse_pod, .infix = NULL, .prec = prec_zero },
    [tok_u0]            = { .prefix = parse_pod, .infix = NULL, .prec = prec_zero },

    [tok_ident]         = { .prefix = parse_ident, .infix = NULL, .prec = prec_zero },
    [tok_literal_u]     = { .prefix = parse_literal, .infix = NULL, .prec = prec_zero },
    [tok_literal_f]     = { .prefix = parse_literal, .infix = NULL, .prec = prec_zero },
    [tok_literal_c]     = { .prefix = parse_literal, .infix = NULL, .prec = prec_zero },
    [tok_literal_str]   = { .prefix = parse_literal, .infix = NULL, .prec = prec_zero },
    [tok_as]            = { .prefix = NULL, .infix = parse_infix, .prec = prec_as },

    [tok_plusplus]      = { .prefix = parse_prefix, .infix = parse_postfix, .prec = prec_postfix },
    [tok_minusminus]    = { .prefix = parse_prefix, .infix = parse_postfix, .prec = prec_postfix },
    [tok_lparen]        = { .prefix = parse_prefix, .infix = parse_postfix, .prec = prec_postfix },
    [tok_lbrack]        = { .prefix = NULL, .infix = parse_postfix, .prec = prec_postfix },
    [tok_dot]           = { .prefix = NULL, .infix = parse_infix, .prec = prec_postfix },
    [tok_plus]          = { .prefix = NULL, .infix = parse_infix, .prec = prec_addit },
    [tok_minus]         = { .prefix = NULL, .infix = parse_infix, .prec = prec_addit },
    [tok_not]           = { .prefix = parse_prefix, .infix = NULL, .prec = prec_prefix },
    [tok_bnot]          = { .prefix = parse_prefix, .infix = NULL, .prec = prec_prefix },
    [tok_star]          = { .prefix = parse_prefix, .infix = parse_infix, .prec = prec_mulpli },
    [tok_slash]         = { .prefix = NULL, .infix = parse_infix, .prec = prec_mulpli },
    [tok_modulo]        = { .prefix = NULL, .infix = parse_infix, .prec = prec_mulpli },
    [tok_lshift]        = { .prefix = NULL, .infix = parse_infix, .prec = prec_shift },
    [tok_rshift]        = { .prefix = NULL, .infix = parse_infix, .prec = prec_shift },
    [tok_less]          = { .prefix = NULL, .infix = parse_infix, .prec = prec_relation },
    [tok_lesseq]        = { .prefix = NULL, .infix = parse_infix, .prec = prec_relation },
    [tok_greater]       = { .prefix = NULL, .infix = parse_infix, .prec = prec_relation },
    [tok_greatereq]     = { .prefix = NULL, .infix = parse_infix, .prec = prec_relation },
    [tok_eqeq]          = { .prefix = NULL, .infix = parse_infix, .prec = prec_equality },
    [tok_noteq]         = { .prefix = NULL, .infix = parse_infix, .prec = prec_equality },
    [tok_band]          = { .prefix = parse_prefix, .infix = parse_infix, .prec = prec_band },
    [tok_bxor]          = { .prefix = NULL, .infix = parse_infix, .prec = prec_bxor },
    [tok_bor]           = { .prefix = NULL, .infix = parse_infix, .prec = prec_bor },
    [tok_and]           = { .prefix = NULL, .infix = parse_infix, .prec = prec_land },
    [tok_or]            = { .prefix = NULL, .infix = parse_infix, .prec = prec_lor },
    [tok_eq]            = { .prefix = NULL, .infix = parse_infix, .prec = prec_assign },
    [tok_pluseq]        = { .prefix = NULL, .infix = parse_infix, .prec = prec_assign },
    [tok_minuseq]       = { .prefix = NULL, .infix = parse_infix, .prec = prec_assign },
    [tok_timeseq]       = { .prefix = NULL, .infix = parse_infix, .prec = prec_assign },
    [tok_diveq]         = { .prefix = NULL, .infix = parse_infix, .prec = prec_assign },
    [tok_modeq]         = { .prefix = NULL, .infix = parse_infix, .prec = prec_assign },
    [tok_lshifteq]      = { .prefix = NULL, .infix = parse_infix, .prec = prec_assign },
    [tok_rshifteq]      = { .prefix = NULL, .infix = parse_infix, .prec = prec_assign },
    [tok_bandeq]        = { .prefix = NULL, .infix = parse_infix, .prec = prec_assign },
    [tok_bxoreq]        = { .prefix = NULL, .infix = parse_infix, .prec = prec_assign },
    [tok_boreq]         = { .prefix = NULL, .infix = parse_infix, .prec = prec_assign },
    [tok_comma]         = { .prefix = NULL, .infix = parse_infix, .prec = prec_comma },
};

static bool ast_state_error(ast_state_t *state, bool lexer, const char *msg, ...) {
    const bool toomany = state->nerrs >= state->errbuflen;
    ast_error_t *err = toomany
        ? state->errbuf + state->errbuflen - 1
        : state->errbuf + state->nerrs++;
    if (!toomany) {
        *err = (ast_error_t){
            .line = state->lasttok.line,
            .chr = state->lasttok.chr,
            .is_lex_error = lexer,
        };
    } else {
        strcpy(err->msg, "Aborting! Too many errors!");
        return false;
    }

    va_list args;
    va_start(args, msg);
    vsnprintf(err->msg, arrsz(err->msg), msg, args);
    va_end(args);
    
    return true;
}

static bool ast_next_token(ast_state_t *state) {
    state->lasttok = state->lexer.tok;
    if (lex_state_next(&state->lexer).okay) return true;
    ast_state_error(state, true, "Invalid token");
    return false;
}

#define X(ENUM) case ENUM: return #ENUM;
static const char *tok_to_str(lex_token_type_t type) {
    switch (type) {
    LEX_TOKENS
    default: return "tok_undefined";
    }
}
#undef X

static bool ast_eat_token(ast_state_t *state, lex_token_type_t type) {
    if (state->lexer.tok.type != type) {
        ast_state_error(state, false, "Expected token %s", tok_to_str(type));
        return false;
    }
    if (!ast_next_token(state)) return false;
    return true;
}

static bool ast_error_eof(ast_state_t *state) {
    if (state->lexer.tok.type != tok_eof && state->lexer.tok.type != tok_undefined) return true;
    ast_state_error(state, false, "Unexpected end of file or unknown token");
    return false;
}

static ast_t *ast_alloc(ast_state_t *state, ast_type_t type) {
    if (state->nused == state->buflen) {
        ast_state_error(state, false, "Out of ast nodes!");
        return NULL;
    }

    ast_t *node = state->buf + state->nused++;
    *node = (ast_t){
        .token = state->lexer.tok,
        .type = type,
    };
    return node;
}

// Parses a pod type creation
static ast_t *parse_pod(ast_state_t *state, parse_prec_t prec) {
    return NULL;
}
static ast_t *parse_literal(ast_state_t *state, parse_prec_t prec) {
    ast_t *node = ast_alloc(state, ast_literal);
    if (!node) return NULL;
    if (!ast_next_token(state)) return NULL;
    return node;
}
// TODO: Parse struct initialization
static ast_t *parse_ident(ast_state_t *state, parse_prec_t prec) {
    ast_t *node = ast_alloc(state, ast_ident);
    if (!node) return NULL;
    if (!ast_next_token(state)) return NULL;
    return node;
}
static ast_t *parse_prefix(ast_state_t *state, parse_prec_t prec) {
    ast_t *node = ast_alloc(state, ast_op_unary);
    if (!node) return NULL;
    if (!ast_next_token(state)) return NULL;
    node->child = parse_expr(state, prec);
    if (!node->child) {
        ast_state_error(state, false, "Expected expression after prefix operand");
        return NULL;
    }
    return node;
}
static ast_t *parse_postfix(ast_state_t *state, ast_t *left, parse_prec_t prec) {
    ast_t *node = ast_alloc(state, ast_op_unary);
    if (!node) return NULL;
    if (!ast_next_token(state)) return NULL;
    node->child = left;
    return node;
}
static ast_t *parse_infix(ast_state_t *state, ast_t *left, parse_prec_t prec) {
    ast_t *node = ast_alloc(state, ast_op_binary);
    if (!node) return NULL;
    if (!ast_next_token(state)) return NULL;
    node->a = left;
    if (prec != prec_assign) prec--;
    if (!(node->b = parse_expr(state, prec))) {
        ast_state_error(state, false, "Expected expression after infix operand");
        return NULL;
    }
    return node;
}

static ast_t *parse_expr(ast_state_t *state, parse_prec_t prec) {
    const parser_rule_t *rule = &parser_rules[state->lexer.tok.type];
    if (!rule->prefix) return NULL;
    ast_t *eval = rule->prefix(state, prec);
    if (!eval) return NULL;

    rule = &parser_rules[state->lexer.tok.type];
    while (rule->infix && prec >= rule->prec) {
        if (!(eval = rule->infix(state, eval, rule->prec))) return NULL;
        rule = &parser_rules[state->lexer.tok.type];
    }

    return eval;
}

static ast_t *parse_type(ast_state_t *state) {
    return NULL;
}

static ast_t *parse_stmt(ast_state_t *state);
static ast_t *parse_stmt_let(ast_state_t *state) {
    return NULL;
}
static ast_t *parse_stmt_group(ast_state_t *state) {
    ast_t *node = ast_alloc(state, ast_stmt_group);
    if (!node) return NULL;

    ast_t **last = &node->child, *curr = NULL;
    if (!ast_eat_token(state, tok_lbrace)) return NULL;
    while (state->lexer.tok.type != tok_rbrace) {
        if (!ast_error_eof(state)) return NULL;
        curr = parse_stmt(state);
        if (!curr) return NULL;
        *last = curr;
        last = &curr->next;
    }
    if (!ast_next_token(state)) return NULL;

    return node;
}
static ast_t *parse_stmt(ast_state_t *state) {
    ast_t *node = NULL;

    bool expect_semicol = true;
    switch (state->lexer.tok.type) {
    case tok_semicol: break;
    case tok_return:
        if (!(node = ast_alloc(state, ast_stmt_return))) return NULL;
        node->child = parse_expr(state, prec_full);
        break;
    case tok_let:
        if (!(node = parse_stmt_let(state))) return NULL;
        break;
    case tok_lbrace:
        if (!(node = parse_stmt_group(state))) return NULL;
        expect_semicol = false;
        break;
    default: {
        if (!(node = ast_alloc(state, ast_stmt_expr))) return NULL;
        if (!(node->child = parse_expr(state, prec_full))) {
            ast_state_error(state, false, "Expected statement");
            return NULL;
        }
        } break;
    }

    if (expect_semicol && !ast_eat_token(state, tok_semicol)) return NULL;
    return node;
}
static ast_t *parse_def_func(ast_state_t *state) {
    return NULL;
}


static ast_t *parse_top_level(ast_state_t *state) {
    switch (state->lexer.tok.type) {
    case tok_let: return parse_stmt_let(state);
    case tok_fn: return parse_def_func(state);
    default: return NULL;
    }
}

ast_t *ast_build(ast_state_t *state) {
    ast_t *root = NULL, **last, *curr;

    if (!ast_next_token(state)) return root;
    return parse_expr(state, prec_full);

    last = &root;
    if (!ast_next_token(state)) return root;
    while (state->lexer.tok.type != tok_eof) {
        curr = parse_top_level(state);
        *last = curr;
        last = &curr->next;
        if (!curr || !ast_next_token(state)) break;
    }

    return root;
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
        print_tabs(out, tabs + 1);
        fprintf(out, "child: ");
        ast_log_recurrsive(ast->child, out, tabs + 1);
        fprintf(out, ",\n");
    }
    if (ast->a) {
        print_tabs(out, tabs + 1);
        fprintf(out, "a: ");
        ast_log_recurrsive(ast->a, out, tabs + 1);
        fprintf(out, ",\n");
    }
    if (ast->b) {
        print_tabs(out, tabs + 1);
        fprintf(out, "b: ");
        ast_log_recurrsive(ast->b, out, tabs + 1);
        fprintf(out, ",\n");
    }
    print_tabs(out, tabs);
    fprintf(out, "}%s", ast->next ? ",\n" : "");
    if (ast->next) ast_log_recurrsive(ast->next, out, tabs);
}

void ast_log(const ast_t *ast, FILE *out) {
    ast_log_recurrsive(ast, out, 0);
}

void ast_error_log(const ast_error_t *err, FILE *out) {
    fprintf(out, "ast_error_t{\n");
    fprintf(out, "    msg: \"%s\",\n", err->msg);
    fprintf(out, "    line: %d,\n", err->line);
    fprintf(out, "    chr: %d,\n", err->chr);
    fprintf(out, "    is_lex_error: %s,\n", err->is_lex_error ? "true" : "false");
    fprintf(out, "}\n");
}

