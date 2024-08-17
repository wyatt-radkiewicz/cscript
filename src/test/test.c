#include <stdio.h>

#include <sys/mman.h>

#include "../cnm.c"

static uint8_t test_region[2048];
static uint8_t test_globals[2048];
static void *test_code_area;
static size_t test_code_size;

static void test_errcb(int line, const char *verbose, const char *simple) {
    printf("\n%s", verbose);
}
static bool test_expect_err = false;
static void test_expect_errcb(int line, const char *v, const char *s) {
    test_expect_err = true;
}

#define SIMPLE_TEST(_name, _errcb, _src) \
    static bool _name(void) { \
        cnm_t *cnm = cnm_init(test_region, sizeof(test_region), \
                              test_code_area, test_code_size, \
                              test_globals, sizeof(test_globals)); \
        cnm_set_errcb(cnm, _errcb); \
        cnm_set_src(cnm, _src, #_name);
static bool test_dofail(const char *file, int line) {
    int i = printf("\nfail at %s:%d:", file, line);
    for (; i < 34; i++) printf(" ");
    return false;
}
#define TESTFAIL (test_dofail(__FILE__, __LINE__))

///////////////////////////////////////////////////////////////////////////////
//
// Lexer testing
//
///////////////////////////////////////////////////////////////////////////////
SIMPLE_TEST(test_lexer_uninitialized, test_errcb, "")
    if (cnm->s.tok.type != TOKEN_UNINITIALIZED) return TESTFAIL;
    if (cnm->s.tok.start.row != 1) return TESTFAIL;
    if (cnm->s.tok.start.col != 1) return TESTFAIL;
    if (cnm->s.tok.end.row != 1) return TESTFAIL;
    if (cnm->s.tok.end.col != 1) return TESTFAIL;
    if (!strview_eq(cnm->s.tok.src, SV(""))) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_lexer_ident, test_errcb, " _foo123_ ")
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_IDENT) return TESTFAIL;
    if (cnm->s.tok.start.row != 1) return TESTFAIL;
    if (cnm->s.tok.start.col != 2) return TESTFAIL;
    if (cnm->s.tok.end.row != 1) return TESTFAIL;
    if (cnm->s.tok.end.col != 10) return TESTFAIL;
    if (!strview_eq(cnm->s.tok.src, SV("_foo123_"))) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_lexer_string1, test_errcb, "\"hello \"")
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_STRING) return TESTFAIL;
    if (cnm->s.tok.start.row != 1) return TESTFAIL;
    if (cnm->s.tok.start.col != 1) return TESTFAIL;
    if (cnm->s.tok.end.row != 1) return TESTFAIL;
    if (cnm->s.tok.end.col != 9) return TESTFAIL;
    if (!strview_eq(cnm->s.tok.src, SV("\"hello \""))) return TESTFAIL;
    if (strcmp(cnm->s.tok.s, "hello ") != 0) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_lexer_string2, test_errcb, "\"hello \"  \"world\"")
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_STRING) return TESTFAIL;
    if (cnm->s.tok.start.row != 1) return TESTFAIL;
    if (cnm->s.tok.start.col != 1) return TESTFAIL;
    if (cnm->s.tok.end.row != 1) return TESTFAIL;
    if (cnm->s.tok.end.col != 18) return TESTFAIL;
    if (!strview_eq(cnm->s.tok.src, SV("\"hello \"  \"world\""))) return TESTFAIL;
    if (strcmp(cnm->s.tok.s, "hello world") != 0) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_lexer_string3, test_errcb,
        "\"hello \"\n"
        "  \"world\"")
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_STRING) return TESTFAIL;
    if (cnm->s.tok.start.row != 1) return TESTFAIL;
    if (cnm->s.tok.start.col != 1) return TESTFAIL;
    if (cnm->s.tok.end.row != 2) return TESTFAIL;
    if (cnm->s.tok.end.col != 10) return TESTFAIL;
    if (!strview_eq(cnm->s.tok.src, SV("\"hello \"\n  \"world\""))) return TESTFAIL;
    if (strcmp(cnm->s.tok.s, "hello world") != 0) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_lexer_string4, test_errcb, "\"hello \\\"world\"")
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_STRING) return TESTFAIL;
    if (cnm->s.tok.start.row != 1) return TESTFAIL;
    if (cnm->s.tok.start.col != 1) return TESTFAIL;
    if (cnm->s.tok.end.row != 1) return TESTFAIL;
    if (cnm->s.tok.end.col != 16) return TESTFAIL;
    if (!strview_eq(cnm->s.tok.src, SV("\"hello \\\"world\""))) return TESTFAIL;
    if (strcmp(cnm->s.tok.s, "hello \"world") != 0) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_lexer_string5, test_errcb,  "\"hello \\u263A world\"")
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_STRING) return TESTFAIL;
    if (cnm->s.tok.start.row != 1) return TESTFAIL;
    if (cnm->s.tok.start.col != 1) return TESTFAIL;
    if (cnm->s.tok.end.row != 1) return TESTFAIL;
    if (cnm->s.tok.end.col != 21) return TESTFAIL;
    if (!strview_eq(cnm->s.tok.src, SV("\"hello \\u263A world\""))) return TESTFAIL;
    if (strcmp(cnm->s.tok.s, "hello ☺ world") != 0) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_lexer_string6, test_errcb,  "\"hello ☺ world\"")
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_STRING) return TESTFAIL;
    if (cnm->s.tok.start.row != 1) return TESTFAIL;
    if (cnm->s.tok.start.col != 1) return TESTFAIL;
    if (cnm->s.tok.end.row != 1) return TESTFAIL;
    if (cnm->s.tok.end.col != 16) return TESTFAIL;
    if (!strview_eq(cnm->s.tok.src, SV("\"hello ☺ world\""))) return TESTFAIL;
    if (strcmp(cnm->s.tok.s, "hello ☺ world") != 0) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_lexer_char1, test_errcb,  "'h'")
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_CHAR) return TESTFAIL;
    if (cnm->s.tok.start.row != 1) return TESTFAIL;
    if (cnm->s.tok.start.col != 1) return TESTFAIL;
    if (cnm->s.tok.end.row != 1) return TESTFAIL;
    if (cnm->s.tok.end.col != 4) return TESTFAIL;
    if (!strview_eq(cnm->s.tok.src, SV("'h'"))) return TESTFAIL;
    if (cnm->s.tok.c != 'h') return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_lexer_char2, test_errcb,  "'\\x41'")
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_CHAR) return TESTFAIL;
    if (cnm->s.tok.start.row != 1) return TESTFAIL;
    if (cnm->s.tok.start.col != 1) return TESTFAIL;
    if (cnm->s.tok.end.row != 1) return TESTFAIL;
    if (cnm->s.tok.end.col != 7) return TESTFAIL;
    if (!strview_eq(cnm->s.tok.src, SV("'\\x41'"))) return TESTFAIL;
    if (cnm->s.tok.c != 'A') return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_lexer_char3, test_errcb,  "'☺'")
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_CHAR) return TESTFAIL;
    if (cnm->s.tok.start.row != 1) return TESTFAIL;
    if (cnm->s.tok.start.col != 1) return TESTFAIL;
    if (cnm->s.tok.end.row != 1) return TESTFAIL;
    if (cnm->s.tok.end.col != 4) return TESTFAIL;
    if (!strview_eq(cnm->s.tok.src, SV("'☺'"))) return TESTFAIL;
    if (cnm->s.tok.c != 0x263A) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_lexer_char4, test_expect_errcb,  "'h '")
    token_next(cnm);
    return test_expect_err;
}
SIMPLE_TEST(test_lexer_int1, test_errcb,  "120")
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_INT) return TESTFAIL;
    if (cnm->s.tok.start.row != 1) return TESTFAIL;
    if (cnm->s.tok.start.col != 1) return TESTFAIL;
    if (cnm->s.tok.end.row != 1) return TESTFAIL;
    if (cnm->s.tok.end.col != 4) return TESTFAIL;
    if (!strview_eq(cnm->s.tok.src, SV("120"))) return TESTFAIL;
    if (cnm->s.tok.i.base != 10) return TESTFAIL;
    if (cnm->s.tok.i.n != 120) return TESTFAIL;
    if (strcmp(cnm->s.tok.i.suffix, "") != 0) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_lexer_int2, test_errcb,  "69U")
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_INT) return TESTFAIL;
    if (cnm->s.tok.start.row != 1) return TESTFAIL;
    if (cnm->s.tok.start.col != 1) return TESTFAIL;
    if (cnm->s.tok.end.row != 1) return TESTFAIL;
    if (cnm->s.tok.end.col != 4) return TESTFAIL;
    if (!strview_eq(cnm->s.tok.src, SV("69U"))) return TESTFAIL;
    if (cnm->s.tok.i.base != 10) return TESTFAIL;
    if (cnm->s.tok.i.n != 69) return TESTFAIL;
    if (strcmp(cnm->s.tok.i.suffix, "u") != 0) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_lexer_int3, test_errcb,  "420ulL")
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_INT) return TESTFAIL;
    if (cnm->s.tok.start.row != 1) return TESTFAIL;
    if (cnm->s.tok.start.col != 1) return TESTFAIL;
    if (cnm->s.tok.end.row != 1) return TESTFAIL;
    if (cnm->s.tok.end.col != 7) return TESTFAIL;
    if (!strview_eq(cnm->s.tok.src, SV("420ulL"))) return TESTFAIL;
    if (cnm->s.tok.i.base != 10) return TESTFAIL;
    if (cnm->s.tok.i.n != 420) return TESTFAIL;
    if (strcmp(cnm->s.tok.i.suffix, "ull") != 0) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_lexer_int4, test_errcb,  "0xFF")
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_INT) return TESTFAIL;
    if (cnm->s.tok.start.row != 1) return TESTFAIL;
    if (cnm->s.tok.start.col != 1) return TESTFAIL;
    if (cnm->s.tok.end.row != 1) return TESTFAIL;
    if (cnm->s.tok.end.col != 5) return TESTFAIL;
    if (!strview_eq(cnm->s.tok.src, SV("0xFF"))) return TESTFAIL;
    if (cnm->s.tok.i.base != 16) return TESTFAIL;
    if (cnm->s.tok.i.n != 255) return TESTFAIL;
    if (strcmp(cnm->s.tok.i.suffix, "") != 0) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_lexer_int5, test_expect_errcb,  "0xxF")
    token_next(cnm);
    return test_expect_err;
}
SIMPLE_TEST(test_lexer_int6, test_expect_errcb,  "420f")
    token_next(cnm);
    return test_expect_err;
}
SIMPLE_TEST(test_lexer_int7, test_errcb,  "0b1101")
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_INT) return TESTFAIL;
    if (cnm->s.tok.start.row != 1) return TESTFAIL;
    if (cnm->s.tok.start.col != 1) return TESTFAIL;
    if (cnm->s.tok.end.row != 1) return TESTFAIL;
    if (cnm->s.tok.end.col != 7) return TESTFAIL;
    if (!strview_eq(cnm->s.tok.src, SV("0b1101"))) return TESTFAIL;
    if (cnm->s.tok.i.base != 2) return TESTFAIL;
    if (cnm->s.tok.i.n != 13) return TESTFAIL;
    if (strcmp(cnm->s.tok.i.suffix, "") != 0) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_lexer_double1, test_errcb,  "0.0")
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_DOUBLE) return TESTFAIL;
    if (cnm->s.tok.start.row != 1) return TESTFAIL;
    if (cnm->s.tok.start.col != 1) return TESTFAIL;
    if (cnm->s.tok.end.row != 1) return TESTFAIL;
    if (cnm->s.tok.end.col != 4) return TESTFAIL;
    if (!strview_eq(cnm->s.tok.src, SV("0.0"))) return TESTFAIL;
    if (cnm->s.tok.f.n != 0.0) return TESTFAIL;
    if (strcmp(cnm->s.tok.f.suffix, "") != 0) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_lexer_double2, test_errcb,  "0.5")
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_DOUBLE) return TESTFAIL;
    if (cnm->s.tok.start.row != 1) return TESTFAIL;
    if (cnm->s.tok.start.col != 1) return TESTFAIL;
    if (cnm->s.tok.end.row != 1) return TESTFAIL;
    if (cnm->s.tok.end.col != 4) return TESTFAIL;
    if (!strview_eq(cnm->s.tok.src, SV("0.5"))) return TESTFAIL;
    if (cnm->s.tok.f.n != 0.5) return TESTFAIL;
    if (strcmp(cnm->s.tok.f.suffix, "") != 0) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_lexer_double3, test_errcb,  "420.69")
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_DOUBLE) return TESTFAIL;
    if (cnm->s.tok.start.row != 1) return TESTFAIL;
    if (cnm->s.tok.start.col != 1) return TESTFAIL;
    if (cnm->s.tok.end.row != 1) return TESTFAIL;
    if (cnm->s.tok.end.col != 7) return TESTFAIL;
    if (!strview_eq(cnm->s.tok.src, SV("420.69"))) return TESTFAIL;
    if (cnm->s.tok.f.n != 420.69) return TESTFAIL;
    if (strcmp(cnm->s.tok.f.suffix, "") != 0) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_lexer_double4, test_errcb,  "420.69f")
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_DOUBLE) return TESTFAIL;
    if (cnm->s.tok.start.row != 1) return TESTFAIL;
    if (cnm->s.tok.start.col != 1) return TESTFAIL;
    if (cnm->s.tok.end.row != 1) return TESTFAIL;
    if (cnm->s.tok.end.col != 8) return TESTFAIL;
    if (!strview_eq(cnm->s.tok.src, SV("420.69f"))) return TESTFAIL;
    if (cnm->s.tok.f.n != 420.69) return TESTFAIL;
    if (strcmp(cnm->s.tok.f.suffix, "f") != 0) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_lexer_double5, test_errcb,  "42.")
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_DOUBLE) return TESTFAIL;
    if (cnm->s.tok.start.row != 1) return TESTFAIL;
    if (cnm->s.tok.start.col != 1) return TESTFAIL;
    if (cnm->s.tok.end.row != 1) return TESTFAIL;
    if (cnm->s.tok.end.col != 4) return TESTFAIL;
    if (!strview_eq(cnm->s.tok.src, SV("42."))) return TESTFAIL;
    if (cnm->s.tok.f.n != 42.0) return TESTFAIL;
    if (strcmp(cnm->s.tok.f.suffix, "") != 0) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_lexer_double6, test_errcb,  "42.f")
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_DOUBLE) return TESTFAIL;
    if (cnm->s.tok.start.row != 1) return TESTFAIL;
    if (cnm->s.tok.start.col != 1) return TESTFAIL;
    if (cnm->s.tok.end.row != 1) return TESTFAIL;
    if (cnm->s.tok.end.col != 5) return TESTFAIL;
    if (!strview_eq(cnm->s.tok.src, SV("42.f"))) return TESTFAIL;
    if (cnm->s.tok.f.n != 42.0) return TESTFAIL;
    if (strcmp(cnm->s.tok.f.suffix, "f") != 0) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_lexer_double7, test_expect_errcb,  "0.0asdf")
    token_next(cnm);
    return test_expect_err;
}

