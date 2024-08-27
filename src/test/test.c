#include <stdio.h>

#include <sys/mman.h>

#include "../cnm.c"

static uint8_t test_region[2048];
static uint8_t test_globals[2048];
static uint8_t *test_globals_a4; // Aligned to 4 byte boundary
static uint8_t *test_globals_a8; // Aligned to 4 byte boundary

static void *test_code_area;
static size_t test_code_size;

static void test_errcb(int line, const char *verbose, const char *simple) {
    fprintf(stderr, "\n%s", verbose);
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
#define GENERIC_TEST(_name, _errcb) \
    static bool _name(void) { \
        cnm_t *cnm = cnm_init(test_region, sizeof(test_region), \
                              test_code_area, test_code_size, \
                              test_globals, sizeof(test_globals)); \
        cnm_set_errcb(cnm, _errcb);
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
    if (strcmp(cnm->s.tok.suffix, "") != 0) return TESTFAIL;
    if (!strview_eq(cnm->s.tok.src, SV("\"hello \""))) return TESTFAIL;
    if (strcmp(cnm->s.tok.s, "hello ") != 0) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_lexer_string2, test_errcb, "u8\"hello \"  \"world\"")
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_STRING) return TESTFAIL;
    if (cnm->s.tok.start.row != 1) return TESTFAIL;
    if (cnm->s.tok.start.col != 1) return TESTFAIL;
    if (cnm->s.tok.end.row != 1) return TESTFAIL;
    if (cnm->s.tok.end.col != 20) return TESTFAIL;
    if (strcmp(cnm->s.tok.suffix, "u8") != 0) return TESTFAIL;
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
    if (strcmp(cnm->s.tok.suffix, "") != 0) return TESTFAIL;
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
SIMPLE_TEST(test_lexer_string5, test_errcb,  "u8\"hello \\u263A world\"")
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_STRING) return TESTFAIL;
    if (cnm->s.tok.start.row != 1) return TESTFAIL;
    if (cnm->s.tok.start.col != 1) return TESTFAIL;
    if (cnm->s.tok.end.row != 1) return TESTFAIL;
    if (cnm->s.tok.end.col != 23) return TESTFAIL;
    if (strcmp(cnm->s.tok.suffix, "u8") != 0) return TESTFAIL;
    if (!strview_eq(cnm->s.tok.src, SV("\"hello \\u263A world\""))) return TESTFAIL;
    if (strcmp(cnm->s.tok.s, "hello ☺ world") != 0) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_lexer_string6, test_errcb,  "u8\"hello ☺ world\"")
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_STRING) return TESTFAIL;
    if (cnm->s.tok.start.row != 1) return TESTFAIL;
    if (cnm->s.tok.start.col != 1) return TESTFAIL;
    if (cnm->s.tok.end.row != 1) return TESTFAIL;
    if (cnm->s.tok.end.col != 18) return TESTFAIL;
    if (strcmp(cnm->s.tok.suffix, "u8") != 0) return TESTFAIL;
    if (!strview_eq(cnm->s.tok.src, SV("\"hello ☺ world\""))) return TESTFAIL;
    if (strcmp(cnm->s.tok.s, "hello ☺ world") != 0) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_lexer_string7, test_errcb,  "U\"abc\"")
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_STRING) return TESTFAIL;
    if (cnm->s.tok.start.row != 1) return TESTFAIL;
    if (cnm->s.tok.start.col != 1) return TESTFAIL;
    if (cnm->s.tok.end.row != 1) return TESTFAIL;
    if (cnm->s.tok.end.col != 7) return TESTFAIL;
    if (strcmp(cnm->s.tok.suffix, "U") != 0) return TESTFAIL;
    if (!strview_eq(cnm->s.tok.src, SV("\"abc\""))) return TESTFAIL;
    if (((uint32_t *)cnm->s.tok.s)[0] != 'a') return TESTFAIL;
    if (((uint32_t *)cnm->s.tok.s)[1] != 'b') return TESTFAIL;
    if (((uint32_t *)cnm->s.tok.s)[2] != 'c') return TESTFAIL;
    if (((uint32_t *)cnm->s.tok.s)[3] != '\0') return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_lexer_char1, test_errcb,  "'h'")
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_CHAR) return TESTFAIL;
    if (cnm->s.tok.start.row != 1) return TESTFAIL;
    if (cnm->s.tok.start.col != 1) return TESTFAIL;
    if (cnm->s.tok.end.row != 1) return TESTFAIL;
    if (cnm->s.tok.end.col != 4) return TESTFAIL;
    if (strcmp(cnm->s.tok.suffix, "") != 0) return TESTFAIL;
    if (!strview_eq(cnm->s.tok.src, SV("'h'"))) return TESTFAIL;
    if (cnm->s.tok.c != 'h') return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_lexer_char2, test_errcb,  "u8'\\x41'")
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_CHAR) return TESTFAIL;
    if (cnm->s.tok.start.row != 1) return TESTFAIL;
    if (cnm->s.tok.start.col != 1) return TESTFAIL;
    if (cnm->s.tok.end.row != 1) return TESTFAIL;
    if (cnm->s.tok.end.col != 9) return TESTFAIL;
    if (strcmp(cnm->s.tok.suffix, "u8") != 0) return TESTFAIL;
    if (!strview_eq(cnm->s.tok.src, SV("'\\x41'"))) return TESTFAIL;
    if (cnm->s.tok.c != 'A') return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_lexer_char3, test_errcb,  "U'☺'")
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_CHAR) return TESTFAIL;
    if (cnm->s.tok.start.row != 1) return TESTFAIL;
    if (cnm->s.tok.start.col != 1) return TESTFAIL;
    if (cnm->s.tok.end.row != 1) return TESTFAIL;
    if (cnm->s.tok.end.col != 5) return TESTFAIL;
    if (strcmp(cnm->s.tok.suffix, "U") != 0) return TESTFAIL;
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
    if (strcmp(cnm->s.tok.suffix, "") != 0) return TESTFAIL;
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
    if (strcmp(cnm->s.tok.suffix, "u") != 0) return TESTFAIL;
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
    if (strcmp(cnm->s.tok.suffix, "ull") != 0) return TESTFAIL;
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
    if (strcmp(cnm->s.tok.suffix, "") != 0) return TESTFAIL;
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
    if (strcmp(cnm->s.tok.suffix, "") != 0) return TESTFAIL;
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
    if (cnm->s.tok.f != 0.0) return TESTFAIL;
    if (strcmp(cnm->s.tok.suffix, "") != 0) return TESTFAIL;
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
    if (cnm->s.tok.f != 0.5) return TESTFAIL;
    if (strcmp(cnm->s.tok.suffix, "") != 0) return TESTFAIL;
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
    if (cnm->s.tok.f != 420.69) return TESTFAIL;
    if (strcmp(cnm->s.tok.suffix, "") != 0) return TESTFAIL;
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
    if (cnm->s.tok.f != 420.69) return TESTFAIL;
    if (strcmp(cnm->s.tok.suffix, "f") != 0) return TESTFAIL;
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
    if (cnm->s.tok.f != 42.0) return TESTFAIL;
    if (strcmp(cnm->s.tok.suffix, "") != 0) return TESTFAIL;
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
    if (cnm->s.tok.f != 42.0) return TESTFAIL;
    if (strcmp(cnm->s.tok.suffix, "f") != 0) return TESTFAIL;
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
    if (!expr_parse(cnm, &val, false, false, PREC_FULL, NULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_INT) return TESTFAIL;
    if (val.type.size != 1) return TESTFAIL;
    if (val.literal.i != 5) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding2, test_errcb,  "5.0")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL, NULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_DOUBLE) return TESTFAIL;
    if (val.literal.d != 5.0) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding3, test_errcb,  "5000000000")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL, NULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_LONG) return TESTFAIL;
    if (val.literal.i != 5000000000) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding4, test_errcb,  "5u")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL, NULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_UINT) return TESTFAIL;
    if (val.literal.u != 5) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding5, test_errcb,  "9223372036854775809ull")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL, NULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_ULLONG) return TESTFAIL;
    if (val.literal.u != 9223372036854775809ull) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding6, test_expect_errcb,  "9223372036854775809")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL, NULL)) return TESTFAIL;
    return test_expect_err;
}
SIMPLE_TEST(test_expr_constant_folding7, test_errcb,  "92ull")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL, NULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_ULLONG) return TESTFAIL;
    if (val.literal.u != 92ull) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding8, test_errcb,  "5 + 5")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL, NULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_INT) return TESTFAIL;
    if (val.literal.i != 10) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding9, test_errcb,  "5 * 5 + 3")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL, NULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_INT) return TESTFAIL;
    if (val.literal.i != 28) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding10, test_errcb,  "5 + 5 * 3")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL, NULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_INT) return TESTFAIL;
    if (val.literal.i != 20) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding11, test_errcb,  "5ul + 30.0")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL, NULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_DOUBLE) return TESTFAIL;
    if (val.literal.d != 35) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding12, test_errcb,  "1 / 2.0f")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL, NULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_FLOAT) return TESTFAIL;
    if (val.literal.f != 1 / 2.0f) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding13, test_errcb,  "10 / 3")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL, NULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_INT) return TESTFAIL;
    if (val.literal.i != 10 / 3) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding14, test_errcb,  "10ul * 12")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL, NULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_ULONG) return TESTFAIL;
    if (val.literal.u != 120) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding15, test_errcb,  "3 - 9 * 2l")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL, NULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_LONG) return TESTFAIL;
    if (val.literal.i != -15) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding16, test_errcb,  "-8")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL, NULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_INT) return TESTFAIL;
    if (val.literal.i != -8) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding17, test_errcb,  "(-8)")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL, NULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_INT) return TESTFAIL;
    if (val.literal.i != -8) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding18, test_errcb,  "-(0x8)")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL, NULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_INT) return TESTFAIL;
    if (val.literal.i != -8) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding19, test_errcb,  "~0x8u")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL, NULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_UINT) return TESTFAIL;
    if (val.literal.u != ~0x8u) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding20, test_expect_errcb,  "~(1 + 0.9)")
    token_next(cnm);
    valref_t val;
    if (expr_parse(cnm, &val, false, false, PREC_FULL, NULL)) return TESTFAIL;
    return test_expect_err;
}
SIMPLE_TEST(test_expr_constant_folding21, test_errcb,  "!1")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL, NULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_BOOL) return TESTFAIL;
    if (val.literal.u != false) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding22, test_errcb,  "!0.0")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL, NULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_BOOL) return TESTFAIL;
    if (val.literal.u != true) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding23, test_errcb,  "1 << 4")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL, NULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_INT) return TESTFAIL;
    if (val.literal.i != 16) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding24, test_errcb,  "0xff | 0xff << 8")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL, NULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_INT) return TESTFAIL;
    if (val.literal.i != 0xffff) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding25, test_errcb,  "-(5 + 10) * 2 + 60 >> 2")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL, NULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_INT) return TESTFAIL;
    if (val.literal.i != (-(5 + 10) * 2 + 60) >> 2) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_expr_constant_folding26, test_errcb,  "1 + 'a'")
    token_next(cnm);
    valref_t val;
    if (!expr_parse(cnm, &val, false, false, PREC_FULL, NULL)) return TESTFAIL;
    if (!val.isliteral) return TESTFAIL;
    if (val.type.type[0].class != TYPE_INT) return TESTFAIL;
    if (val.literal.i != 'b') return TESTFAIL;
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
    typeref_t type = type_parse(cnm, &base, &name, false);
    if (istypedef) return TESTFAIL;
    if (!strview_eq(name, SV("foo"))) return TESTFAIL;
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_PTR },
            (type_t){ .class = TYPE_CHAR, .isconst = true, .n = 8 },
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
    typeref_t type = type_parse(cnm, &base, &name, false);
    if (!istypedef) return TESTFAIL;
    if (!strview_eq(name, SV("u8"))) return TESTFAIL;
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_UCHAR, .n = 8 },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_char, test_errcb,  "char")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL, false);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_CHAR, .n = 8 },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_uchar, test_errcb,  "unsigned char")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL, false);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_UCHAR, .n = 8 },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_short1, test_errcb,  "short")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL, false);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_SHORT, .n = 16 },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_short2, test_errcb,  "short int")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL, false);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_SHORT, .n = 16 },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_short3, test_expect_errcb,  "short short int")
    token_next(cnm);
    type_t base;
    if (type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    type_parse(cnm, &base, NULL, false);
    return test_expect_err;
}
SIMPLE_TEST(test_type_parsing_short4, test_expect_errcb,  "short char")
    token_next(cnm);
    type_t base;
    if (type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    type_parse(cnm, &base, NULL, false);
    return test_expect_err;
}
SIMPLE_TEST(test_type_parsing_ushort1, test_errcb,  "unsigned short")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL, false);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_USHORT, .n = 16 },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_ushort2, test_errcb,  "unsigned short int")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL, false);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_USHORT, .n = 16 },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_ushort3, test_expect_errcb,  "unsigned unsigned short")
    token_next(cnm);
    type_t base;
    if (type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    type_parse(cnm, &base, NULL, false);
    return test_expect_err;
}
SIMPLE_TEST(test_type_parsing_int, test_errcb,  "int")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL, false);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT, .n = 32 },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_uint1, test_errcb,  "unsigned int")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL, false);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_UINT, .n = 32 },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_uint2, test_errcb,  "unsigned")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL, false);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_UINT, .n = 32 },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_uint3, test_errcb,  "const unsigned")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL, false);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_UINT, .isconst = true, .n = 32 },
        },
        .size = 1,
    }, true)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_long1, test_errcb,  "long")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL, false);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_LONG, .n = sizeof(long) * 8 },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_long2, test_errcb,  "long long")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL, false);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_LLONG, .n = 64 },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_long3, test_errcb,  "long int")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL, false);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_LONG, .n = sizeof(long) * 8 },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_long4, test_errcb,  "long long int")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL, false);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_LLONG, .n = 64 },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_ulong, test_errcb,  "unsigned long long")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL, false);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_ULLONG, .n = 64 },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing_float, test_errcb,  "float")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL, false);
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
    typeref_t type = type_parse(cnm, &base, NULL, false);
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
    typeref_t type = type_parse(cnm, &base, NULL, false);
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
    type_parse(cnm, &base, NULL, false);
    return test_expect_err;
}
SIMPLE_TEST(test_type_parsing5, test_errcb,  "const char *(* const)[][3]")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL, false);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_PTR, .isconst = true },
            (type_t){ .class = TYPE_ARR },
            (type_t){ .class = TYPE_ARR, .n = 3 },
            (type_t){ .class = TYPE_PTR },
            (type_t){ .class = TYPE_CHAR, .isconst = true, .n = 8 },
        },
        .size = 5,
    }, true)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing6, test_errcb,  "int *(*get_int)(char x[], bool z)")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL, false);
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_PTR, .isconst = true },
            (type_t){ .class = TYPE_FN, .n = 2 },
            (type_t){ .class = TYPE_FN_ARG, .n = 2 },
            (type_t){ .class = TYPE_PTR },
            (type_t){ .class = TYPE_CHAR, .n = 8 },
            (type_t){ .class = TYPE_FN_ARG, .n = 1 },
            (type_t){ .class = TYPE_BOOL, .isconst = true },
            (type_t){ .class = TYPE_PTR },
            (type_t){ .class = TYPE_INT, .n = 32 },
        },
        .size = 9,
    }, false)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing7, test_errcb,  "int *(*get_int)(char x[], bool z)")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL, false);
    return !type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_PTR, .isconst = true },
            (type_t){ .class = TYPE_FN, .n = 2 },
            (type_t){ .class = TYPE_FN_ARG, .n = 2 },
            (type_t){ .class = TYPE_PTR },
            (type_t){ .class = TYPE_CHAR, .isconst = true, .n = 8 },
            (type_t){ .class = TYPE_FN_ARG, .n = 1 },
            (type_t){ .class = TYPE_BOOL, .isconst = true, .n = 8 },
            (type_t){ .class = TYPE_PTR, .isconst = true },
            (type_t){ .class = TYPE_INT, .n = 32 },
        },
        .size = 9,
    }, false);
}
SIMPLE_TEST(test_type_parsing8, test_errcb,  "double")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL, false);
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
    typeref_t type = type_parse(cnm, &base, &name, false);
    if (!strview_eq(name, SV("a"))) return TESTFAIL;
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT, .n = 32 },
        },
        .size = 1,
    }, false)) return TESTFAIL;

    if (cnm->s.tok.type != TOKEN_COMMA) return TESTFAIL;
    token_next(cnm);

    // b
    type = type_parse(cnm, &base, &name, false);
    if (!strview_eq(name, SV("b"))) return TESTFAIL;
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_PTR },
            (type_t){ .class = TYPE_INT, .n = 32 },
        },
        .size = 2,
    }, false)) return TESTFAIL;

    if (cnm->s.tok.type != TOKEN_COMMA) return TESTFAIL;
    token_next(cnm);

    // c
    type = type_parse(cnm, &base, &name, false);
    if (!strview_eq(name, SV("c"))) return TESTFAIL;
    if (!type_eq(type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_ARR, .n = 2 },
            (type_t){ .class = TYPE_PTR },
            (type_t){ .class = TYPE_INT, .n = 32 },
        },
        .size = 3,
    }, false)) return TESTFAIL;

    return true;
}
GENERIC_TEST(test_type_parsing10, test_expect_errcb)
    if (!cnm_parse(cnm, "typedef int foo_t;", "test_type_parsing10")) return TESTFAIL;
    cnm_set_src(cnm, "foo_t foo", "test_type_parsing10");
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    strview_t name;
    typeref_t type = type_parse(cnm, &base, &name, false);
    if (!type.type) return TESTFAIL;

    if (!strview_eq(name, SV("foo"))) return TESTFAIL;
    if (!type_eq(type, (typeref_t){
        .size = 1,
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT, .n = 32 },
        },
    }, true)) return TESTFAIL;

    return !test_expect_err;
}
GENERIC_TEST(test_type_parsing11, test_expect_errcb)
    if (!cnm_parse(cnm, "typedef int foo_t;", "test_type_parsing11")) return TESTFAIL;
    cnm_set_src(cnm, "unsigned foo_t *foo", "test_type_parsing11");
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL, false);
    if (!type.type) return TESTFAIL;

    if (!type_eq(type, (typeref_t){
        .size = 2,
        .type = (type_t[]){
            (type_t){ .class = TYPE_PTR },
            (type_t){ .class = TYPE_UINT, .n = 32 },
        },
    }, true)) return TESTFAIL;

    return !test_expect_err;
}
GENERIC_TEST(test_type_parsing12, test_expect_errcb)
    if (!cnm_parse(cnm, "typedef const int *foo_t;", "test_type_parsing12")) return TESTFAIL;
    cnm_set_src(cnm, "foo_t foo[2]", "test_type_parsing12");
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL, false);
    if (!type.type) return TESTFAIL;

    if (!type_eq(type, (typeref_t){
        .size = 3,
        .type = (type_t[]){
            (type_t){ .class = TYPE_ARR, .n = 2 },
            (type_t){ .class = TYPE_PTR },
            (type_t){ .class = TYPE_INT, .isconst = true, .n = 32 },
        },
    }, true)) return TESTFAIL;

    return !test_expect_err;
}
GENERIC_TEST(test_type_parsing13, test_expect_errcb)
    if (!cnm_parse(cnm, "typedef const int *foo_t;", "test_type_parsing13")) return TESTFAIL;
    cnm_set_src(cnm, "unsigned foo_t foo", "test_type_parsing13");
    token_next(cnm);
    type_t base;
    if (type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    return test_expect_err;
}
SIMPLE_TEST(test_type_parsing14, test_errcb,  "int")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL, true);
    if (!type_eq(type, (typeref_t){
        .size = 1,
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT, .n = 32 },
        },
    }, true)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing15, test_errcb,  "int : 3")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    type_parse(cnm, &base, NULL, false);
    if (cnm->s.tok.type != TOKEN_COLON) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing16, test_errcb,  "int : 9")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL, true);
    if (!type_eq(type, (typeref_t){
        .size = 1,
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT, .n = 9 },
        },
    }, true)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing17, test_errcb,  "long long : 30 + (1 << 3)")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, NULL, true);
    if (!type_eq(type, (typeref_t){
        .size = 1,
        .type = (type_t[]){
            (type_t){ .class = TYPE_LLONG, .n = 38 },
        },
    }, true)) return TESTFAIL;
    return true;
}
SIMPLE_TEST(test_type_parsing18, test_expect_errcb,  "bool : 4")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    type_parse(cnm, &base, NULL, true);
    return test_expect_err;
}
SIMPLE_TEST(test_type_parsing19, test_expect_errcb,  "char : 9")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    type_parse(cnm, &base, NULL, true);
    return test_expect_err;
}
SIMPLE_TEST(test_type_parsing20, test_expect_errcb,  "double : 9")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    type_parse(cnm, &base, NULL, true);
    return test_expect_err;
}
static cnm_t *test_util_create_types1(const char *fname) {
    cnm_t *cnm = cnm_init(test_region, sizeof(test_region),
                          test_code_area, test_code_size,
                          test_globals, sizeof(test_globals));
    cnm_set_errcb(cnm, test_errcb);
    return cnm_parse(cnm,
                   "struct foo {"
                   "    int x;"
                   "    int y;"
                   "};"
                   "typedef struct {"
                   "    char a[2];"
                   "    double b;"
                   "} baz_t;", fname) ? cnm : NULL;
}
// Half type parsing half typedef parsing
static bool test_type_parsing21(void) {
    cnm_t *cnm = test_util_create_types1("test_type_parsing21");
    if (!cnm) return TESTFAIL;
    cnm_set_src(cnm, "struct foo bar", "test_type_parsing21");
    token_next(cnm);

    // bar
    type_t base;
    strview_t name;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, &name, true);
    if (!type.type) return TESTFAIL;
    if (!strview_eq(name, SV("bar"))) return TESTFAIL;
    if (!type_eq(type, (typeref_t){
        .size = 1,
        .type = (type_t[]){
            (type_t){ .class = TYPE_USER, .n = 0 },
        },
    }, true)) return TESTFAIL;

    // struct foo
    if (!cnm->type.types) return TESTFAIL;
    userty_t *u = cnm->type.types->next;
    if (!u) return TESTFAIL;
    field_list_t *s = (field_list_t *)u->data;

    if (!strview_eq(u->name, SV("foo"))) return TESTFAIL;
    if (u->type != USER_STRUCT) return TESTFAIL;
    if (u->inf.size != sizeof(int) * 2) return TESTFAIL;
    if (u->inf.align != sizeof(int)) return TESTFAIL;
    if (u->scope != 0) return TESTFAIL;
    if (u->typeid != 0) return TESTFAIL;
    if (u->next != NULL) return TESTFAIL;
    if (!s->fields) return TESTFAIL;

    // struct foo::y
    field_t *f = s->fields;
    if (!strview_eq(f->name, SV("y"))) return TESTFAIL;
    if (f->offs != sizeof(int)) return TESTFAIL;
    if (f->bit_offs != 0) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .size = 1,
        .type = &(type_t){ .class = TYPE_INT, .n = 32 },
    }, true)) return TESTFAIL;
    if (f->next == NULL) return TESTFAIL;

    // struct foo::x
    f = f->next;
    if (!strview_eq(f->name, SV("x"))) return TESTFAIL;
    if (f->offs != 0) return TESTFAIL;
    if (f->bit_offs != 0) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .size = 1,
        .type = &(type_t){ .class = TYPE_INT, .n = 32 },
    }, true)) return TESTFAIL;
    if (f->next != NULL) return TESTFAIL;

    return true;
}
// Half type parsing half typedef parsing
static bool test_type_parsing22(void) {
    cnm_t *cnm = test_util_create_types1("test_type_parsing22");
    if (!cnm) return TESTFAIL;
    cnm_set_src(cnm, "baz_t bar", "test_type_parsing22");
    token_next(cnm);

    // bar
    type_t base;
    strview_t name;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t type = type_parse(cnm, &base, &name, true);
    if (!type.type) return TESTFAIL;
    if (!strview_eq(name, SV("bar"))) return TESTFAIL;
    if (!type_eq(type, (typeref_t){
        .size = 1,
        .type = (type_t[]){
            (type_t){ .class = TYPE_USER, .n = 1 },
        },
    }, true)) return TESTFAIL;

    if (!cnm->type.types) return TESTFAIL;
    if (cnm->type.types->typeid != 1) return TESTFAIL;

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
    typeref_t ref = type_parse(cnm, &base, &name, false);
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
    field_list_t *s = (field_list_t *)t->data;
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
            (type_t){ .class = TYPE_INT, .n = 32 },
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
            (type_t){ .class = TYPE_INT, .n = 32 },
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
    typeref_t ref = type_parse(cnm, &base, &name, false);
    if (!strview_eq(name, SV("FSN__FHA__FZR__FEX__FGO"))) return TESTFAIL;
    if (!type_eq(ref, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_USER, .n = 0 },
        },
        .size = 1,
    }, false)) return TESTFAIL;

    // struct baz
    userty_t *t = cnm->type.types;
    field_list_t *s = (field_list_t *)t->data;
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
            (type_t){ .class = TYPE_INT, .n = 32 },
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
            (type_t){ .class = TYPE_CHAR, .n = 8 },
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
    typeref_t ref = type_parse(cnm, &base, &name, false);
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
    field_list_t *s = (field_list_t *)t->data;
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
            (type_t){ .class = TYPE_CHAR, .n = 8 },
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
            (type_t){ .class = TYPE_INT, .n = 32 },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    f = f->next;

    // Goto next struct
    t = t->next;
    if (!t) return TESTFAIL;
    s = (field_list_t *)t->data;
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
            (type_t){ .class = TYPE_CHAR, .n = 8 },
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
            (type_t){ .class = TYPE_INT, .n = 32 },
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
    typeref_t ref = type_parse(cnm, &base, &name, false);
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
    typeref_t ref = type_parse(cnm, &base, &name, false);
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
    typeref_t ref = type_parse(cnm, &base, &name, false);
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
            (type_t){ .class = TYPE_UINT, .n = 32 },
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
    typeref_t ref = type_parse(cnm, &base, &name, false);
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
    field_list_t *s = (field_list_t *)t->data;
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
            (type_t){ .class = TYPE_CHAR, .n = 8 },
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
            (type_t){ .class = TYPE_INT, .n = 32 },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    f = f->next;

    // Goto next struct
    t = t->next;
    if (!t) return TESTFAIL;
    s = (field_list_t *)t->data;
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
            (type_t){ .class = TYPE_CHAR, .n = 8 },
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
            (type_t){ .class = TYPE_INT, .n = 32 },
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
    type_parse(cnm, &base, NULL, false);
    return test_expect_err;
}
SIMPLE_TEST(test_typedef_parsing9, test_expect_errcb,
        "enum SNU {\n"
        "} EBE \n")
    token_next(cnm);
    type_t base;
    if (type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    type_parse(cnm, &base, NULL, false);
    return test_expect_err;
}
SIMPLE_TEST(test_typedef_parsing10, test_expect_errcb,
        "union SNU {\n"
        "} EBE \n")
    token_next(cnm);
    type_t base;
    if (type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    type_parse(cnm, &base, NULL, false);
    return test_expect_err;
}
SIMPLE_TEST(test_typedef_parsing11, test_errcb,
        "struct foo {\n"
        "    int a, b;\n"
        "}\n")
    token_next(cnm);
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t ref = type_parse(cnm, &base, NULL, false);
    if (!type_eq(ref, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_USER, .n = 0 },
        },
        .size = 1,
    }, false)) return TESTFAIL;

    // struct foo
    userty_t *t = cnm->type.types;
    field_list_t *s = (field_list_t *)t->data;
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
            (type_t){ .class = TYPE_INT, .n = 32 },
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
            (type_t){ .class = TYPE_INT, .n = 32 },
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
    typeref_t ref = type_parse(cnm, &base, NULL, false);
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
SIMPLE_TEST(test_typedef_parsing13, test_errcb,
        "struct foo {\n"
        "    int a : 4;\n"
        "    int b : 24;\n"
        "}\n")
    token_next(cnm);
    strview_t name;
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t ref = type_parse(cnm, &base, &name, false);

    // struct foo
    userty_t *t = cnm->type.types;
    field_list_t *s = (field_list_t *)t->data;
    if (!t) return TESTFAIL;
    if (t->inf.size != sizeof(int)) return TESTFAIL;
    if (t->inf.align != sizeof(int)) return TESTFAIL;
    if (t->typeid != 0) return TESTFAIL;
   
    // foo::b
    field_t *f = s->fields;
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("b"))) return TESTFAIL;
    if (f->offs != 0) return TESTFAIL;
    if (f->bit_offs != 4) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT, .n = 24 },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    f = f->next;

    // foo::a
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("a"))) return TESTFAIL;
    if (f->offs != 0) return TESTFAIL;
    if (f->bit_offs != 0) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT, .n = 4 },
        },
        .size = 1,
    }, false)) return TESTFAIL;

    return true;
}
SIMPLE_TEST(test_typedef_parsing14, test_errcb,
        "struct foo {\n"
        "    int a : 4;\n"
        "    int b : 20;\n"
        "    int c : 16;\n"
        "}\n")
    token_next(cnm);
    strview_t name;
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t ref = type_parse(cnm, &base, &name, false);

    // struct foo
    userty_t *t = cnm->type.types;
    field_list_t *s = (field_list_t *)t->data;
    if (!t) return TESTFAIL;
    if (t->inf.size != sizeof(int) * 2) return TESTFAIL;
    if (t->inf.align != sizeof(int)) return TESTFAIL;
    if (t->typeid != 0) return TESTFAIL;
   
    // foo::c
    field_t *f = s->fields;
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("c"))) return TESTFAIL;
    if (f->offs != 4) return TESTFAIL;
    if (f->bit_offs != 0) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT, .n = 16 },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    f = f->next;

    // foo::b
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("b"))) return TESTFAIL;
    if (f->offs != 0) return TESTFAIL;
    if (f->bit_offs != 4) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT, .n = 20 },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    f = f->next;

    // foo::a
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("a"))) return TESTFAIL;
    if (f->offs != 0) return TESTFAIL;
    if (f->bit_offs != 0) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT, .n = 4 },
        },
        .size = 1,
    }, false)) return TESTFAIL;

    return true;
}
SIMPLE_TEST(test_typedef_parsing15, test_errcb,
        "struct foo {\n"
        "    int a : 4;\n"
        "    unsigned int b : 24;\n"
        "}\n")
    token_next(cnm);
    strview_t name;
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t ref = type_parse(cnm, &base, &name, false);

    // struct foo
    userty_t *t = cnm->type.types;
    field_list_t *s = (field_list_t *)t->data;
    if (!t) return TESTFAIL;
    if (t->inf.size != sizeof(int)) return TESTFAIL;
    if (t->inf.align != sizeof(int)) return TESTFAIL;
    if (t->typeid != 0) return TESTFAIL;
   
    // foo::b
    field_t *f = s->fields;
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("b"))) return TESTFAIL;
    if (f->offs != 0) return TESTFAIL;
    if (f->bit_offs != 4) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_UINT, .n = 24 },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    f = f->next;

    // foo::a
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("a"))) return TESTFAIL;
    if (f->offs != 0) return TESTFAIL;
    if (f->bit_offs != 0) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT, .n = 4 },
        },
        .size = 1,
    }, false)) return TESTFAIL;

    return true;
}
SIMPLE_TEST(test_typedef_parsing16, test_errcb,
        "struct foo {\n"
        "    int a : 4;\n"
        "    int : 0;\n"
        "    unsigned int b : 24;\n"
        "}\n")
    token_next(cnm);
    strview_t name;
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t ref = type_parse(cnm, &base, &name, false);

    // struct foo
    userty_t *t = cnm->type.types;
    field_list_t *s = (field_list_t *)t->data;
    if (!t) return TESTFAIL;
    if (t->inf.size != sizeof(int) * 2) return TESTFAIL;
    if (t->inf.align != sizeof(int)) return TESTFAIL;
    if (t->typeid != 0) return TESTFAIL;
   
    // foo::b
    field_t *f = s->fields;
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("b"))) return TESTFAIL;
    if (f->offs != 4) return TESTFAIL;
    if (f->bit_offs != 0) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_UINT, .n = 24 },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    f = f->next;

    // foo::a
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("a"))) return TESTFAIL;
    if (f->offs != 0) return TESTFAIL;
    if (f->bit_offs != 0) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT, .n = 4 },
        },
        .size = 1,
    }, false)) return TESTFAIL;

    return true;
}
SIMPLE_TEST(test_typedef_parsing17, test_errcb,
        "struct foo {\n"
        "    char a : 4;\n"
        "    char b : 5;\n"
        "    int c : 23;\n"
        "}\n")
    token_next(cnm);
    strview_t name;
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t ref = type_parse(cnm, &base, &name, false);

    // struct foo
    userty_t *t = cnm->type.types;
    field_list_t *s = (field_list_t *)t->data;
    if (!t) return TESTFAIL;
    if (t->inf.size != sizeof(int) * 2) return TESTFAIL;
    if (t->inf.align != sizeof(int)) return TESTFAIL;
    if (t->typeid != 0) return TESTFAIL;
   
    // foo::c
    field_t *f = s->fields;
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("c"))) return TESTFAIL;
    if (f->offs != 4) return TESTFAIL;
    if (f->bit_offs != 0) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT, .n = 23 },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    f = f->next;

    // foo::b
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("b"))) return TESTFAIL;
    if (f->offs != 1) return TESTFAIL;
    if (f->bit_offs != 0) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_CHAR, .n = 5 },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    f = f->next;

    // foo::a
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("a"))) return TESTFAIL;
    if (f->offs != 0) return TESTFAIL;
    if (f->bit_offs != 0) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_CHAR, .n = 4 },
        },
        .size = 1,
    }, false)) return TESTFAIL;

    return true;
}
SIMPLE_TEST(test_typedef_parsing18, test_errcb,
        "struct foo {\n"
        "    char a : 3;\n"
        "    char b : 5;\n"
        "    short c : 8;\n"
        "}\n")
    token_next(cnm);
    strview_t name;
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t ref = type_parse(cnm, &base, &name, false);

    // struct foo
    userty_t *t = cnm->type.types;
    field_list_t *s = (field_list_t *)t->data;
    if (!t) return TESTFAIL;
    if (t->inf.size != sizeof(short) * 2) return TESTFAIL;
    if (t->inf.align != sizeof(short)) return TESTFAIL;
    if (t->typeid != 0) return TESTFAIL;
   
    // foo::c
    field_t *f = s->fields;
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("c"))) return TESTFAIL;
    if (f->offs != 2) return TESTFAIL;
    if (f->bit_offs != 0) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_SHORT, .n = 8 },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    f = f->next;

    // foo::b
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("b"))) return TESTFAIL;
    if (f->offs != 0) return TESTFAIL;
    if (f->bit_offs != 3) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_CHAR, .n = 5 },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    f = f->next;

    // foo::a
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("a"))) return TESTFAIL;
    if (f->offs != 0) return TESTFAIL;
    if (f->bit_offs != 0) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_CHAR, .n = 3 },
        },
        .size = 1,
    }, false)) return TESTFAIL;

    return true;
}
SIMPLE_TEST(test_typedef_parsing19, test_errcb,
        "struct foo {\n"
        "    char a : 3;\n"
        "    char b : 5;\n"
        "    short c;\n"
        "}\n")
    token_next(cnm);
    strview_t name;
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t ref = type_parse(cnm, &base, &name, false);

    // struct foo
    userty_t *t = cnm->type.types;
    field_list_t *s = (field_list_t *)t->data;
    if (!t) return TESTFAIL;
    if (t->inf.size != sizeof(short) * 2) return TESTFAIL;
    if (t->inf.align != sizeof(short)) return TESTFAIL;
    if (t->typeid != 0) return TESTFAIL;
   
    // foo::c
    field_t *f = s->fields;
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("c"))) return TESTFAIL;
    if (f->offs != 2) return TESTFAIL;
    if (f->bit_offs != 0) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_SHORT, .n = 16 },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    f = f->next;

    // foo::b
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("b"))) return TESTFAIL;
    if (f->offs != 0) return TESTFAIL;
    if (f->bit_offs != 3) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_CHAR, .n = 5 },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    f = f->next;

    // foo::a
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("a"))) return TESTFAIL;
    if (f->offs != 0) return TESTFAIL;
    if (f->bit_offs != 0) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_CHAR, .n = 3 },
        },
        .size = 1,
    }, false)) return TESTFAIL;

    return true;
}
SIMPLE_TEST(test_typedef_parsing20, test_errcb,
        "struct foo {\n"
        "    int a : 4;\n"
        "    int b : 24;\n"
        "    int c : 16;\n"
        "}\n")
    token_next(cnm);
    strview_t name;
    type_t base;
    if (!type_parse_declspec(cnm, &base, NULL)) return TESTFAIL;
    typeref_t ref = type_parse(cnm, &base, &name, false);

    // struct foo
    userty_t *t = cnm->type.types;
    field_list_t *s = (field_list_t *)t->data;
    if (!t) return TESTFAIL;
    if (t->inf.size != sizeof(int) * 2) return TESTFAIL;
    if (t->inf.align != sizeof(int)) return TESTFAIL;
    if (t->typeid != 0) return TESTFAIL;
   
    // foo::c
    field_t *f = s->fields;
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("c"))) return TESTFAIL;
    if (f->offs != 4) return TESTFAIL;
    if (f->bit_offs != 0) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT, .n = 16 },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    f = f->next;

    // foo::b
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("b"))) return TESTFAIL;
    if (f->offs != 0) return TESTFAIL;
    if (f->bit_offs != 4) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT, .n = 24 },
        },
        .size = 1,
    }, false)) return TESTFAIL;
    f = f->next;

    // foo::a
    if (!f) return TESTFAIL;
    if (!strview_eq(f->name, SV("a"))) return TESTFAIL;
    if (f->offs != 0) return TESTFAIL;
    if (f->bit_offs != 0) return TESTFAIL;
    if (!type_eq(f->type, (typeref_t){
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT, .n = 4 },
        },
        .size = 1,
    }, false)) return TESTFAIL;

    return true;
}

