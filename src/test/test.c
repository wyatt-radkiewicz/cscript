#include <stdio.h>

#include <sys/mman.h>

#include "../cnm.c"

static uint8_t test_region[2048];
static uint8_t test_globals[2048];
static void *test_code_area;
static size_t test_code_size;

static void test_errcb(int line, const char *verbose, const char *simple) {
    printf("%s", verbose);
}
static bool test_expect_err = false;
static void test_expect_errcb(int line, const char *v, const char *s) {
    test_expect_err = true;
}

#define SIMPLE_TEST(_name, _errcb, _src, ...) \
    static bool _name(void) { \
        cnm_t *cnm = cnm_init(test_region, sizeof(test_region), \
                              test_code_area, test_code_size, \
                              test_globals, sizeof(test_globals)); \
        cnm_set_errcb(cnm, _errcb); \
        cnm_set_src(cnm, _src, #_name".cnm"); \
        __VA_ARGS__ \
    }

///////////////////////////////////////////////////////////////////////////////
//
// Lexer testing
//
///////////////////////////////////////////////////////////////////////////////
SIMPLE_TEST(test_lexer_uninitialized, test_errcb, "", {
    if (cnm->s.tok.type != TOKEN_UNINITIALIZED) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 1) return false;
    if (!strview_eq(cnm->s.tok.src, SV(""))) return false;
    return true;
})
SIMPLE_TEST(test_lexer_ident, test_errcb, " _foo123_ ", {
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_IDENT) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 2) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 10) return false;
    if (!strview_eq(cnm->s.tok.src, SV("_foo123_"))) return false;
    return true;
})
SIMPLE_TEST(test_lexer_string1, test_errcb, "\"hello \"", {
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_STRING) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 9) return false;
    if (!strview_eq(cnm->s.tok.src, SV("\"hello \""))) return false;
    if (strcmp(cnm->s.tok.s, "hello ") != 0) return false;
    return true;
})
SIMPLE_TEST(test_lexer_string2, test_errcb, "\"hello \"  \"world\"", {
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_STRING) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 18) return false;
    if (!strview_eq(cnm->s.tok.src, SV("\"hello \"  \"world\""))) return false;
    if (strcmp(cnm->s.tok.s, "hello world") != 0) return false;
    return true;
})
SIMPLE_TEST(test_lexer_string3, test_errcb,
        "\"hello \"\n"
        "  \"world\"", {
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_STRING) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 2) return false;
    if (cnm->s.tok.end.col != 10) return false;
    if (!strview_eq(cnm->s.tok.src, SV("\"hello \"\n  \"world\""))) return false;
    if (strcmp(cnm->s.tok.s, "hello world") != 0) return false;
    return true;
})
SIMPLE_TEST(test_lexer_string4, test_errcb, "\"hello \\\"world\"", {
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_STRING) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 16) return false;
    if (!strview_eq(cnm->s.tok.src, SV("\"hello \\\"world\""))) return false;
    if (strcmp(cnm->s.tok.s, "hello \"world") != 0) return false;
    return true;
})
SIMPLE_TEST(test_lexer_string5, test_errcb,  "\"hello \\u263A world\"", {
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_STRING) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 21) return false;
    if (!strview_eq(cnm->s.tok.src, SV("\"hello \\u263A world\""))) return false;
    if (strcmp(cnm->s.tok.s, "hello ☺ world") != 0) return false;
    return true;
})
SIMPLE_TEST(test_lexer_string6, test_errcb,  "\"hello ☺ world\"", {
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_STRING) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 16) return false;
    if (!strview_eq(cnm->s.tok.src, SV("\"hello ☺ world\""))) return false;
    if (strcmp(cnm->s.tok.s, "hello ☺ world") != 0) return false;
    return true;
})
SIMPLE_TEST(test_lexer_char1, test_errcb,  "'h'", {
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_CHAR) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 4) return false;
    if (!strview_eq(cnm->s.tok.src, SV("'h'"))) return false;
    if (cnm->s.tok.c != 'h') return false;
    return true;
})
SIMPLE_TEST(test_lexer_char2, test_errcb,  "'\\x41'", {
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_CHAR) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 7) return false;
    if (!strview_eq(cnm->s.tok.src, SV("'\\x41'"))) return false;
    if (cnm->s.tok.c != 'A') return false;
    return true;
})
SIMPLE_TEST(test_lexer_char3, test_errcb,  "'☺'", {
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_CHAR) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 4) return false;
    if (!strview_eq(cnm->s.tok.src, SV("'☺'"))) return false;
    if (cnm->s.tok.c != 0x263A) return false;
    return true;
})
SIMPLE_TEST(test_lexer_char4, test_expect_errcb,  "'h '", {
    token_next(cnm);
    return test_expect_err;
})
SIMPLE_TEST(test_lexer_int1, test_errcb,  "120", {
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_INT) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 4) return false;
    if (!strview_eq(cnm->s.tok.src, SV("120"))) return false;
    if (cnm->s.tok.i.base != 10) return false;
    if (cnm->s.tok.i.n != 120) return false;
    if (strcmp(cnm->s.tok.i.suffix, "") != 0) return false;
    return true;
})
SIMPLE_TEST(test_lexer_int2, test_errcb,  "69U", {
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_INT) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 4) return false;
    if (!strview_eq(cnm->s.tok.src, SV("69U"))) return false;
    if (cnm->s.tok.i.base != 10) return false;
    if (cnm->s.tok.i.n != 69) return false;
    if (strcmp(cnm->s.tok.i.suffix, "u") != 0) return false;
    return true;
})
SIMPLE_TEST(test_lexer_int3, test_errcb,  "420ulL", {
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_INT) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 7) return false;
    if (!strview_eq(cnm->s.tok.src, SV("420ulL"))) return false;
    if (cnm->s.tok.i.base != 10) return false;
    if (cnm->s.tok.i.n != 420) return false;
    if (strcmp(cnm->s.tok.i.suffix, "ull") != 0) return false;
    return true;
})
SIMPLE_TEST(test_lexer_int4, test_errcb,  "0xFF", {
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_INT) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 5) return false;
    if (!strview_eq(cnm->s.tok.src, SV("0xFF"))) return false;
    if (cnm->s.tok.i.base != 16) return false;
    if (cnm->s.tok.i.n != 255) return false;
    if (strcmp(cnm->s.tok.i.suffix, "") != 0) return false;
    return true;
})
SIMPLE_TEST(test_lexer_int5, test_expect_errcb,  "0xxF", {
    token_next(cnm);
    return test_expect_err;
})
SIMPLE_TEST(test_lexer_int6, test_expect_errcb,  "420f", {
    token_next(cnm);
    return test_expect_err;
})
SIMPLE_TEST(test_lexer_int7, test_errcb,  "0b1101", {
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_INT) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 7) return false;
    if (!strview_eq(cnm->s.tok.src, SV("0b1101"))) return false;
    if (cnm->s.tok.i.base != 2) return false;
    if (cnm->s.tok.i.n != 13) return false;
    if (strcmp(cnm->s.tok.i.suffix, "") != 0) return false;
    return true;
})
SIMPLE_TEST(test_lexer_double1, test_errcb,  "0.0", {
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_DOUBLE) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 4) return false;
    if (!strview_eq(cnm->s.tok.src, SV("0.0"))) return false;
    if (cnm->s.tok.f.n != 0.0) return false;
    if (strcmp(cnm->s.tok.f.suffix, "") != 0) return false;
    return true;
})
SIMPLE_TEST(test_lexer_double2, test_errcb,  "0.5", {
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_DOUBLE) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 4) return false;
    if (!strview_eq(cnm->s.tok.src, SV("0.5"))) return false;
    if (cnm->s.tok.f.n != 0.5) return false;
    if (strcmp(cnm->s.tok.f.suffix, "") != 0) return false;
    return true;
})
SIMPLE_TEST(test_lexer_double3, test_errcb,  "420.69", {
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_DOUBLE) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 7) return false;
    if (!strview_eq(cnm->s.tok.src, SV("420.69"))) return false;
    if (cnm->s.tok.f.n != 420.69) return false;
    if (strcmp(cnm->s.tok.f.suffix, "") != 0) return false;
    return true;
})
SIMPLE_TEST(test_lexer_double4, test_errcb,  "420.69f", {
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_DOUBLE) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 8) return false;
    if (!strview_eq(cnm->s.tok.src, SV("420.69f"))) return false;
    if (cnm->s.tok.f.n != 420.69) return false;
    if (strcmp(cnm->s.tok.f.suffix, "f") != 0) return false;
    return true;
})
SIMPLE_TEST(test_lexer_double5, test_errcb,  "42.", {
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_DOUBLE) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 4) return false;
    if (!strview_eq(cnm->s.tok.src, SV("42."))) return false;
    if (cnm->s.tok.f.n != 42.0) return false;
    if (strcmp(cnm->s.tok.f.suffix, "") != 0) return false;
    return true;
})
SIMPLE_TEST(test_lexer_double6, test_errcb,  "42.f", {
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_DOUBLE) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 5) return false;
    if (!strview_eq(cnm->s.tok.src, SV("42.f"))) return false;
    if (cnm->s.tok.f.n != 42.0) return false;
    if (strcmp(cnm->s.tok.f.suffix, "f") != 0) return false;
    return true;
})
SIMPLE_TEST(test_lexer_double7, test_expect_errcb,  "0.0asdf", {
    token_next(cnm);
    return test_expect_err;
})

