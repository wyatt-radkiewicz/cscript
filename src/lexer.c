#include <ctype.h>

#include "keywords.h"
#include "lexer.h"

static lex_error_t lex_error_init(lex_state_t *state, bool okay) {
    return (lex_error_t){
        .okay = okay,
        .line = state->line,
        .chr = state->chr,
    };
}
lex_state_t lex_state_init(const uint8_t *src) {
    lex_state_t state = {
        .src = src,
        .line = 1,
        .chr = 1,
    };
    if (!lex_state_next(&state).okay) state = (lex_state_t){0};
    return state;
}
static inline void settok_len2(lex_state_t *state, lex_token_type_t type) {
    state->src++;
    state->chr++;
    state->next.ty = type;
    state->next.str.len = 2;
}
static inline bool skip_line(lex_state_t *state) {
    uint32_t c;
    
    do {
        if (!u8next_char(&state->src, &c, NULL)) return false;
    } while (c != '\n');

    state->chr = 1;
    state->line++;
    return true;
}
static inline bool skip_whitespace(lex_state_t *state) {
    while (true) {
        uint32_t c, c_next;
        const uint8_t *src = state->src;
        if (!u8next_char(&src, &c, NULL)) return false;
        if (!isspace(c) && c != '/') return true;
        const uint8_t *next_src = src;
        if (!u8next_char(&next_src, &c, NULL)) return false;
        if (c == '/' && c != '/') return true;
        else if (c == '/') skip_line(state);
        else {
            state->src = src;
            state->chr++;
            if (c == '\n') {
                state->chr = 1;
                state->line++;
            }
        }
    }

    return true;
}
lex_error_t lex_state_next(lex_state_t *state) {
    if (!state->src) return lex_error_init(state, false);
    if (state->next.ty == tok_eof) return lex_error_init(state, true);

    if (!skip_whitespace(state)) return lex_error_init(state, false);

    state->tok = state->next;
    state->next = (lex_token_t){
        .line = state->line,
        .chr = state->chr,
        .ty = tok_eof,
        .u8str = (u8strview_t){
            .str = state->src,
            .size = 1,
        },
    };

    state->chr++;
    const uint8_t c = *state->src++;
    switch (c) {
    case '\0': state->next.ty = tok_eof; break;
    case '+':
        if (*state->src == '+') settok_len2(state, tok_plusplus);
        else if (*state->src == '=') settok_len2(state, tok_pluseq);
        else state->next.ty = tok_plus;
        break;
    case '-':
        if (*state->src == '-') settok_len2(state, tok_minusminus);
        else if (*state->src == '=') settok_len2(state, tok_minuseq);
        else state->next.ty = tok_plus;
        break;
    case '(': state->next.ty = tok_lparen; break;
    case ')': state->next.ty = tok_rparen; break;
    case '[': state->next.ty = tok_lbrack; break;
    case ']': state->next.ty = tok_rbrack; break;
    case '.': state->next.ty = tok_dot; break;
    case '{': state->next.ty = tok_lbrace; break;
    case '}': state->next.ty = tok_rbrace; break;
    case '!':
        if (*state->src == '=') settok_len2(state, tok_noteq);
        else state->next.ty = tok_not;
        break;
    case '~': state->next.ty = tok_bnot; break;
    case '*':
        if (*state->src == '=') settok_len2(state, tok_timeseq);
        else state->next.ty = tok_star;
        break;
    case '/':
        if (*state->src == '=') settok_len2(state, tok_diveq);
        else state->next.ty = tok_slash;
        break;
    case '%':
        if (*state->src == '=') settok_len2(state, tok_modeq);
        else state->next.ty = tok_modulo;
        break;
    case '<':
        if (*state->src == '=') settok_len2(state, tok_lesseq);
        else if (*state->src == '<') {
            state->src++;
            state->chr++;
            state->next.ty = tok_lshift;
            state->next.str.len = 2;
            if (*state->src == '=') {
                state->src++;
                state->chr++;
                state->next.ty = tok_lshifteq;
                state->next.str.len = 3;
            }
        } else state->next.ty = tok_less;
        break;
    case '>':
        if (*state->src == '=') settok_len2(state, tok_greatereq);
        else if (*state->src == '>') {
            state->src++;
            state->chr++;
            state->next.ty = tok_rshift;
            state->next.str.len = 2;
            if (*state->src == '=') {
                state->src++;
                state->chr++;
                state->next.ty = tok_rshifteq;
                state->next.str.len = 3;
            }
        } else state->next.ty = tok_greater;
        break;
    case '=':
        if (*state->src == '=') settok_len2(state, tok_eqeq);
        else if (*state->src & 0x80) return lex_error_init(state, false);
        else state->next.ty = tok_eq;
    case '&':
        if (*state->src == '&') settok_len2(state, tok_and);
        else if (*state->src == '=') settok_len2(state, tok_bandeq);
        else state->next.ty = tok_band;
    case '^':
        if (*state->src == '=') settok_len2(state, tok_bxoreq);
        else if (*state->src & 0x80) return lex_error_init(state, false);
        else state->next.ty = tok_bxor;
    case '|':
        if (*state->src == '|') settok_len2(state, tok_or);
        else if (*state->src == '=') settok_len2(state, tok_boreq);
        else state->next.ty = tok_bor;
    case '?': state->next.ty = tok_qmark; break;
    case ':': state->next.ty = tok_colon; break;
    case ',': state->next.ty = tok_comma; break;
    case ';': state->next.ty = tok_semicol; break;
    default:
        if (c & 0x80) return lex_error_init(state, true);

        break;
    }

    return lex_error_init(state, false);
}