///////////////////////////////////////////////////////////////////////////////
//
// File Level Typedef Tests
//
///////////////////////////////////////////////////////////////////////////////
GENERIC_TEST(test_filelvl_typedef1, test_expect_errcb)
    if (!cnm_parse(cnm, "typedef int foo_t;", "test_filelvl_typedef1")) return TESTFAIL;
    if (!cnm->type.typedefs) return TESTFAIL;
    if (cnm->type.typedef_gid != 1) return TESTFAIL;

    typedef_t *t = cnm->type.typedefs;

    // foo_t
    if (!strview_eq(t->name, SV("foo_t"))) return TESTFAIL;
    if (t->scope != 0) return TESTFAIL;
    if (t->typedef_id != 0) return TESTFAIL;
    if (!type_eq(t->type, (typeref_t){
        .size = 1,
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT, .n = 32 },
        },
    }, true)) return TESTFAIL;
    if (t->next) return TESTFAIL;

    return !test_expect_err;
}
GENERIC_TEST(test_filelvl_typedef2, test_expect_errcb)
    if (!cnm_parse(cnm, "typedef int foo_t, *bar_t;",
                   "test_filelvl_typedef2")) return TESTFAIL;
    if (!cnm->type.typedefs) return TESTFAIL;
    if (cnm->type.typedef_gid != 2) return TESTFAIL;

    typedef_t *t = cnm->type.typedefs;

    // bar_t
    if (!strview_eq(t->name, SV("bar_t"))) return TESTFAIL;
    if (t->scope != 0) return TESTFAIL;
    if (t->typedef_id != 1) return TESTFAIL;
    if (!type_eq(t->type, (typeref_t){
        .size = 2,
        .type = (type_t[]){
            (type_t){ .class = TYPE_PTR },
            (type_t){ .class = TYPE_INT, .n = 32 },
        },
    }, true)) return TESTFAIL;
    if (!t->next) return TESTFAIL;
    t = t->next;

    // foo_t
    if (!strview_eq(t->name, SV("foo_t"))) return TESTFAIL;
    if (t->typedef_id != 0) return TESTFAIL;
    if (!type_eq(t->type, (typeref_t){
        .size = 1,
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT, .n = 32 },
        },
    }, true)) return TESTFAIL;
    if (t->next) return TESTFAIL;

    return !test_expect_err;
}
GENERIC_TEST(test_filelvl_typedef3, test_expect_errcb)
    if (!cnm_parse(cnm, "typedef int foo_t, foo_t;",
                   "test_filelvl_typedef3")) return TESTFAIL;
    if (!cnm->type.typedefs) return TESTFAIL;
    if (cnm->type.typedef_gid != 1) return TESTFAIL;

    typedef_t *t = cnm->type.typedefs;

    // foo_t
    if (!strview_eq(t->name, SV("foo_t"))) return TESTFAIL;
    if (t->typedef_id != 0) return TESTFAIL;
    if (!type_eq(t->type, (typeref_t){
        .size = 1,
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT, .n = 32 },
        },
    }, true)) return TESTFAIL;
    if (t->next) return TESTFAIL;

    return !test_expect_err;
}
GENERIC_TEST(test_filelvl_typedef4, test_expect_errcb)
    if (cnm_parse(cnm, "typedef int foo_t, *foo_t;",
                   "test_filelvl_typedef4")) return TESTFAIL;
    return test_expect_err;
}
GENERIC_TEST(test_filelvl_typedef5, test_expect_errcb)
    if (!cnm_parse(cnm, "typedef int foo_t; typedef double bar_t;",
                   "test_filelvl_typedef5")) return TESTFAIL;
    if (!cnm->type.typedefs) return TESTFAIL;
    if (cnm->type.typedef_gid != 2) return TESTFAIL;

    typedef_t *t = cnm->type.typedefs;

    // bar_t
    if (!strview_eq(t->name, SV("bar_t"))) return TESTFAIL;
    if (t->scope != 0) return TESTFAIL;
    if (t->typedef_id != 1) return TESTFAIL;
    if (!type_eq(t->type, (typeref_t){
        .size = 1,
        .type = (type_t[]){
            (type_t){ .class = TYPE_DOUBLE },
        },
    }, true)) return TESTFAIL;
    if (!t->next) return TESTFAIL;
    t = t->next;

    // foo_t
    if (!strview_eq(t->name, SV("foo_t"))) return TESTFAIL;
    if (t->typedef_id != 0) return TESTFAIL;
    if (!type_eq(t->type, (typeref_t){
        .size = 1,
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT, .n = 32 },
        },
    }, true)) return TESTFAIL;
    if (t->next) return TESTFAIL;

    return !test_expect_err;
}