#define TEST_LEXER_TOKEN(name_end, string, token_type) \
    static bool test_lexer_##name_end(void) { \
        cnm_t *cnm = cnm_init(test_region, sizeof(test_region), \
                              test_code_area, test_code_size, \
                              test_globals, sizeof(test_globals)); \
        cnm_set_errcb(cnm, test_errcb); \
        cnm_set_src(cnm, string, "test_lexer_"#name_end".cnm"); \
        token_next(cnm); \
        if (cnm->s.tok.type != token_type) return TESTFAIL; \
        if (cnm->s.tok.start.row != 1) return TESTFAIL; \
        if (cnm->s.tok.start.col != 1) return TESTFAIL; \
        if (cnm->s.tok.end.row != 1) return TESTFAIL; \
        if (cnm->s.tok.end.col != sizeof(string)) return TESTFAIL; \
        if (!strview_eq(cnm->s.tok.src, SV(string))) return TESTFAIL; \
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
        "foo \"str\" \"ing\" 'A' 34u 0.0f")
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
            return TESTFAIL;
        }
    }

    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_IDENT || cnm->s.tok.start.row != 5
        || cnm->s.tok.start.col != 1   || cnm->s.tok.end.row != 5
        || cnm->s.tok.end.col != 4     || cnm->s.tok.src.len != 3) return TESTFAIL;
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_STRING || cnm->s.tok.start.row != 5
        || cnm->s.tok.start.col != 5   || cnm->s.tok.end.row != 5
        || cnm->s.tok.end.col != 16    || cnm->s.tok.src.len != 11) return TESTFAIL;
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_CHAR || cnm->s.tok.start.row != 5
        || cnm->s.tok.start.col != 17  || cnm->s.tok.end.row != 5
        || cnm->s.tok.end.col != 20    || cnm->s.tok.src.len != 3) return TESTFAIL;
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_INT || cnm->s.tok.start.row != 5
        || cnm->s.tok.start.col != 21  || cnm->s.tok.end.row != 5
        || cnm->s.tok.end.col != 24    || cnm->s.tok.src.len != 3) return TESTFAIL;
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_DOUBLE || cnm->s.tok.start.row != 5
        || cnm->s.tok.start.col != 25  || cnm->s.tok.end.row != 5
        || cnm->s.tok.end.col != 29    || cnm->s.tok.src.len != 4) return TESTFAIL;
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_EOF || cnm->s.tok.start.row != 5
        || cnm->s.tok.start.col != 29  || cnm->s.tok.end.row != 5
        || cnm->s.tok.end.col != 29    || cnm->s.tok.src.len != 0) return TESTFAIL;

    return true;
}