#define TEST_LEXER_TOKEN(name_end, string, token_type) \
    static bool test_lexer_##name_end(void) { \
        cnm_t *cnm = cnm_init(test_region, sizeof(test_region), \
                              test_code_area, test_code_size, \
                              test_globals, sizeof(test_globals)); \
        cnm_set_errcb(cnm, test_errcb); \
        cnm_set_src(cnm, string, "test_lexer_"#name_end".cnm"); \
        token_next(cnm); \
        if (cnm->s.tok.type != token_type) return false; \
        if (cnm->s.tok.start.row != 1) return false; \
        if (cnm->s.tok.start.col != 1) return false; \
        if (cnm->s.tok.end.row != 1) return false; \
        if (cnm->s.tok.end.col != sizeof(string)) return false; \
        if (!strview_eq(cnm->s.tok.src, SV(string))) return false; \
        return true; \
    }
TEST_LEXER_TOKEN(eof, "", TOKEN_EOF)
TEST_LEXER_TOKEN(dot, ".", TOKEN_DOT)
TEST_LEXER_TOKEN(comma, ",", TOKEN_COMMA)
TEST_LEXER_TOKEN(cond, "?", TOKEN_COND)
TEST_LEXER_TOKEN(colon, ":", TOKEN_COLON)
TEST_LEXER_TOKEN(semicolon, ";", TOKEN_SEMICOLON)
TEST_LEXER_TOKEN(paren_l, "(", TOKEN_PAREN_L)
TEST_LEXER_TOKEN(paren_r, ")", TOKEN_PAREN_R)
TEST_LEXER_TOKEN(brack_l, "[", TOKEN_BRACK_L)
TEST_LEXER_TOKEN(brack_r, "]", TOKEN_BRACK_R)
TEST_LEXER_TOKEN(brace_l, "{", TOKEN_BRACE_L)
TEST_LEXER_TOKEN(brace_r, "}", TOKEN_BRACE_R)
TEST_LEXER_TOKEN(plus, "+", TOKEN_PLUS)
TEST_LEXER_TOKEN(plus_eq, "+=", TOKEN_PLUS_EQ)
TEST_LEXER_TOKEN(plus_dbl, "++", TOKEN_PLUS_DBL)
TEST_LEXER_TOKEN(minus, "-", TOKEN_MINUS)
TEST_LEXER_TOKEN(minus_eq, "-=", TOKEN_MINUS_EQ)
TEST_LEXER_TOKEN(minus_dbl, "--", TOKEN_MINUS_DBL)
TEST_LEXER_TOKEN(star, "*", TOKEN_STAR)
TEST_LEXER_TOKEN(times_eq, "*=", TOKEN_TIMES_EQ)
TEST_LEXER_TOKEN(divide, "/", TOKEN_DIVIDE)
TEST_LEXER_TOKEN(divide_eq, "/=", TOKEN_DIVIDE_EQ)
TEST_LEXER_TOKEN(modulo, "%", TOKEN_MODULO)
TEST_LEXER_TOKEN(modulo_eq, "%=", TOKEN_MODULO_EQ)
TEST_LEXER_TOKEN(bit_xor, "^", TOKEN_BIT_XOR)
TEST_LEXER_TOKEN(bit_xor_eq, "^=", TOKEN_BIT_XOR_EQ)
TEST_LEXER_TOKEN(bit_not, "~", TOKEN_BIT_NOT)
TEST_LEXER_TOKEN(bit_not_eq, "~=", TOKEN_BIT_NOT_EQ)
TEST_LEXER_TOKEN(not, "!", TOKEN_NOT)
TEST_LEXER_TOKEN(not_eq, "!=", TOKEN_NOT_EQ)
TEST_LEXER_TOKEN(assign, "=", TOKEN_ASSIGN)
TEST_LEXER_TOKEN(eq_eq, "==", TOKEN_EQ_EQ)
TEST_LEXER_TOKEN(bit_and, "&", TOKEN_BIT_AND)
TEST_LEXER_TOKEN(and_eq, "&=", TOKEN_AND_EQ)
TEST_LEXER_TOKEN(and, "&&", TOKEN_AND)
TEST_LEXER_TOKEN(bit_or, "|", TOKEN_BIT_OR)
TEST_LEXER_TOKEN(or_eq, "|=", TOKEN_OR_EQ)
TEST_LEXER_TOKEN(or, "||", TOKEN_OR)
TEST_LEXER_TOKEN(less, "<", TOKEN_LESS)
TEST_LEXER_TOKEN(less_eq, "<=", TOKEN_LESS_EQ)
TEST_LEXER_TOKEN(shift_l, "<<", TOKEN_SHIFT_L)
TEST_LEXER_TOKEN(shift_l_eq, "<<=", TOKEN_SHIFT_L_EQ)
TEST_LEXER_TOKEN(greater, ">", TOKEN_GREATER)
TEST_LEXER_TOKEN(greater_eq, ">=", TOKEN_GREATER_EQ)
TEST_LEXER_TOKEN(shift_r, ">>", TOKEN_SHIFT_R)
TEST_LEXER_TOKEN(shift_r_eq, ">>=", TOKEN_SHIFT_R_EQ)