///////////////////////////////////////////////////////////////////////////////
//
// Global Variable Parsing Tests
//
///////////////////////////////////////////////////////////////////////////////
GENERIC_TEST(test_global_variable1, test_expect_errcb)
    if (!cnm_parse(cnm, "int a;", "test_global_variable1")) return TESTFAIL;
    scope_t *var = cnm->vars;
    if (!var) return TESTFAIL;
    if (var->scope != 0) return TESTFAIL;
    if (var->abs_addr != NULL) return TESTFAIL;
    if (!strview_eq(var->name, SV("a"))) return TESTFAIL;
    if (!type_eq(var->type, (typeref_t){
        .size = 1,
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT, .n = 32 },
        },
    }, true)) return TESTFAIL;
    if (var->next != NULL) return TESTFAIL;
    return !test_expect_err;
}
GENERIC_TEST(test_global_variable2, test_expect_errcb)
    if (!cnm_parse(cnm, "int a, *b, c[2];", "test_global_variable2")) return TESTFAIL;

    // c
    scope_t *var = cnm->vars;
    if (!var) return TESTFAIL;
    if (var->scope != 0) return TESTFAIL;
    if (var->abs_addr != NULL) return TESTFAIL;
    if (!strview_eq(var->name, SV("c"))) return TESTFAIL;
    if (!type_eq(var->type, (typeref_t){
        .size = 2,
        .type = (type_t[]){
            (type_t){ .class = TYPE_ARR, .n = 2 },
            (type_t){ .class = TYPE_INT, .n = 32 },
        },
    }, true)) return TESTFAIL;
    if (var->next == NULL) return TESTFAIL;
    var = var->next;

    // b
    if (!strview_eq(var->name, SV("b"))) return TESTFAIL;
    if (!type_eq(var->type, (typeref_t){
        .size = 2,
        .type = (type_t[]){
            (type_t){ .class = TYPE_PTR },
            (type_t){ .class = TYPE_INT, .n = 32 },
        },
    }, true)) return TESTFAIL;
    if (var->next == NULL) return TESTFAIL;
    var = var->next;

    // a
    if (!strview_eq(var->name, SV("a"))) return TESTFAIL;
    if (!type_eq(var->type, (typeref_t){
        .size = 1,
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT, .n = 32 },
        },
    }, true)) return TESTFAIL;
    if (var->next != NULL) return TESTFAIL;

    return !test_expect_err;
}
GENERIC_TEST(test_global_variable3, test_expect_errcb)
    if (!cnm_parse(cnm, "int a = 69;", "test_global_variable3")) return TESTFAIL;
    scope_t *var = cnm->vars;
    if (!var) return TESTFAIL;
    if (var->scope != 0) return TESTFAIL;
    if (var->abs_addr != test_globals_a4 + 0) return TESTFAIL;
    if (!strview_eq(var->name, SV("a"))) return TESTFAIL;
    if (!type_eq(var->type, (typeref_t){
        .size = 1,
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT, .n = 32 },
        },
    }, true)) return TESTFAIL;
    if (memcmp(var->abs_addr, &(int){69}, sizeof(int)) != 0) return TESTFAIL;
    if (var->next != NULL) return TESTFAIL;
    return !test_expect_err;
}
GENERIC_TEST(test_global_variable4, test_expect_errcb)
    if (!cnm_parse(cnm, "int a = 1 << 4;", "test_global_variable4")) return TESTFAIL;
    scope_t *var = cnm->vars;
    if (!var) return TESTFAIL;
    if (var->scope != 0) return TESTFAIL;
    if (var->abs_addr != test_globals_a4 + 0) return TESTFAIL;
    if (!strview_eq(var->name, SV("a"))) return TESTFAIL;
    if (!type_eq(var->type, (typeref_t){
        .size = 1,
        .type = (type_t[]){
            (type_t){ .class = TYPE_INT, .n = 32 },
        },
    }, true)) return TESTFAIL;
    if (memcmp(var->abs_addr, &(int){16}, sizeof(int)) != 0) return TESTFAIL;
    if (var->next != NULL) return TESTFAIL;
    return !test_expect_err;
}
GENERIC_TEST(test_global_variable5, test_expect_errcb)
    if (!cnm_parse(cnm, "int a; int a;", "test_global_variable5")) return TESTFAIL;
    return !test_expect_err;
}
GENERIC_TEST(test_global_variable6, test_expect_errcb)
    if (!cnm_parse(cnm, "int a; int b;", "test_global_variable6")) return TESTFAIL;
    return !test_expect_err;
}
GENERIC_TEST(test_global_variable7, test_expect_errcb)
    if (!cnm_parse(cnm, "int;", "test_global_variable7")) return TESTFAIL;
    return test_expect_err;
}
GENERIC_TEST(test_global_variable8, test_expect_errcb)
    if (cnm_parse(cnm, "int a; char a;", "test_global_variable8")) return TESTFAIL;
    return test_expect_err;
}
GENERIC_TEST(test_global_variable9, test_expect_errcb)
    if (cnm_parse(cnm, "int a = 1; int a = 2;", "test_global_variable9")) return TESTFAIL;
    return test_expect_err;
}
GENERIC_TEST(test_global_variable10, test_expect_errcb)
    if (!cnm_parse(cnm, "const char *str = \"hello world\";",
                        "test_global_variable10")) return TESTFAIL;
    scope_t *var = cnm->vars;
    if (!var) return TESTFAIL;
    if (var->scope != 0) return TESTFAIL;
    if (!strview_eq(var->name, SV("str"))) return TESTFAIL;
    if (!type_eq(var->type, (typeref_t){
        .size = 2,
        .type = (type_t[]){
            (type_t){ .class = TYPE_PTR },
            (type_t){ .class = TYPE_CHAR, .n = 8, .isconst = true },
        },
    }, true)) return TESTFAIL;
    if (memcmp(var->abs_addr, &(void *){*(void **)var->abs_addr},
               sizeof(void *)) != 0) return TESTFAIL;
    if (var->next != NULL) return TESTFAIL;
    return !test_expect_err;
}
GENERIC_TEST(test_global_variable11, test_expect_errcb)
    if (!cnm_parse(cnm, "unsigned long long x = 3.4;",
                        "test_global_variable11")) return TESTFAIL;
    scope_t *var = cnm->vars;
    if (!var) return TESTFAIL;
    if (var->scope != 0) return TESTFAIL;
    if (var->abs_addr != test_globals_a8 + 0) return TESTFAIL;
    if (!strview_eq(var->name, SV("x"))) return TESTFAIL;
    if (!type_eq(var->type, (typeref_t){
        .size = 1,
        .type = (type_t[]){
            (type_t){ .class = TYPE_ULLONG, .n = 64 },
        },
    }, true)) return TESTFAIL;
    if (memcmp(var->abs_addr, &(unsigned long long){3},
               sizeof(unsigned long long)) != 0) return TESTFAIL;
    if (var->next != NULL) return TESTFAIL;
    return !test_expect_err;
}
GENERIC_TEST(test_global_variable12, test_expect_errcb)
    if (cnm_parse(cnm, "int *p = 3.4;",
                        "test_global_variable12")) return TESTFAIL;
    return test_expect_err;
}
GENERIC_TEST(test_global_variable13, test_expect_errcb)
    if (!cnm_parse(cnm, "void *p = (void *)1234;",
                        "test_global_variable13")) return TESTFAIL;
    scope_t *var = cnm->vars;
    if (!var) return TESTFAIL;
    if (var->scope != 0) return TESTFAIL;
    if (var->abs_addr != test_globals_a8 + 0) return TESTFAIL;
    if (!strview_eq(var->name, SV("p"))) return TESTFAIL;
    if (!type_eq(var->type, (typeref_t){
        .size = 2,
        .type = (type_t[]){
            (type_t){ .class = TYPE_PTR },
            (type_t){ .class = TYPE_VOID },
        },
    }, true)) return TESTFAIL;
    if (memcmp(var->abs_addr, &(void *){(void *)1234}, sizeof(void *)) != 0) return TESTFAIL;
    if (var->next != NULL) return TESTFAIL;
    return !test_expect_err;
}

