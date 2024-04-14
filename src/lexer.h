#ifndef _lexer_h_
#define _lexer_h_

#include "common.h"
#include "tokens.h"

typedef struct lex_token {
    lex_token_type_t ty;
    union {
        strview_t str;
        u8strview_t u8str;
    };
    int32_t line, chr;
} lex_token_t;

typedef struct lex_error {
    bool okay;
    int32_t line, chr;
} lex_error_t;

typedef struct lex_state {
    const uint8_t *src;
    int32_t line, chr;

    lex_token_t tok, next;
} lex_state_t;

lex_state_t lex_state_init(const uint8_t *src);
lex_error_t lex_state_next(lex_state_t *state);

#endif

