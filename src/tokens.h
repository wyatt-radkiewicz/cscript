#ifndef _tokens_h_
#define _tokens_h_

#include <stdint.h>

#define LEX_TOKENS_KEYWORDS \
    X(tok_extern) \
    X(tok_static) \
    X(tok_fn) \
    X(tok_let) \
    X(tok_if) \
    X(tok_as) \
    X(tok_for) \
    X(tok_while) \
    X(tok_new) \
    X(tok_return) \
    X(tok_break) \
    X(tok_continue) \
    X(tok_typedef) \
    X(tok_null) \
    X(tok_switch) \
    X(tok_case) \
    X(tok_default) \
    X(tok_const) \
    X(tok_do) \
    X(tok_enum) \
    X(tok_struct) \
    X(tok_true) \
    X(tok_false) \
    X(tok_sizeof) \
    X(tok_lenof) \
     \
    X(tok_i8) \
    X(tok_i16) \
    X(tok_i32) \
    X(tok_i64) \
    X(tok_u8) \
    X(tok_u16) \
    X(tok_u32) \
    X(tok_u64) \
    X(tok_f32) \
    X(tok_f64) \
    X(tok_c8) \
    X(tok_c16) \
    X(tok_c32) \
    X(tok_b8) \
    X(tok_u0)
#define tok_types_start tok_i8
#define tok_types_last tok_u0
#define tok_num_keywords (tok_u0 + 1)
#define LEX_TOKENS \
    LEX_TOKENS_KEYWORDS \
     \
    X(tok_eof) \
    X(tok_ident) \
    X(tok_literal_u) \
    X(tok_literal_f) \
    X(tok_literal_c) \
    X(tok_literal_str) \
     \
    X(tok_plusplus) \
    X(tok_minusminus) \
    X(tok_lparen) \
    X(tok_rparen) \
    X(tok_lbrack) \
    X(tok_rbrack) \
    X(tok_dot) \
    X(tok_dotdot) \
    X(tok_lbrace) \
    X(tok_rbrace) \
    X(tok_plus) \
    X(tok_minus) \
    X(tok_not) \
    X(tok_bnot) \
    X(tok_star) \
    X(tok_slash) \
    X(tok_modulo) \
    X(tok_lshift) \
    X(tok_rshift) \
    X(tok_less) \
    X(tok_lesseq) \
    X(tok_greater) \
    X(tok_greatereq) \
    X(tok_eqeq) \
    X(tok_noteq) \
    X(tok_band) \
    X(tok_bxor) \
    X(tok_bor) \
    X(tok_and) \
    X(tok_or) \
    X(tok_qmark) \
    X(tok_colon) \
    X(tok_eq) \
    X(tok_pluseq) \
    X(tok_minuseq) \
    X(tok_timeseq) \
    X(tok_diveq) \
    X(tok_modeq) \
    X(tok_lshifteq) \
    X(tok_rshifteq) \
    X(tok_bandeq) \
    X(tok_bxoreq) \
    X(tok_boreq) \
    X(tok_comma) \
    X(tok_arrow) \
    X(tok_semicol)

#define tok_undefined (tok_semicol + 1)

#define X(ENUM) ENUM,
typedef enum lex_token_type {
    LEX_TOKENS
    tok_max,
} lex_token_type_t;
#undef X

typedef struct lex_keyword {
    uint16_t type, psl;
    uint32_t hash;
} lex_keyword_t;

#endif

