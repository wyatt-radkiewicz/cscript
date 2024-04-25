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
    prec_range,
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

static ast_t *parse_type(ast_state_t *state);
static ast_t *parse_init_array(ast_state_t *state, parse_prec_t prec);
static ast_t *parse_init_pod(ast_state_t *state, parse_prec_t prec);
static ast_t *parse_literal(ast_state_t *state, parse_prec_t prec);
static ast_t *parse_ident(ast_state_t *state, parse_prec_t prec);
static ast_t *parse_prefix(ast_state_t *state, parse_prec_t prec);
static ast_t *parse_postfix(ast_state_t *state, ast_t *left, parse_prec_t prec);
static ast_t *parse_infix(ast_state_t *state, ast_t *left, parse_prec_t prec);
static ast_t *parse_expr(ast_state_t *state, parse_prec_t prec);

static const parser_rule_t parser_rules[tok_max+1] = {
    [tok_i8]            = { .prefix = parse_init_pod, .infix = NULL, .prec = prec_zero },
    [tok_i16]           = { .prefix = parse_init_pod, .infix = NULL, .prec = prec_zero },
    [tok_i32]           = { .prefix = parse_init_pod, .infix = NULL, .prec = prec_zero },
    [tok_i64]           = { .prefix = parse_init_pod, .infix = NULL, .prec = prec_zero },
    [tok_u8]            = { .prefix = parse_init_pod, .infix = NULL, .prec = prec_zero },
    [tok_u16]           = { .prefix = parse_init_pod, .infix = NULL, .prec = prec_zero },
    [tok_u32]           = { .prefix = parse_init_pod, .infix = NULL, .prec = prec_zero },
    [tok_u64]           = { .prefix = parse_init_pod, .infix = NULL, .prec = prec_zero },
    [tok_f32]           = { .prefix = parse_init_pod, .infix = NULL, .prec = prec_zero },
    [tok_f64]           = { .prefix = parse_init_pod, .infix = NULL, .prec = prec_zero },
    [tok_c8]            = { .prefix = parse_init_pod, .infix = NULL, .prec = prec_zero },
    [tok_c16]           = { .prefix = parse_init_pod, .infix = NULL, .prec = prec_zero },
    [tok_c32]           = { .prefix = parse_init_pod, .infix = NULL, .prec = prec_zero },
    [tok_b8]            = { .prefix = parse_init_pod, .infix = NULL, .prec = prec_zero },
    [tok_u0]            = { .prefix = parse_init_pod, .infix = NULL, .prec = prec_zero },

    [tok_ident]         = { .prefix = parse_ident, .infix = NULL, .prec = prec_zero },
    [tok_literal_u]     = { .prefix = parse_literal, .infix = NULL, .prec = prec_zero },
    [tok_literal_f]     = { .prefix = parse_literal, .infix = NULL, .prec = prec_zero },
    [tok_literal_c]     = { .prefix = parse_literal, .infix = NULL, .prec = prec_zero },
    [tok_literal_str]   = { .prefix = parse_literal, .infix = NULL, .prec = prec_zero },
    [tok_true]          = { .prefix = parse_literal, .infix = NULL, .prec = prec_zero },
    [tok_false]         = { .prefix = parse_literal, .infix = NULL, .prec = prec_zero },
    [tok_as]            = { .prefix = NULL, .infix = parse_infix, .prec = prec_as },

    [tok_plusplus]      = { .prefix = parse_prefix, .infix = parse_postfix, .prec = prec_postfix },
    [tok_minusminus]    = { .prefix = parse_prefix, .infix = parse_postfix, .prec = prec_postfix },
    [tok_lparen]        = { .prefix = parse_prefix, .infix = parse_postfix, .prec = prec_postfix },
    [tok_lbrack]        = { .prefix = parse_init_array, .infix = parse_postfix, .prec = prec_postfix },
    [tok_dot]           = { .prefix = NULL, .infix = parse_infix, .prec = prec_postfix },
    [tok_dotdot]        = { .prefix = NULL, .infix = parse_infix, .prec = prec_range },
    [tok_arrow]         = { .prefix = NULL, .infix = parse_infix, .prec = prec_postfix },
    [tok_plus]          = { .prefix = NULL, .infix = parse_infix, .prec = prec_addit },
    [tok_minus]         = { .prefix = parse_prefix, .infix = parse_infix, .prec = prec_addit },
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
    error_t *err = toomany
        ? state->errbuf + state->errbuflen - 1
        : state->errbuf + state->nerrs++;
    if (!toomany) {
        *err = (error_t){
            .line = state->lasttok.line,
            .chr = state->lasttok.chr,
            .category = lexer,
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

#define X(ENUM) case ENUM: return #ENUM;
static const char *tok_to_str(lex_token_type_t type) {
    switch (type) {
    LEX_TOKENS
    default: return "tok_undefined";
    }
}
#undef X

static bool ast_error_eof(ast_state_t *state) {
    if (state->lexer.tok.type != tok_eof && state->lexer.tok.type != tok_undefined) return true;
    ast_state_error(state, false, "Unexpected end of file or unknown token");
    return false;
}

static bool ast_next_token(ast_state_t *state, bool noeof) {
    state->lasttok = state->lexer.tok;
    if (lex_state_next(&state->lexer).okay) {
        return !noeof || ast_error_eof(state);
    }
    ast_state_error(state, true, "Invalid token");
    return false;
}

static bool ast_eat_token(ast_state_t *state, bool noeof, lex_token_type_t type) {
    if (state->lexer.tok.type != type) {
        ast_state_error(state, false, "Expected token %s", tok_to_str(type));
        return false;
    }
    if (!ast_next_token(state, noeof)) return false;
    return true;
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

static ast_t *parse_init_pod(ast_state_t *state, parse_prec_t prec) {
    ast_t *node = ast_alloc(state, ast_init_pod);
    if (!ast_next_token(state, true)) return NULL;
    if (!ast_eat_token(state, true, tok_lbrace)) return NULL;
    node->child = parse_expr(state, prec_full);
    if (!ast_eat_token(state, true, tok_rbrace)) return NULL;
    return node;
}
static ast_t *parse_literal(ast_state_t *state, parse_prec_t prec) {
    ast_t *node = ast_alloc(state, ast_literal);
    if (!ast_next_token(state, false)) return NULL;
    if (!node) return NULL;
    return node;
}
static ast_t *parse_init_array(ast_state_t *state, parse_prec_t prec) {
    ast_t *node = ast_alloc(state, ast_init_array);
    if (!node) return NULL;
    if (!ast_eat_token(state, true, tok_lbrack)) return NULL;
    ast_t **last = &node->child, *curr = NULL;
    while (state->lexer.tok.type != tok_rbrack) {
        if (!(curr = parse_expr(state, prec_assign))) break;
        *last = curr;
        last = &curr->next;
        if (state->lexer.tok.type == tok_comma && !ast_next_token(state, true)) return NULL;
    }
    if (!ast_eat_token(state, false, tok_rbrack)) return NULL;
    return node;
}
static ast_t *parse_var(ast_state_t *state) {
    ast_t *node = ast_alloc(state, ast_var);
    if (!node) return NULL;
    if (!ast_eat_token(state, true, tok_ident)) return NULL;
    if (!ast_eat_token(state, true, tok_colon)) return NULL;
    if (!(node->child = parse_type(state))) return NULL;
    return node;
}
static ast_t *parse_ident(ast_state_t *state, parse_prec_t prec) {
    ast_t *node = ast_alloc(state, ast_ident);
    if (!ast_next_token(state, false)) return NULL;
    if (!node) return NULL;
    if (state->lexer.tok.type == tok_lbrace) {
        node->type = ast_init_struct;
        if (!ast_next_token(state, true)) return NULL;
        ast_t **last = &node->child, *curr = NULL;
        while (state->lexer.tok.type != tok_rbrace) {
            if (!(curr = ast_alloc(state, ast_namedexpr))) return NULL;
            if (!ast_eat_token(state, true, tok_ident)) return NULL;
            if (!ast_eat_token(state, true, tok_colon)) return NULL;
            if (!(curr->child = parse_expr(state, prec_ternary))) return NULL;
            
            if (state->lexer.tok.type == tok_comma
                && !ast_next_token(state, true)) return NULL;
            *last = curr;
            last = &curr->next;
        }
        if (!ast_eat_token(state, true, tok_rbrace)) return NULL;
    }
    return node;
}
static ast_t *parse_prefix(ast_state_t *state, parse_prec_t prec) {
    ast_t *node = NULL;
    if (state->lexer.tok.type != tok_lparen) {
        if (!(node = ast_alloc(state, ast_op_unary))) return NULL;
        if (!ast_next_token(state, false)) return NULL;
        node->child = parse_expr(state, prec);
        if (!node->child) {
            ast_state_error(state, false, "Expected expression after prefix operand");
            return NULL;
        }
    } else {
        if (!ast_next_token(state, false)) return NULL;
        if (!(node = parse_expr(state, prec))) {
            ast_state_error(state, false, "Expected expression inside parenthesis");
            return NULL;
        }
        if (!ast_eat_token(state, true, tok_rparen)) return NULL;
    }
    return node;
}
static ast_t *parse_postfix(ast_state_t *state, ast_t *left, parse_prec_t prec) {
    ast_t *node = ast_alloc(state, ast_op_postfix);
    if (!ast_next_token(state, false)) return NULL;
    if (!node) return NULL;
    if (node->token.type == tok_lbrack) {
        node->type = ast_op_binary;
        node->a = left;
        if (!(node->b = parse_expr(state, prec_ternary))) return NULL;
        if (!ast_eat_token(state, true, tok_rbrack)) return NULL;
    } else if (node->token.type == tok_lparen) {
        node->type = ast_op_call;
        node->a = left;
        
        ast_t **last = &node->b, *curr = NULL;
        while (state->lexer.tok.type != tok_rparen) {
            if (!(curr = parse_expr(state, prec_ternary))) return NULL;
            if (state->lexer.tok.type == tok_comma
                && !ast_eat_token(state, true, tok_comma)) return NULL;
            *last = curr;
            last = &curr->next;
        }
        if (!ast_eat_token(state, true, tok_rparen)) return NULL;
    } else {
        node->child = left;
    }
    return node;
}
static ast_t *parse_infix(ast_state_t *state, ast_t *left, parse_prec_t prec) {
    ast_t *node = ast_alloc(state, ast_op_binary);
    if (!ast_next_token(state, false)) return NULL;
    if (!node) return NULL;
    node->a = left;
    if (node->token.type == tok_as) {
        if (!(node->b = parse_type(state))) return NULL;
    } else if (!(node->b = parse_expr(state, prec == prec_assign ? prec - 1 : prec))) {
        ast_state_error(state, false, "Expected expression after infix operand");
        return NULL;
    }
    if ((node->token.type == tok_dot || node->token.type == tok_arrow)
        && node->b->type != ast_ident) {
        ast_state_error(state, false, "Expected identifier after struct access");
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

    // Ternary expressions
    if (state->lexer.tok.type == tok_qmark && prec >= prec_ternary) {
        ast_t *node = ast_alloc(state, ast_op_ternary);
        if (!node) return NULL;
        node->child = eval;
        if (!ast_next_token(state, true)) return NULL;
        if (!(node->a = parse_expr(state, prec_full))) return NULL;
        if (!ast_eat_token(state, true, tok_colon)) return NULL;
        if (!(node->b = parse_expr(state, prec_full))) return NULL;
        return node;
    }

    return eval;
}

static ast_t *parse_pfn(ast_state_t *state) {
    ast_t *node = ast_alloc(state, ast_type_pfn), **last;
    if (!ast_next_token(state, true)) return NULL;
    if (!node) return NULL;
    last = &node->a;
    ast_t *curr = NULL;
    while (state->lexer.tok.type != tok_rparen) {
        if (state->lexer.tok.type == tok_ident
            && state->lexer.next.type == tok_colon) {
            if (!ast_next_token(state, true)) return NULL;
            if (!ast_next_token(state, true)) return NULL;
        }
        if (!(curr = parse_type(state))) return NULL;
        *last = curr;
        last = &curr->next;
        if (state->lexer.tok.type == tok_comma
            && !ast_next_token(state, true)) return NULL;
    }
    if (!ast_eat_token(state, true, tok_rparen)) return NULL;
    if (state->lexer.tok.type != tok_arrow) {
        if (!(node->b = ast_alloc(state, ast_type_pod))) return NULL;
        node->b->token = (lex_token_t){
            .type = tok_u0,
            .line = state->lexer.tok.line,
            .chr = state->lexer.tok.chr,
        };
    } else {
        if (!ast_next_token(state, true)) return NULL;
        if (!(node->b = parse_type(state))) return NULL;
    }
    return node;
}
static ast_t *parse_type(ast_state_t *state) {
    ast_t *root = NULL, **last = &root;
    while (true) {
        ast_t *child = NULL;
        switch (state->lexer.tok.type) {
        case tok_const:
            if (!ast_next_token(state, true)) return NULL;
            if (!(child = ast_alloc(state, ast_type_const))) return NULL;
            break;
        case tok_extern:
            if (!ast_next_token(state, true)) return NULL;
            if (!(child = ast_alloc(state, ast_type_extern))) return NULL;
            break;
        case tok_static:
            if (!ast_next_token(state, true)) return NULL;
            if (!(child = ast_alloc(state, ast_type_static))) return NULL;
            break;
        case tok_star:
            if (!ast_next_token(state, true)) return NULL;
            if (!(child = ast_alloc(state, ast_type_ptr))) return NULL;
            break;
        case tok_band:
            if (!ast_next_token(state, true)) return NULL;
            if (!(child = ast_alloc(state, ast_type_ref))) return NULL;
            break;
        default: goto skip_attribs;
        }
        *last = child;
        last = &child->child;
    }
skip_attribs:
    
    if (state->lexer.tok.type == tok_lparen) {
        if (!(*last = parse_pfn(state))) return NULL;
        return root;
    }

    if (state->lexer.tok.type == tok_lbrack) {
        ast_t *child = ast_alloc(state, ast_type_array);
        if (!ast_next_token(state, true)) return NULL;
        if (!child) return NULL;
        *last = child;
        if (!(child->a = parse_type(state))) return NULL;
        if (state->lexer.tok.type == tok_semicol) {
            if (!ast_next_token(state, true)) return NULL;
            if (!(child->b = parse_expr(state, prec_ternary))) return NULL;
        }
        if (!ast_eat_token(state, false, tok_rbrack)) return NULL;
        return root;
    }

    if (state->lexer.tok.type != tok_ident
        && state->lexer.tok.type < tok_types_start
        && state->lexer.tok.type > tok_types_last) {
        ast_state_error(state, false, "Expected type");
        return NULL;
    }
    ast_t *child = ast_alloc(state, state->lexer.tok.type == tok_ident ? ast_type_ident : ast_type_pod);
    if (!ast_next_token(state, false)) return NULL;
    if (!child) return NULL;
    *last = child;
    return root;
}

static ast_t *parse_stmt(ast_state_t *state);
static ast_t *parse_stmt_group(ast_state_t *state);
static ast_t *parse_stmt_if(ast_state_t *state) {
    ast_t *node = ast_alloc(state, ast_stmt_if);
    if (!ast_eat_token(state, true, tok_if)) return NULL;
    if (!node) return NULL;
    if (!(node->child = parse_expr(state, prec_comma))) return NULL;
    if (!(node->a = parse_stmt_group(state))) return NULL;
    if (state->lexer.tok.type != tok_else) return node;
    if (!ast_next_token(state, true)) return NULL;
    lex_token_type_t ty = state->lexer.tok.type;
    if (ty == tok_if
        && !(node->b = parse_stmt_if(state))) return NULL;
    else if (ty != tok_if
            && !(node->b = parse_stmt_group(state))) return NULL;
    return node;
}
static ast_t *parse_stmt_while(ast_state_t *state) {
    ast_t *node = ast_alloc(state, ast_stmt_while);
    if (!ast_eat_token(state, true, tok_while)) return NULL;
    if (!node) return NULL;
    if (!(node->a = parse_expr(state, prec_comma))) return NULL;
    if (!(node->b = parse_stmt_group(state))) return NULL;
    return node;
}
static ast_t *parse_stmt_for(ast_state_t *state) {
    ast_t *node = ast_alloc(state, ast_stmt_for);
    if (!ast_eat_token(state, true, tok_for)) return NULL;
    if (!node) return NULL;
    if (state->lexer.next.type == tok_colon) {
        if (!(node->a = parse_var(state))) return NULL;
    } else {
        if (!(node->a = ast_alloc(state, ast_ident))) return NULL;
        if (!ast_eat_token(state, true, tok_ident)) return NULL;
    }
    if (!ast_eat_token(state, true, tok_in)) return NULL;
    if (!(node->b = parse_expr(state, prec_range))) return NULL;
    if (!(node->child = parse_stmt_group(state))) return NULL;
    return node;
}
static ast_t *parse_let(ast_state_t *state) {
    if (!ast_eat_token(state, true, tok_let)) return NULL;
    ast_t *node = ast_alloc(state, ast_stmt_let);
    if (!node) return NULL;
    if (!ast_next_token(state, true)) return NULL;
    if (state->lexer.tok.type == tok_colon) {
        if (!ast_next_token(state, true)) return NULL;
        if (!(node->a = parse_type(state))) return NULL;
    }
    if (state->lexer.tok.type == tok_eq) {
        if (!ast_next_token(state, true)) return NULL;
        if (!(node->b = parse_expr(state, prec_ternary))) {
            ast_state_error(state, false, "Expected expression");
            return NULL;
        }
    } else if (!node->a) {
        ast_state_error(state, false, "Can not infer type of variable when it is not initialized!");
    }
    return node;
}
static ast_t *parse_stmt_group(ast_state_t *state) {
    ast_t *node = ast_alloc(state, ast_stmt_group);
    if (!node) return NULL;

    ast_t **last = &node->child, *curr = NULL;
    if (!ast_eat_token(state, true, tok_lbrace)) return NULL;
    while (state->lexer.tok.type != tok_rbrace) {
        const int32_t line = state->lexer.line, chr = state->lexer.chr;
        if (!ast_error_eof(state)) return NULL;
        if (!(curr = parse_stmt(state))) {
            if (line == state->lexer.line && chr == state->lexer.chr) {
                return NULL;
            }
            continue;
        }
        *last = curr;
        last = &curr->next;
    }
    if (!ast_next_token(state, false)) return NULL;

    return node;
}
static ast_t *parse_stmt(ast_state_t *state) {
    ast_t *node = NULL;

    switch (state->lexer.tok.type) {
    case tok_semicol:
        if (!(node = ast_alloc(state, ast_stmt_void))) return NULL;
        if (!ast_next_token(state, false)) return NULL;
        break;
    case tok_return:
        if (!(node = ast_alloc(state, ast_stmt_return))) return NULL;
        if (!ast_next_token(state, true)) return NULL;
        node->child = parse_expr(state, prec_full);
        if (!ast_eat_token(state, false, tok_semicol)) return NULL;
        break;
    case tok_let:
        if (!(node = parse_let(state))) return NULL;
        if (!ast_eat_token(state, false, tok_semicol)) return NULL;
        break;
    case tok_lbrace:
        if (!(node = parse_stmt_group(state))) return NULL;
        break;
    case tok_if:
        if (!(node = parse_stmt_if(state))) return NULL;
        break;
    case tok_while:
        if (!(node = parse_stmt_while(state))) return NULL;
        break;
    case tok_for:
        if (!(node = parse_stmt_for(state))) return NULL;
        break;
    default: {
        if (!(node = ast_alloc(state, ast_stmt_expr))) return NULL;
        if (!(node->child = parse_expr(state, prec_full))) {
            ast_state_error(state, false, "Expected statement");
            return NULL;
        }
        if (!ast_eat_token(state, false, tok_semicol)) return NULL;
        } break;
    }

    return node;
}
static ast_t *parse_def_struct(ast_state_t *state) {
    if (!ast_eat_token(state, true, tok_struct)) return NULL;
    ast_t *node = ast_alloc(state, ast_def_struct);
    if (!node) return NULL;
    if (!ast_eat_token(state, true, tok_ident)) return NULL;
    if (state->lexer.tok.type == tok_semicol) {
        if (!ast_next_token(state, true)) return NULL;
        return node;
    }
    if (!ast_eat_token(state, true, tok_lbrace)) return NULL;

    ast_t **last = &node->child, *curr = NULL;
    while (state->lexer.tok.type != tok_rbrace) {
        if (!(curr = parse_var(state))) return NULL;
        if (state->lexer.tok.type == tok_comma
            && !ast_next_token(state, true)) return NULL;
        *last = curr;
        last = &curr->next;
    }
    if (!ast_eat_token(state, false, tok_rbrace)) return NULL;
    return node;
}
static ast_t *parse_def_func(ast_state_t *state) {
    if (!ast_eat_token(state, true, tok_fn)) return NULL;
    ast_t *node = ast_alloc(state, ast_def_func);
    if (!node) return NULL;
    if (!ast_eat_token(state, true, tok_ident)) return NULL;
    if (!ast_eat_token(state, true, tok_lparen)) return NULL;

    ast_t **last = &node->a, *curr = NULL;
    while (state->lexer.tok.type != tok_rparen) {
        if (!(curr = parse_var(state))) return NULL;
        if (state->lexer.tok.type == tok_comma
            && !ast_next_token(state, true)) return NULL;
        *last = curr;
        last = &curr->next;
    }
    if (!ast_eat_token(state, true, tok_rparen)) return NULL;
    if (state->lexer.tok.type != tok_arrow) {
        if (!(node->b = ast_alloc(state, ast_type_pod))) return NULL;
        node->b->token = (lex_token_t){
            .type = tok_u0,
            .line = state->lexer.tok.line,
            .chr = state->lexer.tok.chr,
        };
    } else {
        if (!ast_next_token(state, true)) return NULL;
        if (!(node->b = parse_type(state))) return NULL;
    }

    if (state->lexer.tok.type == tok_semicol) {
        if (!ast_next_token(state, false)) return NULL;
        return node;
    }

    if (!(node->child = parse_stmt_group(state))) {
        ast_state_error(state, false, "Expected function body");
        return NULL;
    }

    return node;
}

static ast_t *parse_top_level(ast_state_t *state) {
    switch (state->lexer.tok.type) {
    case tok_extern: {
        if (!ast_next_token(state, true)) return NULL;
        ast_t *node = ast_alloc(state, ast_type_extern);
        if (!node) return NULL;
        if (!(node->child = parse_def_func(state))) return NULL;
        return node;
        }
    case tok_static: {
        if (!ast_next_token(state, true)) return NULL;
        ast_t *node = ast_alloc(state, ast_type_static);
        if (!node) return NULL;
        if (!(node->child = parse_def_func(state))) return NULL;
        return node;
        }
    case tok_let: {
        ast_t *node = parse_let(state);
        if (!ast_eat_token(state, false, tok_semicol)) return NULL;
        return node;
        }
    case tok_typedef: {
        ast_t *node = ast_alloc(state, ast_def_typedef);
        if (!ast_eat_token(state, true, tok_typedef)) return NULL;
        if (!node) return NULL;
        if (!(node->child = parse_var(state))) return NULL;
        if (!ast_eat_token(state, false, tok_semicol)) return NULL;
        return node;
        }
    case tok_struct: return parse_def_struct(state);
    case tok_fn: return parse_def_func(state);
    default: {
        ast_state_error(state, false, "Expected top level declaration.");
    } return NULL;
    }
}

ast_t *ast_build(ast_state_t *state) {
    ast_t *root = NULL, **last, *curr;

    last = &root;
    if (!ast_next_token(state, false)) return root;
    while (state->lexer.tok.type != tok_eof) {
        curr = parse_top_level(state);
        if (!curr) break;
        *last = curr;
        last = &curr->next;
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
    if (!ast->a && !ast->b && !ast->child) fprintf(out, ")");
    else fprintf(out, "){\n");
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
    if (!ast->a && !ast->b && !ast->child) {
        fprintf(out, "%s", ast->next ? ",\n" : "");
    } else {
        print_tabs(out, tabs);
        fprintf(out, "}%s", ast->next ? ",\n" : "");
    }
    if (ast->next) {
        print_tabs(out, tabs);
        ast_log_recurrsive(ast->next, out, tabs);
    }
}

void ast_log(const ast_t *ast, FILE *out) {
    ast_log_recurrsive(ast, out, 0);
}