SIMPLE_TEST(test_lexer_token_location, test_errcb, 
        ".,?:;()[]{}+ +=++\n"
        "- -=--**=//=%%=^^=\n"
        "~~=!!== ==& &=&&\n"
        "| |=||< <=<<<<=> >=>>>>=\n"
        "foo \"str\" \"ing\" 'A' 34u 0.0f", {
    static const struct {
        int row, start_col, end_col;
        size_t len;
    } token_checks[TOKEN_MAX - TOKEN_DOT] = {
        // row  scol ecol len
        {  1,   1,   2,   1   }, // TOKEN_DOT
        {  1,   2,   3,   1   }, // TOKEN_COMMA
        {  1,   3,   4,   1   }, // TOKEN_COND
        {  1,   4,   5,   1   }, // TOKEN_COLON
        {  1,   5,   6,   1   }, // TOKEN_SEMICOLON
        {  1,   6,   7,   1   }, // TOKEN_PAREN_L
        {  1,   7,   8,   1   }, // TOKEN_PAREN_R
        {  1,   8,   9,   1   }, // TOKEN_BRACK_L
        {  1,   9,   10,  1   }, // TOKEN_BRACK_R
        {  1,   10,  11,  1   }, // TOKEN_BRACE_L
        {  1,   11,  12,  1   }, // TOKEN_BRACE_R
        {  1,   12,  13,  1   }, // TOKEN_PLUS
        {  1,   14,  16,  2   }, // TOKEN_PLUS_EQ
        {  1,   16,  18,  2   }, // TOKEN_PLUS_DBL
        {  2,   1,   2,   1   }, // TOKEN_MINUS
        {  2,   3,   5,   2   }, // TOKEN_MINUS_EQ
        {  2,   5,   7,   2   }, // TOKEN_MINUS_DBL
        {  2,   7,   8,   1   }, // TOKEN_STAR
        {  2,   8,   10,  2   }, // TOKEN_TIMES_EQ
        {  2,   10,  11,  1   }, // TOKEN_DIVIDE
        {  2,   11,  13,  2   }, // TOKEN_DIVIDE_EQ
        {  2,   13,  14,  1   }, // TOKEN_MODULO
        {  2,   14,  16,  2   }, // TOKEN_MODULO_EQ
        {  2,   16,  17,  1   }, // TOKEN_BIT_XOR
        {  2,   17,  19,  2   }, // TOKEN_BIT_XOR_EQ
        {  3,   1,   2,   1   }, // TOKEN_BIT_NOT
        {  3,   2,   4,   2   }, // TOKEN_BIT_NOT_EQ
        {  3,   4,   5,   1   }, // TOKEN_NOT
        {  3,   5,   7,   2   }, // TOKEN_NOT_EQ
        {  3,   7,   8,   1   }, // TOKEN_ASSIGN
        {  3,   9,   11,  2   }, // TOKEN_EQ_EQ
        {  3,   11,  12,  1   }, // TOKEN_BIT_AND
        {  3,   13,  15,  2   }, // TOKEN_AND_EQ
        {  3,   15,  17,  2   }, // TOKEN_AND
        {  4,   1,   2,   1   }, // TOKEN_BIT_OR
        {  4,   3,   5,   2   }, // TOKEN_OR_EQ
        {  4,   5,   7,   2   }, // TOKEN_OR
        {  4,   7,   8,   1   }, // TOKEN_LESS
        {  4,   9,   11,  2   }, // TOKEN_LESS_EQ
        {  4,   11,  13,  2   }, // TOKEN_SHIFT_L
        {  4,   13,  16,  3   }, // TOKEN_SHIFT_L_EQ
        {  4,   16,  17,  1   }, // TOKEN_GREATER
        {  4,   18,  20,  2   }, // TOKEN_GREATER_EQ
        {  4,   20,  22,  2   }, // TOKEN_SHIFT_R
        {  4,   22,  25,  3   }, // TOKEN_SHIFT_R_EQ
    };
    for (int i = 0; i < TOKEN_MAX - TOKEN_DOT; i++) {
        token_next(cnm);

        if (cnm->s.tok.type != i + TOKEN_DOT
            || cnm->s.tok.start.row != token_checks[i].row
            || cnm->s.tok.start.col != token_checks[i].start_col
            || cnm->s.tok.end.row != token_checks[i].row
            || cnm->s.tok.end.col != token_checks[i].end_col
            || cnm->s.tok.src.len != token_checks[i].len) {
            return false;
        }
    }

    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_IDENT || cnm->s.tok.start.row != 5
        || cnm->s.tok.start.col != 1   || cnm->s.tok.end.row != 5
        || cnm->s.tok.end.col != 4     || cnm->s.tok.src.len != 3) return false;
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_STRING || cnm->s.tok.start.row != 5
        || cnm->s.tok.start.col != 5   || cnm->s.tok.end.row != 5
        || cnm->s.tok.end.col != 16    || cnm->s.tok.src.len != 11) return false;
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_CHAR || cnm->s.tok.start.row != 5
        || cnm->s.tok.start.col != 17  || cnm->s.tok.end.row != 5
        || cnm->s.tok.end.col != 20    || cnm->s.tok.src.len != 3) return false;
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_INT || cnm->s.tok.start.row != 5
        || cnm->s.tok.start.col != 21  || cnm->s.tok.end.row != 5
        || cnm->s.tok.end.col != 24    || cnm->s.tok.src.len != 3) return false;
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_DOUBLE || cnm->s.tok.start.row != 5
        || cnm->s.tok.start.col != 25  || cnm->s.tok.end.row != 5
        || cnm->s.tok.end.col != 29    || cnm->s.tok.src.len != 4) return false;
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_EOF || cnm->s.tok.start.row != 5
        || cnm->s.tok.start.col != 29  || cnm->s.tok.end.row != 5
        || cnm->s.tok.end.col != 29    || cnm->s.tok.src.len != 0) return false;

    return true;
})