///////////////////////////////////////////////////////////////////////////////
//
// Expression Constant Folding Testing
//
///////////////////////////////////////////////////////////////////////////////
SIMPLE_TEST(test_expr_constant_folding1, test_errcb,  "5")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_INT) return TESTFAIL;
    if (val.type.size != 1) return TESTFAIL;
    if (val.literal.num.i != 5) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding2, test_errcb,  "5.0")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_DOUBLE) return TESTFAIL;
    if (val.literal.num.f != 5.0) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding3, test_errcb,  "5000000000")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_LONG) return TESTFAIL;
    if (val.literal.num.i != 5000000000) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding4, test_errcb,  "5u")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_UINT) return TESTFAIL;
    if (val.literal.num.u != 5) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding5, test_errcb,  "9223372036854775809ull")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_ULLONG) return TESTFAIL;
    if (val.literal.num.u != 9223372036854775809ull) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding6, test_expect_errcb,  "9223372036854775809")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL)) return TESTFAIL;
    return test_expect_err;
}
SIMPLE_TEST(test_expr_constant_folding7, test_errcb,  "92ull")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_ULLONG) return TESTFAIL;
    if (val.literal.num.u != 92ull) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding8, test_errcb,  "5 + 5")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_INT) return TESTFAIL;
    if (val.literal.num.i != 10) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding9, test_errcb,  "5 * 5 + 3")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_INT) return TESTFAIL;
    if (val.literal.num.i != 28) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding10, test_errcb,  "5 + 5 * 3")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_INT) return TESTFAIL;
    if (val.literal.num.i != 20) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding11, test_errcb,  "5ul + 30.0")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_DOUBLE) return TESTFAIL;
    if (val.literal.num.f != 35) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding12, test_errcb,  "10 / 3.0")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_DOUBLE) return TESTFAIL;
    if (val.literal.num.f != 10.0 / 3.0) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding13, test_errcb,  "10 / 3")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_INT) return TESTFAIL;
    if (val.literal.num.i != 10 / 3) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding14, test_errcb,  "10ul * 12")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_ULONG) return TESTFAIL;
    if (val.literal.num.u != 120) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding15, test_errcb,  "3 - 9 * 2l")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_LONG) return TESTFAIL;
    if (val.literal.num.i != -15) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding16, test_errcb,  "-8")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_INT) return TESTFAIL;
    if (val.literal.num.i != -8) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding17, test_errcb,  "(-8)")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_INT) return TESTFAIL;
    if (val.literal.num.i != -8) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding18, test_errcb,  "-(0x8)")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_INT) return TESTFAIL;
    if (val.literal.num.i != -8) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding19, test_errcb,  "~0x8u")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_UINT) return TESTFAIL;
    if (val.literal.num.u != ~0x8u) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding20, test_expect_errcb,  "~(1 + 0.9)")
    token_next(cnm);
    valref_t val;
    if (expr_parse(cnm, &val, false, false, PREC_FULL)) return TESTFAIL;
    return test_expect_err;
}
SIMPLE_TEST(test_expr_constant_folding21, test_errcb,  "!1")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_BOOL) return TESTFAIL;
    if (val.literal.num.u != false) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding22, test_errcb,  "!0.0")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_BOOL) return TESTFAIL;
    if (val.literal.num.u != true) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding23, test_errcb,  "1 << 4")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_INT) return TESTFAIL;
    if (val.literal.num.i != 16) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding24, test_errcb,  "0xff | 0xff << 8")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_INT) return TESTFAIL;
    if (val.literal.num.i != 0xffff) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding25, test_errcb,  "-(5 + 10) * 2 + 60 >> 2")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_INT) return TESTFAIL;
    if (val.literal.num.i != (-(5 + 10) * 2 + 60) >> 2) return TESTFAIL;
    return true;
}