cnm(test_util_types2,
    struct test_vec2 {
        float x;
        float y;
    };
    struct test_obj {
        struct test_vec2 pos;
        char name[8];
        union {
            int plr_lives;
            int imp_target;
        } data;
        struct {
            float x, y, w, h;
        } col;
        int type;
    };
)

cnm(test_util_def1,
    struct test_obj test_obj1 = {
        .pos.x = 4.0, 5.0,
        "hello!",
    };
)
cnm(test_util_def2,
    struct test_obj test_obj2 = {
        .pos.x = 4.0, 5.0,
        .name = "hello!",
        .data.imp_target = 3,
        .col = {
            .x = 420,
            .y = 69,
            .w = 1337,
            .h = 911,
        },
    };
)
cnm(test_util_def3,
    int test_arr3[] = { 1, 2, 3, 4, 5 };
)
cnm(test_util_def4,
    int test_arr4[5] = { 1, 2, 1.0, };
)
cnm(test_util_def5,
    struct test_vec2 test_arr5[] = {
        (struct test_vec2) { .x = 69, .y = 420 },
        { .y = 69, },
        { 420, 69 },
    };
)
cnm(test_util_def6,
    char test_arr6[] = "ballz in yo jaws";
)

static cnm_t *test_util_create_types2(const char *fname) {
    cnm_t *cnm = cnm_init(test_region, sizeof(test_region),
                          test_code_area, test_code_size,
                          test_globals, sizeof(test_globals));
    cnm_set_errcb(cnm, test_errcb);
    return cnm_parse(cnm, cnm_csrc_test_util_types2, fname) ? cnm : NULL;
}
static bool test_global_variable14(void) {
    cnm_t *cnm = test_util_create_types2("test_global_variable14");
    if (!cnm_parse(cnm, cnm_csrc_test_util_def1, "test_global_variable14")) return TESTFAIL;
    scope_t *var = cnm->vars;
    if (!var) return TESTFAIL;
    if (!type_eq(var->type, (typeref_t){
        .size = 1,
        .type = (type_t[]){
            (type_t){ .class = TYPE_USER, .n = 1 },
        },
    }, true)) return TESTFAIL;
    if (memcmp(var->abs_addr, &test_obj1, sizeof(struct test_obj)) != 0) return TESTFAIL;
    return true;
}
static bool test_global_variable15(void) {
    cnm_t *cnm = test_util_create_types2("test_global_variable15");
    if (!cnm_parse(cnm, cnm_csrc_test_util_def2, "test_global_variable15")) return TESTFAIL;
    scope_t *var = cnm->vars;
    if (!var) return TESTFAIL;
    if (!type_eq(var->type, (typeref_t){
        .size = 1,
        .type = (type_t[]){
            (type_t){ .class = TYPE_USER, .n = 1 },
        },
    }, true)) return TESTFAIL;
    if (memcmp(var->abs_addr, &test_obj2, sizeof(struct test_obj)) != 0) return TESTFAIL;
    return true;
}
static bool test_global_variable16(void) {
    cnm_t *cnm = test_util_create_types2("test_global_variable16");
    if (!cnm_parse(cnm, cnm_csrc_test_util_def3, "test_global_variable16")) return TESTFAIL;
    scope_t *var = cnm->vars;
    if (!var) return TESTFAIL;
    if (!type_eq(var->type, (typeref_t){
        .size = 2,
        .type = (type_t[]){
            (type_t){ .class = TYPE_ARR, .n = 5 },
            (type_t){ .class = TYPE_INT, .n = 32 },
        },
    }, true)) return TESTFAIL;
    if (memcmp(var->abs_addr, &test_arr3, sizeof(test_arr3)) != 0) return TESTFAIL;
    return true;
}
static bool test_global_variable17(void) {
    cnm_t *cnm = test_util_create_types2("test_global_variable17");
    if (!cnm_parse(cnm, cnm_csrc_test_util_def4, "test_global_variable17")) return TESTFAIL;
    scope_t *var = cnm->vars;
    if (!var) return TESTFAIL;
    if (!type_eq(var->type, (typeref_t){
        .size = 2,
        .type = (type_t[]){
            (type_t){ .class = TYPE_ARR, .n = 5 },
            (type_t){ .class = TYPE_INT, .n = 32 },
        },
    }, true)) return TESTFAIL;
    if (memcmp(var->abs_addr, &test_arr4, sizeof(test_arr4)) != 0) return TESTFAIL;
    return true;
}
static bool test_global_variable18(void) {
    cnm_t *cnm = test_util_create_types2("test_global_variable18");
    if (!cnm_parse(cnm, cnm_csrc_test_util_def5, "test_global_variable18")) return TESTFAIL;
    scope_t *var = cnm->vars;
    if (!var) return TESTFAIL;
    if (!type_eq(var->type, (typeref_t){
        .size = 2,
        .type = (type_t[]){
            (type_t){ .class = TYPE_ARR, .n = 3 },
            (type_t){ .class = TYPE_USER, .n = 0 },
        },
    }, true)) return TESTFAIL;
    if (memcmp(var->abs_addr, &test_arr5, sizeof(test_arr5)) != 0) return TESTFAIL;
    return true;
}
GENERIC_TEST(test_global_variable19, test_errcb)
    if (!cnm_parse(cnm, cnm_csrc_test_util_def6, "test_global_variable19")) return TESTFAIL;
    scope_t *var = cnm->vars;
    if (!var) return TESTFAIL;
    if (!type_eq(var->type, (typeref_t){
        .size = 2,
        .type = (type_t[]){
            (type_t){ .class = TYPE_ARR, .n = 17 },
            (type_t){ .class = TYPE_CHAR, .n = 8 },
        },
    }, true)) return TESTFAIL;
    if (memcmp(var->abs_addr, &test_arr6, sizeof(test_arr6)) != 0) return TESTFAIL;
    return true;
}
GENERIC_TEST(test_global_variable20, test_expect_errcb)
    if (cnm_parse(cnm, "char str[4] = \"asdfasdf\";", "test_global_variable20")) return TESTFAIL;
    return test_expect_err;
}
static bool test_global_variable21(void) {
    cnm_t *cnm = test_util_create_types2("test_global_variable21");
    cnm_set_errcb(cnm, test_expect_errcb);
    if (cnm_parse(cnm, "struct test_vec2 x = { 0, 3, 4 };", "test_global_variable21")) return TESTFAIL;
    return test_expect_err;
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
    TEST(test_lexer_string7),
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
    TEST(test_expr_constant_folding26),

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
    TEST(test_type_parsing10),
    TEST(test_type_parsing11),
    TEST(test_type_parsing12),
    TEST(test_type_parsing13),
    TEST(test_type_parsing14),
    TEST(test_type_parsing15),
    TEST(test_type_parsing16),
    TEST(test_type_parsing17),
    TEST(test_type_parsing18),
    TEST(test_type_parsing19),
    TEST(test_type_parsing20),
    TEST(test_type_parsing21),
    TEST(test_type_parsing22),

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
    TEST(test_typedef_parsing13),
    TEST(test_typedef_parsing14),
    TEST(test_typedef_parsing15),
    TEST(test_typedef_parsing16),
    TEST(test_typedef_parsing17),
    TEST(test_typedef_parsing18),
    TEST(test_typedef_parsing19),
    TEST(test_typedef_parsing20),

    // File level typedef parsing tests
    TEST_PADDING,
    TEST(test_filelvl_typedef1),
    TEST(test_filelvl_typedef2),
    TEST(test_filelvl_typedef3),
    TEST(test_filelvl_typedef4),
    TEST(test_filelvl_typedef5),

    // Global variable parsing tests
    TEST_PADDING,
    TEST(test_global_variable1),
    TEST(test_global_variable2),
    TEST(test_global_variable3),
    TEST(test_global_variable4),
    TEST(test_global_variable5),
    TEST(test_global_variable6),
    TEST(test_global_variable7),
    TEST(test_global_variable8),
    TEST(test_global_variable9),
    TEST(test_global_variable10),
    TEST(test_global_variable11),
    TEST(test_global_variable12),
    TEST(test_global_variable13),
    TEST(test_global_variable14),
    TEST(test_global_variable15),
    TEST(test_global_variable16),
    TEST(test_global_variable17),
    TEST(test_global_variable18),
    TEST(test_global_variable19),
    TEST(test_global_variable20),
    TEST(test_global_variable21),
};

int main(int argc, char **argv) {
    printf("cnm tester\n");
   
    test_code_size = 2048;
    test_code_area = mmap(NULL, test_code_size, PROT_EXEC | PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    test_globals_a4 = (uint8_t *)align_size((size_t)test_globals, 4);
    test_globals_a8 = (uint8_t *)align_size((size_t)test_globals, 8);

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