///////////////////////////////////////////////////////////////////////////////
//
// AST Constant Folding Testing
//
///////////////////////////////////////////////////////////////////////////////
SIMPLE_TEST(test_ast_constant_folding1, test_errcb,  "5", {
    token_next(cnm);
    ast_t *ast = ast_generate(cnm, PREC_FULL);
    if (ast->class != AST_NUM_LITERAL) return false;
    if (ast->type.type[0].class != TYPE_INT) return false;
    if (ast->type.size != 1) return false;
    if (ast->num_literal.i != 5) return false;
    return true;
})

SIMPLE_TEST(test_ast_constant_folding2, test_errcb,  "5.0", {
    token_next(cnm);
    ast_t *ast = ast_generate(cnm, PREC_FULL);
    if (ast->class != AST_NUM_LITERAL) return false;
    if (ast->type.type[0].class != TYPE_DOUBLE) return false;
    if (ast->num_literal.f != 5.0) return false;
    return true;
})
SIMPLE_TEST(test_ast_constant_folding3, test_errcb,  "5000000000", {
    token_next(cnm);
    ast_t *ast = ast_generate(cnm, PREC_FULL);
    if (ast->class != AST_NUM_LITERAL) return false;
    if (ast->type.type[0].class != TYPE_LONG) return false;
    if (ast->num_literal.i != 5000000000) return false;
    return true;
})
SIMPLE_TEST(test_ast_constant_folding4, test_errcb,  "5u", {
    token_next(cnm);
    ast_t *ast = ast_generate(cnm, PREC_FULL);
    if (ast->class != AST_NUM_LITERAL) return false;
    if (ast->type.type[0].class != TYPE_UINT) return false;
    if (ast->num_literal.u != 5) return false;
    return true;
})
SIMPLE_TEST(test_ast_constant_folding5, test_errcb,  "9223372036854775809ull", {
    token_next(cnm);
    ast_t *ast = ast_generate(cnm, PREC_FULL);
    if (ast->class != AST_NUM_LITERAL) return false;
    if (ast->type.type[0].class != TYPE_ULLONG) return false;
    if (ast->num_literal.u != 9223372036854775809ull) return false;
    return true;
})
SIMPLE_TEST(test_ast_constant_folding6, test_expect_errcb,  "9223372036854775809", {
    token_next(cnm);
    ast_t *ast = ast_generate(cnm, PREC_FULL);
    return test_expect_err;
})
SIMPLE_TEST(test_ast_constant_folding7, test_errcb,  "92ull", {
    token_next(cnm);
    ast_t *ast = ast_generate(cnm, PREC_FULL);
    if (ast->class != AST_NUM_LITERAL) return false;
    if (ast->type.type[0].class != TYPE_ULLONG) return false;
    if (ast->num_literal.u != 92ull) return false;
    return true;
})
SIMPLE_TEST(test_ast_constant_folding8, test_errcb,  "5 + 5", {
    token_next(cnm);
    ast_t *ast = ast_generate(cnm, PREC_FULL);
    if (ast->class != AST_NUM_LITERAL) return false;
    if (ast->type.type[0].class != TYPE_INT) return false;
    if (ast->num_literal.i != 10) return false;
    return true;
})
SIMPLE_TEST(test_ast_constant_folding9, test_errcb,  "5 * 5 + 3", {
    token_next(cnm);
    ast_t *ast = ast_generate(cnm, PREC_FULL);
    if (ast->class != AST_NUM_LITERAL) return false;
    if (ast->type.type[0].class != TYPE_INT) return false;
    if (ast->num_literal.i != 28) return false;
    return true;
})
SIMPLE_TEST(test_ast_constant_folding10, test_errcb,  "5 + 5 * 3", {
    token_next(cnm);
    ast_t *ast = ast_generate(cnm, PREC_FULL);
    if (ast->class != AST_NUM_LITERAL) return false;
    if (ast->type.type[0].class != TYPE_INT) return false;
    if (ast->num_literal.i != 20) return false;
    return true;
})
SIMPLE_TEST(test_ast_constant_folding11, test_errcb,  "5ul + 30.0", {
    token_next(cnm);
    ast_t *ast = ast_generate(cnm, PREC_FULL);
    if (ast->class != AST_NUM_LITERAL) return false;
    if (ast->type.type[0].class != TYPE_DOUBLE) return false;
    if (ast->num_literal.f != 35) return false;
    return true;
})
SIMPLE_TEST(test_ast_constant_folding12, test_errcb,  "10 / 3.0", {
    token_next(cnm);
    ast_t *ast = ast_generate(cnm, PREC_FULL);
    if (ast->class != AST_NUM_LITERAL) return false;
    if (ast->type.type[0].class != TYPE_DOUBLE) return false;
    if (ast->num_literal.f != 10.0 / 3.0) return false;
    return true;
})
SIMPLE_TEST(test_ast_constant_folding13, test_errcb,  "10 / 3", {
    token_next(cnm);
    ast_t *ast = ast_generate(cnm, PREC_FULL);
    if (ast->class != AST_NUM_LITERAL) return false;
    if (ast->type.type[0].class != TYPE_INT) return false;
    if (ast->num_literal.i != 10 / 3) return false;
    return true;
})
SIMPLE_TEST(test_ast_constant_folding14, test_errcb,  "10ul * 12", {
    token_next(cnm);
    ast_t *ast = ast_generate(cnm, PREC_FULL);
    if (ast->class != AST_NUM_LITERAL) return false;
    if (ast->type.type[0].class != TYPE_ULONG) return false;
    if (ast->num_literal.u != 120) return false;
    return true;
})
SIMPLE_TEST(test_ast_constant_folding15, test_errcb,  "3 - 9 * 2l", {
    token_next(cnm);
    ast_t *ast = ast_generate(cnm, PREC_FULL);
    if (ast->class != AST_NUM_LITERAL) return false;
    if (ast->type.type[0].class != TYPE_LONG) return false;
    if (ast->num_literal.i != -15) return false;
    return true;
})
SIMPLE_TEST(test_ast_constant_folding16, test_errcb,  "-8", {
    token_next(cnm);
    ast_t *ast = ast_generate(cnm, PREC_FULL);
    if (ast->class != AST_NUM_LITERAL) return false;
    if (ast->type.type[0].class != TYPE_INT) return false;
    if (ast->num_literal.i != -8) return false;
    return true;
})
SIMPLE_TEST(test_ast_constant_folding17, test_errcb,  "(-8)", {
    token_next(cnm);
    ast_t *ast = ast_generate(cnm, PREC_FULL);
    if (ast->class != AST_NUM_LITERAL) return false;
    if (ast->type.type[0].class != TYPE_INT) return false;
    if (ast->num_literal.i != -8) return false;
    return true;
})
SIMPLE_TEST(test_ast_constant_folding18, test_errcb,  "-(0x8)", {
    token_next(cnm);
    ast_t *ast = ast_generate(cnm, PREC_FULL);
    if (ast->class != AST_NUM_LITERAL) return false;
    if (ast->type.type[0].class != TYPE_INT) return false;
    if (ast->num_literal.i != -8) return false;
    return true;
})
SIMPLE_TEST(test_ast_constant_folding19, test_errcb,  "~0x8u", {
    token_next(cnm);
    ast_t *ast = ast_generate(cnm, PREC_FULL);
    if (ast->class != AST_NUM_LITERAL) return false;
    if (ast->type.type[0].class != TYPE_UINT) return false;
    if (ast->num_literal.u != ~0x8u) return false;
    return true;
})
SIMPLE_TEST(test_ast_constant_folding20, test_expect_errcb,  "~(1 + 0.9)", {
    token_next(cnm);
    ast_t *ast = ast_generate(cnm, PREC_FULL);
    return test_expect_err;
})
SIMPLE_TEST(test_ast_constant_folding21, test_errcb,  "!1", {
    token_next(cnm);
    ast_t *ast = ast_generate(cnm, PREC_FULL);
    if (ast->class != AST_NUM_LITERAL) return false;
    if (ast->type.type[0].class != TYPE_BOOL) return false;
    if (ast->num_literal.u != false) return false;
    return true;
})
SIMPLE_TEST(test_ast_constant_folding22, test_errcb,  "!0.0", {
    token_next(cnm);
    ast_t *ast = ast_generate(cnm, PREC_FULL);
    if (ast->class != AST_NUM_LITERAL) return false;
    if (ast->type.type[0].class != TYPE_BOOL) return false;
    if (ast->num_literal.u != true) return false;
    return true;
})
SIMPLE_TEST(test_ast_constant_folding23, test_errcb,  "1 << 4", {
    token_next(cnm);
    ast_t *ast = ast_generate(cnm, PREC_FULL);
    if (ast->class != AST_NUM_LITERAL) return false;
    if (ast->type.type[0].class != TYPE_INT) return false;
    if (ast->num_literal.i != 16) return false;
    return true;
})
SIMPLE_TEST(test_ast_constant_folding24, test_errcb,  "0xff | 0xff << 8", {
    token_next(cnm);
    ast_t *ast = ast_generate(cnm, PREC_FULL);
    if (ast->class != AST_NUM_LITERAL) return false;
    if (ast->type.type[0].class != TYPE_INT) return false;
    if (ast->num_literal.i != 0xffff) return false;
    return true;
})
SIMPLE_TEST(test_ast_constant_folding25, test_errcb,  "-(5 + 10) * 2 + 60 >> 2", {
    token_next(cnm);
    ast_t *ast = ast_generate(cnm, PREC_FULL);
    if (ast->class != AST_NUM_LITERAL) return false;
    if (ast->type.type[0].class != TYPE_INT) return false;
    if (ast->num_literal.i != (-(5 + 10) * 2 + 60) >> 2) return false;
    return true;
})