///////////////////////////////////////////////////////////////////////////////
//
// Type parsing tests
//
///////////////////////////////////////////////////////////////////////////////
SIMPLE_TEST(test_type_parsing1, test_errcb,  "const char *foo")
    token_next(cnm);
    bool istypedef;
    strview_t name;
    type_t base;
    if (!type_parse_declspec(cnm, &base, &istypedef)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, &name);
    if (istypedef) return TESTFAIL;
    if (!strview_eq(name, SV("foo"))) return TESTFAIL;
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_PTR },
            (type_t){ .class = TYPE_CHAR, .isconst = true },
        },
        .size = 2,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing2, test_errcb,  "typedef unsigned char u8")
    token_next(cnm);
    bool istypedef;
    strview_t name;
    type_t base;
    if (!type_parse_declspec(cnm, &base, &istypedef)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, &name);
    if (!istypedef) return TESTFAIL;
    if (!strview_eq(name, SV("u8"))) return TESTFAIL;
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_UCHAR },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_char, test_errcb,  "char")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_CHAR },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_uchar, test_errcb,  "unsigned char")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_UCHAR },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_short1, test_errcb,  "short")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_SHORT },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_short2, test_errcb,  "short int")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_SHORT },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_short3, test_expect_errcb,  "short short int")
    token_next(cnm);
    type_t base;
    if (type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    type_parse(cnm, &base, NULL);
    return test_expect_err;
}
SIMPLE_TEST(test_type_parsing_short4, test_expect_errcb,  "short char")
    token_next(cnm);
    type_t base;
    if (type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    type_parse(cnm, &base, NULL);
    return test_expect_err;
}
SIMPLE_TEST(test_type_parsing_ushort1, test_errcb,  "unsigned short")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_USHORT },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_ushort2, test_errcb,  "unsigned short int")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_USHORT },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_ushort3, test_expect_errcb,  "unsigned unsigned short")
    token_next(cnm);
    type_t base;
    if (type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    type_parse(cnm, &base, NULL);
    return test_expect_err;
}
SIMPLE_TEST(test_type_parsing_int, test_errcb,  "int")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_uint1, test_errcb,  "unsigned int")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_UINT },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_uint2, test_errcb,  "unsigned")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_UINT },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_uint3, test_errcb,  "const unsigned")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_UINT, .isconst = true },
        },
        .size = 1,
    }, true)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_long1, test_errcb,  "long")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_LONG },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_long2, test_errcb,  "long long")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_LLONG },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_long3, test_errcb,  "long int")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_LONG },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_long4, test_errcb,  "long long int")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_LLONG },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_ulong, test_errcb,  "unsigned long long")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_ULLONG },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_float, test_errcb,  "float")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_FLOAT },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_double, test_errcb,  "double")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_DOUBLE, .isconst = true },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing3, test_errcb,  "const void *")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_PTR },
            (type_t){ .class = TYPE_VOID, .isconst = true },
        },
        .size = 2,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing4, test_expect_errcb,  "int [static 3]")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    type_parse(cnm, &base, NULL);
    return test_expect_err;
}
SIMPLE_TEST(test_type_parsing5, test_errcb,  "const char *(* const)[][3]")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_PTR, .isconst = true },
            (type_t){ .class = TYPE_ARR },
            (type_t){ .class = TYPE_ARR, .n = 3 },
            (type_t){ .class = TYPE_PTR },
            (type_t){ .class = TYPE_CHAR, .isconst = true },
        },
        .size = 5,
    }, true)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing6, test_errcb,  "int *(*get_int)(char x[], bool z)")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_PTR, .isconst = true },
            (type_t){ .class = TYPE_FN, .n = 2 },
            (type_t){ .class = TYPE_FN_ARG, .n = 2 },
            (type_t){ .class = TYPE_PTR },
            (type_t){ .class = TYPE_CHAR },
            (type_t){ .class = TYPE_FN_ARG, .n = 1 },
            (type_t){ .class = TYPE_BOOL, .isconst = true },
            (type_t){ .class = TYPE_PTR },
            (type_t){ .class = TYPE_INT },
        },
        .size = 9,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing7, test_errcb,  "int *(*get_int)(char x[], bool z)")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL);
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
    }, false);
}
SIMPLE_TEST(test_type_parsing8, test_errcb,  "double")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL);
    return !type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_DOUBLE, .isconst = true },
        },
        .size = 1,
    }, true);
}
SIMPLE_TEST(test_type_parsing9, test_errcb,  "int a, *b, *c[2]")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    strview_t name;

    // a
    typeref_t type = type_parse(cnm, &base, &name);
    if (!strview_eq(name, SV("a"))) return TESTFAIL;
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT },
        },
        .size = 1,
    }, false)) return TESTFAIL;

    if (cnm->s.tok.type != TOKEN_COMMA) return TESTFAIL;
    token_next(cnm);

    // b
    type = type_parse(cnm, &base, &name);
    if (!strview_eq(name, SV("b"))) return TESTFAIL;
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_PTR },
            (type_t){ .class = TYPE_INT },
        },
        .size = 2,
    }, false)) return TESTFAIL;

    if (cnm->s.tok.type != TOKEN_COMMA) return TESTFAIL;
    token_next(cnm);

    // c
    type = type_parse(cnm, &base, &name);
    if (!strview_eq(name, SV("c"))) return TESTFAIL;
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_ARR, .n = 2 },
            (type_t){ .class = TYPE_PTR },
            (type_t){ .class = TYPE_INT },
        },
        .size = 3,
    }, false)) return TESTFAIL;

    return true;
}

