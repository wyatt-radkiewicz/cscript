#ifndef _lexer_h_
#define _lexer_h_

#include "common.h"
#include "tokens.h"

typedef struct lex_token_char {
    uint32_t c32, bits;
} lex_token_char_t;

typedef struct lex_token {
    lex_token_type_t type;
    union {
        strview_t str;
        u8strview_t u8str;
        lex_token_char_t chr;
        uint64_t u64;
        double f64;
    } data;
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

#include <stdio.h>
void lex_token_log(const lex_token_t *tok, FILE *out);

#endif