///////////////////////////////////////////////////////////////////////////////
//
// Type parsing tests
//
///////////////////////////////////////////////////////////////////////////////
SIMPLE_TEST(test_type_parsing1, test_errcb,  "const char *foo", {
    token_next(cnm);
    bool istypedef;
    strview_t name;
    typeref_t type = type_parse(cnm, &name, &istypedef);
    if (istypedef) return false;
    if (!strview_eq(name, SV("foo"))) return false;
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_PTR },
            (type_t){ .class = TYPE_CHAR, .isconst = true },
        },
        .size = 2,
    })) return false;
    return true;
})
SIMPLE_TEST(test_type_parsing2, test_errcb,  "typedef unsigned char u8", {
    token_next(cnm);
    bool istypedef;
    strview_t name;
    typeref_t type = type_parse(cnm, &name, &istypedef);
    if (!istypedef) return false;
    if (!strview_eq(name, SV("u8"))) return false;
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_UCHAR },
        },
        .size = 1,
    })) return false;
    return true;
})
SIMPLE_TEST(test_type_parsing_char, test_errcb,  "char", {
    token_next(cnm);
    typeref_t type = type_parse(cnm, NULL, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_CHAR },
        },
        .size = 1,
    })) return false;
    return true;
})
SIMPLE_TEST(test_type_parsing_uchar, test_errcb,  "unsigned char", {
    token_next(cnm);
    typeref_t type = type_parse(cnm, NULL, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_UCHAR },
        },
        .size = 1,
    })) return false;
    return true;
})
SIMPLE_TEST(test_type_parsing_short1, test_errcb,  "short", {
    token_next(cnm);
    typeref_t type = type_parse(cnm, NULL, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_SHORT },
        },
        .size = 1,
    })) return false;
    return true;
})
SIMPLE_TEST(test_type_parsing_short2, test_errcb,  "short int", {
    token_next(cnm);
    typeref_t type = type_parse(cnm, NULL, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_SHORT },
        },
        .size = 1,
    })) return false;
    return true;
})
SIMPLE_TEST(test_type_parsing_short3, test_expect_errcb,  "short short int", {
    token_next(cnm);
    type_parse(cnm, NULL, NULL);
    return test_expect_err;
})
SIMPLE_TEST(test_type_parsing_short4, test_expect_errcb,  "short char", {
    token_next(cnm);
    type_parse(cnm, NULL, NULL);
    return test_expect_err;
})
SIMPLE_TEST(test_type_parsing_ushort1, test_errcb,  "unsigned short", {
    token_next(cnm);
    typeref_t type = type_parse(cnm, NULL, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_USHORT },
        },
        .size = 1,
    })) return false;
    return true;
})
SIMPLE_TEST(test_type_parsing_ushort2, test_errcb,  "unsigned short int", {
    token_next(cnm);
    typeref_t type = type_parse(cnm, NULL, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_USHORT },
        },
        .size = 1,
    })) return false;
    return true;
})
SIMPLE_TEST(test_type_parsing_ushort3, test_expect_errcb,  "unsigned unsigned short", {
    token_next(cnm);
    type_parse(cnm, NULL, NULL);
    return test_expect_err;
})
SIMPLE_TEST(test_type_parsing_int, test_errcb,  "int", {
    token_next(cnm);
    typeref_t type = type_parse(cnm, NULL, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT },
        },
        .size = 1,
    })) return false;
    return true;
})
SIMPLE_TEST(test_type_parsing_uint1, test_errcb,  "unsigned int", {
    token_next(cnm);
    typeref_t type = type_parse(cnm, NULL, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_UINT },
        },
        .size = 1,
    })) return false;
    return true;
})
SIMPLE_TEST(test_type_parsing_uint2, test_errcb,  "unsigned", {
    token_next(cnm);
    typeref_t type = type_parse(cnm, NULL, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_UINT },
        },
        .size = 1,
    })) return false;
    return true;
})
SIMPLE_TEST(test_type_parsing_uint3, test_errcb,  "const unsigned", {
    token_next(cnm);
    typeref_t type = type_parse(cnm, NULL, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_UINT, .isconst = true },
        },
        .size = 1,
    })) return false;
    if (!type.type[0].isconst) return false;
    return true;
})
SIMPLE_TEST(test_type_parsing_long1, test_errcb,  "long", {
    token_next(cnm);
    typeref_t type = type_parse(cnm, NULL, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_LONG },
        },
        .size = 1,
    })) return false;
    return true;
})
SIMPLE_TEST(test_type_parsing_long2, test_errcb,  "long long", {
    token_next(cnm);
    typeref_t type = type_parse(cnm, NULL, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_LLONG },
        },
        .size = 1,
    })) return false;
    return true;
})
SIMPLE_TEST(test_type_parsing_long3, test_errcb,  "long int", {
    token_next(cnm);
    typeref_t type = type_parse(cnm, NULL, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_LONG },
        },
        .size = 1,
    })) return false;
    return true;
})
SIMPLE_TEST(test_type_parsing_long4, test_errcb,  "long long int", {
    token_next(cnm);
    typeref_t type = type_parse(cnm, NULL, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_LLONG },
        },
        .size = 1,
    })) return false;
    return true;
})
SIMPLE_TEST(test_type_parsing_ulong, test_errcb,  "unsigned long long", {
    token_next(cnm);
    typeref_t type = type_parse(cnm, NULL, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_ULLONG },
        },
        .size = 1,
    })) return false;
    return true;
})
SIMPLE_TEST(test_type_parsing_float, test_errcb,  "float", {
    token_next(cnm);
    typeref_t type = type_parse(cnm, NULL, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_FLOAT },
        },
        .size = 1,
    })) return false;
    return true;
})
SIMPLE_TEST(test_type_parsing_double, test_errcb,  "double", {
    token_next(cnm);
    typeref_t type = type_parse(cnm, NULL, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_DOUBLE },
        },
        .size = 1,
    })) return false;
    return true;
})
SIMPLE_TEST(test_type_parsing3, test_errcb,  "const void *", {
    token_next(cnm);
    typeref_t type = type_parse(cnm, NULL, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_PTR },
            (type_t){ .class = TYPE_VOID, .isconst = true },
        },
        .size = 2,
    })) return false;
    return true;
})
SIMPLE_TEST(test_type_parsing4, test_expect_errcb,  "int [static 3]", {
    token_next(cnm);
    type_parse(cnm, NULL, NULL);
    return test_expect_err;
})
SIMPLE_TEST(test_type_parsing5, test_errcb,  "const char *(* const)[][3]", {
    token_next(cnm);
    typeref_t type = type_parse(cnm, NULL, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_PTR, .isconst = true },
            (type_t){ .class = TYPE_ARR },
            (type_t){ .class = TYPE_ARR, .n = 3 },
            (type_t){ .class = TYPE_PTR },
            (type_t){ .class = TYPE_CHAR, .isconst = true },
        },
        .size = 5,
    })) return false;
    if (!type.type[0].isconst) return false;
    return true;
})
SIMPLE_TEST(test_type_parsing6, test_errcb,  "int *(*get_int)(char x[], bool z)", {
    token_next(cnm);
    typeref_t type = type_parse(cnm, NULL, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_PTR, .isconst = true },
            (type_t){ .class = TYPE_FN, .n = 2 },
            (type_t){ .class = TYPE_FN_ARG, .n = 2 },
            (type_t){ .class = TYPE_PTR },
            (type_t){ .class = TYPE_CHAR },
            (type_t){ .class = TYPE_FN_ARG, .n = 1 },
            (type_t){ .class = TYPE_BOOL, .isconst = true },
            (type_t){ .class = TYPE_PTR, .isconst = true },
            (type_t){ .class = TYPE_INT },
        },
        .size = 9,
    })) return false;
    return true;
})
SIMPLE_TEST(test_type_parsing7, test_errcb,  "int *(*get_int)(char x[], bool z)", {
    token_next(cnm);
    typeref_t type = type_parse(cnm, NULL, NULL);
    return !type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_PTR, .isconst = true },
            (type_t){ .class = TYPE_FN, .n = 2 },
            (type_t){ .class = TYPE_FN_ARG, .n = 2 },
            (type_t){ .class = TYPE_PTR },
            (type_t){ .class = TYPE_CHAR, .isconst = true },
            (type_t){ .class = TYPE_FN_ARG, .n = 1 },
            (type_t){ .class = TYPE_BOOL, .isconst = true },
            (type_t){ .class = TYPE_PTR, .isconst = true },
            (type_t){ .class = TYPE_INT },
        },
        .size = 9,
    });
})