///////////////////////////////////////////////////////////////////////////////
//
// Type definition parsing
//
///////////////////////////////////////////////////////////////////////////////
SIMPLE_TEST(test_typedef_parsing1, test_errcb,
        "struct foo {\n"
        "    int a;\n"
        "    int b;\n"
        "} *bar[4]\n")
    token_next(cnm);
    strview_t name;
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t ref = type_parse(cnm, &base, &name);
    if (!strview_eq(name, SV("bar"))) return TESTFAIL;
    if (!type_eq(ref, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_ARR, .n = 4 },
            (type_t){ .class = TYPE_PTR },
            (type_t){ .class = TYPE_USER, .n = 0 },
        },
        .size = 3,
    }, false)) return TESTFAIL;

    // struct foo
    userty_t *t = cnm->type.types;
    struct_t *s = (struct_t *)t->data;
    if (!t) return TESTFAIL;
    if (t->inf.size != sizeof(int) * 2) return TESTFAIL;
    if (t->inf.align != sizeof(int)) return TESTFAIL;
    if (!strview_eq(t->name, SV("foo"))) return TESTFAIL;
    if (t->typeid != 0) return TESTFAIL;
   
    // foo::b
    field_t *f = s->fields;
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("b"))) return TESTFAIL;
    if (f->offs != 4) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    f = f->next;

    // foo::b
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("a"))) return TESTFAIL;
    if (f->offs != 0) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT },
        },
        .size = 1,
    }, false)) return TESTFAIL;

    return true;
}
SIMPLE_TEST(test_typedef_parsing2, test_errcb,
        "struct baz {\n"
        "    char a;\n"
        "    double b;\n"
        "    int c;\n"
        "} FSN__FHA__FZR__FEX__FGO\n")
    token_next(cnm);
    strview_t name;
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t ref = type_parse(cnm, &base, &name);
    if (!strview_eq(name, SV("FSN__FHA__FZR__FEX__FGO"))) return TESTFAIL;
    if (!type_eq(ref, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_USER, .n = 0 },
        },
        .size = 1,
    }, false)) return TESTFAIL;

    // struct baz
    userty_t *t = cnm->type.types;
    struct_t *s = (struct_t *)t->data;
    if (!t) return TESTFAIL;
    if (t->inf.size != sizeof(double) * 3) return TESTFAIL;
    if (t->inf.align != sizeof(double)) return TESTFAIL;
    if (!strview_eq(t->name, SV("baz"))) return TESTFAIL;
    if (t->typeid != 0) return TESTFAIL;
   
    // foo::c
    field_t *f = s->fields;
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("c"))) return TESTFAIL;
    if (f->offs != 16) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    f = f->next;

    // foo::b
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("b"))) return TESTFAIL;
    if (f->offs != 8) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_DOUBLE },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    f = f->next;

    // foo::a
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("a"))) return TESTFAIL;
    if (f->offs != 0) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_CHAR },
        },
        .size = 1,
    }, false)) return TESTFAIL;

    return true;
}
SIMPLE_TEST(test_typedef_parsing3, test_errcb,
        "struct SNU {\n"
        "    int a;\n"
        "    struct {\n"
        "        int foo;\n"
        "        char baz[3];\n"
        "    } b;\n"
        "    char c;\n"
        "} EBE \n")
    token_next(cnm);
    strview_t name;
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t ref = type_parse(cnm, &base, &name);
    if (!strview_eq(name, SV("EBE"))) return TESTFAIL;
    if (!type_eq(ref, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_USER, .n = 0 },
        },
        .size = 1,
    }, false)) return TESTFAIL;

    // struct "anonymous"
    userty_t *t = cnm->type.types;
    if (!t) return TESTFAIL;
    struct_t *s = (struct_t *)t->data;
    field_t *f = s->fields;
    if (t->inf.size != 8) return TESTFAIL;
    if (t->inf.align != 4) return TESTFAIL;
    if (t->name.str) return TESTFAIL;
    if (t->typeid != 1) return TESTFAIL;

    // foo::b::baz
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("baz"))) return TESTFAIL;
    if (f->offs != 4) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_ARR, .n = 3 },
            (type_t){ .class = TYPE_CHAR },
        },
        .size = 2,
    }, false)) return TESTFAIL;
    f = f->next;

    // foo::b::foo
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("foo"))) return TESTFAIL;
    if (f->offs != 0) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    f = f->next;

    // Goto next struct
    t = t->next;
    if (!t) return TESTFAIL;
    s = (struct_t *)t->data;
    f = s->fields;

    // struct SNU
    if (t->inf.size != 16) return TESTFAIL;
    if (t->inf.align != 4) return TESTFAIL;
    if (!strview_eq(t->name, SV("SNU"))) return TESTFAIL;
    if (t->typeid != 0) return TESTFAIL;
   
    // foo::c
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("c"))) return TESTFAIL;
    if (f->offs != 12) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_CHAR },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    f = f->next;

    // foo::b
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("b"))) return TESTFAIL;
    if (f->offs != 4) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_USER, .n = 1 },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    f = f->next;

    // foo::a
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("a"))) return TESTFAIL;
    if (f->offs != 0) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT },
        },
        .size = 1,
    }, false)) return TESTFAIL;

    return true;
}
SIMPLE_TEST(test_typedef_parsing4, test_errcb,
        "union SNU {\n"
        "    int a;\n"
        "    struct {\n"
        "        int foo;\n"
        "        char baz[3];\n"
        "    } b;\n"
        "    char c;\n"
        "} EBE \n")
    token_next(cnm);
    strview_t name;
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t ref = type_parse(cnm, &base, &name);
    if (!strview_eq(name, SV("EBE"))) return TESTFAIL;
    if (!type_eq(ref, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_USER, .n = 0 },
        },
        .size = 1,
    }, false)) return TESTFAIL;

    // union SNU
    userty_t *t = cnm->type.types->next;
    if (!t) return TESTFAIL;
    if (t->inf.size != 8) return TESTFAIL;
    if (t->inf.align != 4) return TESTFAIL;
    if (!strview_eq(t->name, SV("SNU"))) return TESTFAIL;
    if (t->typeid != 0) return TESTFAIL;

    return true;
}
SIMPLE_TEST(test_typedef_parsing5, test_errcb,
        "enum E {\n"
        "    FOO,\n"
        "    BAR = 5 + 5,\n"
        "    BAZ\n"
        "} subihibi \n")
    token_next(cnm);
    strview_t name;
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t ref = type_parse(cnm, &base, &name);
    if (!strview_eq(name, SV("subihibi"))) return TESTFAIL;
    if (!type_eq(ref, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_USER, .n = 0 },
        },
        .size = 1,
    }, false)) return TESTFAIL;

    // enum SNU
    userty_t *t = cnm->type.types;
    if (!t) return TESTFAIL;
    enum_t *e = (enum_t *)t->data;
    if (t->inf.size != 4) return TESTFAIL;
    if (t->inf.align != 4) return TESTFAIL;
    if (!strview_eq(t->name, SV("E"))) return TESTFAIL;
    if (t->typeid != 0) return TESTFAIL;

    if (e->nvariants != 3) return TESTFAIL;
    if (e->variants[2].id.i != 0) return TESTFAIL;
    if (!strview_eq(e->variants[2].name, SV("FOO"))) return TESTFAIL;

    if (e->variants[1].id.i != 10) return TESTFAIL;
    if (!strview_eq(e->variants[1].name, SV("BAR"))) return TESTFAIL;

    if (e->variants[0].id.i != 11) return TESTFAIL;
    if (!strview_eq(e->variants[0].name, SV("BAZ"))) return TESTFAIL;

    return true;
}
SIMPLE_TEST(test_typedef_parsing6, test_errcb,
        "enum E : unsigned int {\n"
        "    FOO,\n"
        "    BAR,\n"
        "    BAZ,\n"
        "} subihibi \n")
    token_next(cnm);
    strview_t name;
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t ref = type_parse(cnm, &base, &name);
    if (!strview_eq(name, SV("subihibi"))) return TESTFAIL;
    if (!type_eq(ref, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_USER, .n = 0 },
        },
        .size = 1,
    }, false)) return TESTFAIL;

    // enum SNU
    userty_t *t = cnm->type.types;
    if (!t) return TESTFAIL;
    enum_t *e = (enum_t *)t->data;
    if (t->inf.size != 4) return TESTFAIL;
    if (t->inf.align != 4) return TESTFAIL;
    if (!type_eq((typeref_t){ .type = &e->type, .size = 1 }, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_UINT },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    if (!strview_eq(t->name, SV("E"))) return TESTFAIL;
    if (t->typeid != 0) return TESTFAIL;

    if (e->nvariants != 3) return TESTFAIL;
    if (e->variants[2].id.i != 0) return TESTFAIL;
    if (!strview_eq(e->variants[2].name, SV("FOO"))) return TESTFAIL;

    if (e->variants[1].id.i != 1) return TESTFAIL;
    if (!strview_eq(e->variants[1].name, SV("BAR"))) return TESTFAIL;

    if (e->variants[0].id.i != 2) return TESTFAIL;
    if (!strview_eq(e->variants[0].name, SV("BAZ"))) return TESTFAIL;

    return true;
}
SIMPLE_TEST(test_typedef_parsing7, test_errcb,
        "struct SNU {\n"
        "    int a;\n"
        "    struct {\n"
        "        int foo;\n"
        "        char baz[3];\n"
        "    };\n"
        "    char;\n"
        "} EBE \n")
    token_next(cnm);
    strview_t name;
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t ref = type_parse(cnm, &base, &name);
    if (!strview_eq(name, SV("EBE"))) return TESTFAIL;
    if (!type_eq(ref, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_USER, .n = 0 },
        },
        .size = 1,
    }, false)) return TESTFAIL;

    // struct "anonymous"
    userty_t *t = cnm->type.types;
    if (!t) return TESTFAIL;
    struct_t *s = (struct_t *)t->data;
    field_t *f = s->fields;
    if (t->inf.size != 8) return TESTFAIL;
    if (t->inf.align != 4) return TESTFAIL;
    if (t->name.str) return TESTFAIL;
    if (t->typeid != 1) return TESTFAIL;

    // foo::b::baz
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("baz"))) return TESTFAIL;
    if (f->offs != 4) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_ARR, .n = 3 },
            (type_t){ .class = TYPE_CHAR },
        },
        .size = 2,
    }, false)) return TESTFAIL;
    f = f->next;

    // foo::b::foo
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("foo"))) return TESTFAIL;
    if (f->offs != 0) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    f = f->next;

    // Goto next struct
    t = t->next;
    if (!t) return TESTFAIL;
    s = (struct_t *)t->data;
    f = s->fields;

    // struct SNU
    if (t->inf.size != 16) return TESTFAIL;
    if (t->inf.align != 4) return TESTFAIL;
    if (!strview_eq(t->name, SV("SNU"))) return TESTFAIL;
    if (t->typeid != 0) return TESTFAIL;
   
    // foo::c
    if (!f) return TESTFAIL;
    if (f->name.str) return TESTFAIL;
    if (f->offs != 12) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_CHAR },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    f = f->next;

    // foo::b
    if (!f) return TESTFAIL;
    if (f->name.str) return TESTFAIL;
    if (f->offs != 4) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_USER, .n = 1 },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    f = f->next;

    // foo::a
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("a"))) return TESTFAIL;
    if (f->offs != 0) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT },
        },
        .size = 1,
    }, false)) return TESTFAIL;

    return true;
}
SIMPLE_TEST(test_typedef_parsing8, test_expect_errcb,
        "struct SNU {\n"
        "} EBE \n")
    token_next(cnm);
    type_t base;
    if (type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    type_parse(cnm, &base, NULL);
    return test_expect_err;
}
SIMPLE_TEST(test_typedef_parsing9, test_expect_errcb,
        "enum SNU {\n"
        "} EBE \n")
    token_next(cnm);
    type_t base;
    if (type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    type_parse(cnm, &base, NULL);
    return test_expect_err;
}
SIMPLE_TEST(test_typedef_parsing10, test_expect_errcb,
        "union SNU {\n"
        "} EBE \n")
    token_next(cnm);
    type_t base;
    if (type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    type_parse(cnm, &base, NULL);
    return test_expect_err;
}
SIMPLE_TEST(test_typedef_parsing11, test_errcb,
        "struct foo {\n"
        "    int a, b;\n"
        "}\n")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t ref = type_parse(cnm, &base, NULL);
    if (!type_eq(ref, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_USER, .n = 0 },
        },
        .size = 1,
    }, false)) return TESTFAIL;

    // struct foo
    userty_t *t = cnm->type.types;
    struct_t *s = (struct_t *)t->data;
    if (!t) return TESTFAIL;
    if (t->inf.size != sizeof(int) * 2) return TESTFAIL;
    if (t->inf.align != sizeof(int)) return TESTFAIL;
    if (!strview_eq(t->name, SV("foo"))) return TESTFAIL;
    if (t->typeid != 0) return TESTFAIL;
   
    // foo::b
    field_t *f = s->fields;
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("b"))) return TESTFAIL;
    if (f->offs != 4) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    f = f->next;

    // foo::a
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("a"))) return TESTFAIL;
    if (f->offs != 0) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT },
        },
        .size = 1,
    }, false)) return TESTFAIL;

    return true;
}
SIMPLE_TEST(test_typedef_parsing12, test_errcb,
        "union foo {\n"
        "    int a, b;\n"
        "}\n")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t ref = type_parse(cnm, &base, NULL);
    if (!type_eq(ref, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_USER, .n = 0 },
        },
        .size = 1,
    }, false)) return TESTFAIL;

    // union foo
    userty_t *t = cnm->type.types;
    if (!t) return TESTFAIL;
    if (t->inf.size != sizeof(int)) return TESTFAIL;
    if (t->inf.align != sizeof(int)) return TESTFAIL;
    if (!strview_eq(t->name, SV("foo"))) return TESTFAIL;
    if (t->typeid != 0) return TESTFAIL;

    return true;
}

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

    // Expression Generation Tests
    TEST_PADDING,
    TEST(test_expr_constant_folding1),
    TEST(test_expr_constant_folding2),
    TEST(test_expr_constant_folding3),
    TEST(test_expr_constant_folding4),
    TEST(test_expr_constant_folding5),
    TEST(test_expr_constant_folding6),
    TEST(test_expr_constant_folding7),
    TEST(test_expr_constant_folding8),
    TEST(test_expr_constant_folding9),
    TEST(test_expr_constant_folding10),
    TEST(test_expr_constant_folding11),
    TEST(test_expr_constant_folding12),
    TEST(test_expr_constant_folding13),
    TEST(test_expr_constant_folding14),
    TEST(test_expr_constant_folding15),
    TEST(test_expr_constant_folding16),
    TEST(test_expr_constant_folding17),
    TEST(test_expr_constant_folding18),
    TEST(test_expr_constant_folding19),
    TEST(test_expr_constant_folding20),
    TEST(test_expr_constant_folding21),
    TEST(test_expr_constant_folding22),
    TEST(test_expr_constant_folding23),
    TEST(test_expr_constant_folding24),
    TEST(test_expr_constant_folding25),

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
    TEST(test_type_parsing8),
    TEST(test_type_parsing9),

    // Type parsing tests
    TEST_PADDING,
    TEST(test_typedef_parsing1),
    TEST(test_typedef_parsing2),
    TEST(test_typedef_parsing3),
    TEST(test_typedef_parsing4),
    TEST(test_typedef_parsing5),
    TEST(test_typedef_parsing6),
    TEST(test_typedef_parsing7),
    TEST(test_typedef_parsing8),
    TEST(test_typedef_parsing9),
    TEST(test_typedef_parsing10),
    TEST(test_typedef_parsing11),
    TEST(test_typedef_parsing12),
};

int main(int argc, char **argv) {
    printf("cnm tester\n");
   
    test_code_size = 2048;
    test_code_area = mmap(NULL, test_code_size, PROT_EXEC | PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

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
            printf(" \033[42m\033[1mPASS\033[0m \n");
            passed++;
        } else {
            printf("\033[41m\033[1m<FAIL>\033[0m\n");
        }
    }

    printf("\n%d/%d tests passing (%d%%)\n", passed, ntests, (100 * passed) / ntests);

    munmap(test_code_area, test_code_size);

    return passed != ntests;
}