///////////////////////////////////////////////////////////////////////////////
//
// Tester
//
///////////////////////////////////////////////////////////////////////////////
typedef bool (*test_pfn_t)(void);
typedef struct test_s {
    test_pfn_t pfn;
    const char *name;
} test_t;

// Helper macro for adding tests
#define TEST(func_name) ((test_t){ .pfn = func_name, .name = #func_name })

// Adds padding into the test output
#define TEST_PADDING ((test_t){ .pfn = NULL, .name = NULL })

// List of tester functions
static test_t tests[] = {
    // Lexer Tests
    TEST(test_lexer_uninitialized),
    TEST(test_lexer_ident),
    TEST(test_lexer_string1),
    TEST(test_lexer_string2),
    TEST(test_lexer_string3),
    TEST(test_lexer_string4),
    TEST(test_lexer_string5),
    TEST(test_lexer_string6),
    TEST(test_lexer_char1),
    TEST(test_lexer_char2),
    TEST(test_lexer_char3),
    TEST(test_lexer_char4),
    TEST(test_lexer_int1),
    TEST(test_lexer_int2),
    TEST(test_lexer_int3),
    TEST(test_lexer_int4),
    TEST(test_lexer_int5),
    TEST(test_lexer_int6),
    TEST(test_lexer_int7),
    TEST(test_lexer_double1),
    TEST(test_lexer_double2),
    TEST(test_lexer_double3),
    TEST(test_lexer_double4),
    TEST(test_lexer_double5),
    TEST(test_lexer_double6),
    TEST(test_lexer_double7),
    TEST(test_lexer_eof),
    TEST(test_lexer_dot),
    TEST(test_lexer_comma),
    TEST(test_lexer_cond),
    TEST(test_lexer_colon),
    TEST(test_lexer_semicolon),
    TEST(test_lexer_paren_l),
    TEST(test_lexer_paren_r),
    TEST(test_lexer_brack_l),
    TEST(test_lexer_brack_r),
    TEST(test_lexer_brace_l),
    TEST(test_lexer_brace_r),
    TEST(test_lexer_plus),
    TEST(test_lexer_plus_eq),
    TEST(test_lexer_plus_dbl),
    TEST(test_lexer_minus),
    TEST(test_lexer_minus_eq),
    TEST(test_lexer_minus_dbl),
    TEST(test_lexer_star),
    TEST(test_lexer_times_eq),
    TEST(test_lexer_divide),
    TEST(test_lexer_divide_eq),
    TEST(test_lexer_modulo),
    TEST(test_lexer_modulo_eq),
    TEST(test_lexer_bit_xor),
    TEST(test_lexer_bit_xor_eq),
    TEST(test_lexer_bit_not),
    TEST(test_lexer_bit_not_eq),
    TEST(test_lexer_not),
    TEST(test_lexer_not_eq),
    TEST(test_lexer_assign),
    TEST(test_lexer_eq_eq),
    TEST(test_lexer_bit_and),
    TEST(test_lexer_and_eq),
    TEST(test_lexer_and),
    TEST(test_lexer_bit_or),
    TEST(test_lexer_or_eq),
    TEST(test_lexer_or),
    TEST(test_lexer_less),
    TEST(test_lexer_less_eq),
    TEST(test_lexer_shift_l),
    TEST(test_lexer_shift_l_eq),
    TEST(test_lexer_greater),
    TEST(test_lexer_greater_eq),
    TEST(test_lexer_shift_r),
    TEST(test_lexer_shift_r_eq),
    TEST(test_lexer_token_location),

    // AST Generation Tests
    TEST_PADDING,
    TEST(test_ast_constant_folding1),
    TEST(test_ast_constant_folding2),
    TEST(test_ast_constant_folding3),
    TEST(test_ast_constant_folding4),
    TEST(test_ast_constant_folding5),
    TEST(test_ast_constant_folding6),
    TEST(test_ast_constant_folding7),
    TEST(test_ast_constant_folding8),
    TEST(test_ast_constant_folding9),
    TEST(test_ast_constant_folding10),
    TEST(test_ast_constant_folding11),
    TEST(test_ast_constant_folding12),
    TEST(test_ast_constant_folding13),
    TEST(test_ast_constant_folding14),
    TEST(test_ast_constant_folding15),
    TEST(test_ast_constant_folding16),
    TEST(test_ast_constant_folding17),
    TEST(test_ast_constant_folding18),
    TEST(test_ast_constant_folding19),
    TEST(test_ast_constant_folding20),
    TEST(test_ast_constant_folding21),
    TEST(test_ast_constant_folding22),
    TEST(test_ast_constant_folding23),
    TEST(test_ast_constant_folding24),
    TEST(test_ast_constant_folding25),

    // Type parsing tests
    TEST_PADDING,
    TEST(test_type_parsing1),
    TEST(test_type_parsing2),
    TEST(test_type_parsing_char),
    TEST(test_type_parsing_uchar),
    TEST(test_type_parsing_short1),
    TEST(test_type_parsing_short2),
    TEST(test_type_parsing_short3),
    TEST(test_type_parsing_short4),
    TEST(test_type_parsing_int),
    TEST(test_type_parsing_uint1),
    TEST(test_type_parsing_uint2),
    TEST(test_type_parsing_uint3),
    TEST(test_type_parsing_long1),
    TEST(test_type_parsing_long2),
    TEST(test_type_parsing_long3),
    TEST(test_type_parsing_long4),
    TEST(test_type_parsing_ulong),
    TEST(test_type_parsing_float),
    TEST(test_type_parsing_double),
    TEST(test_type_parsing3),
    TEST(test_type_parsing4),
    TEST(test_type_parsing5),
    TEST(test_type_parsing6),
    TEST(test_type_parsing7),
};

int main(int argc, char **argv) {
    printf("cnm tester\n");
   
    test_code_size = 2048;
    test_code_area = mmap(NULL, test_code_size, PROT_EXEC | PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANON, -1, 0);

    int passed = 0, ntests = 0;
    for (int i = 0; i < arrlen(tests); i++) {
        test_expect_err = false;
        if (!tests[i].name && !tests[i].pfn) {
            printf("\n");
            continue;
        }

        ntests++;
        printf("%-32s ", tests[i].name);
        if (tests[i].pfn()) {
            printf(" PASS \n");
            passed++;
        } else {
            printf("<FAIL>\n");
        }
    }

    printf("\n%d/%d tests passing\n", passed, ntests);

    munmap(test_code_area, test_code_size);

    return 0;
}

