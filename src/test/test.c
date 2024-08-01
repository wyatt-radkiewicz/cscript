#include <stdio.h>

#include <sys/mman.h>

#include "../cnm.c"

static uint8_t scratch_buf[2048];
static void *code_area;
static size_t code_size;

static void default_print(int line, const char *verbose, const char *simple) {
    printf("%s", verbose);
}

///////////////////////////////////////////////////////////////////////////////
//
// Lexer testing
//
///////////////////////////////////////////////////////////////////////////////
static bool test_lexer_uninitialized(void) {
    cnm_t *cnm = cnm_init(scratch_buf, sizeof(scratch_buf), code_area, code_size);
    cnm_set_errcb(cnm, default_print);
    cnm_parse(cnm, "", "test_lexer_ident.cnm", NULL, 0);
    if (cnm->s.tok.type != TOKEN_UNINITIALIZED) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 1) return false;
    if (!strview_eq(cnm->s.tok.src, SV(""))) return false;
    return true;
}
static bool test_lexer_ident(void) {
    cnm_t *cnm = cnm_init(scratch_buf, sizeof(scratch_buf), code_area, code_size);
    cnm_set_errcb(cnm, default_print);
    cnm_parse(cnm, " _foo123_ ", "test_lexer_ident.cnm", NULL, 0);
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_IDENT) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 2) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 10) return false;
    if (!strview_eq(cnm->s.tok.src, SV("_foo123_"))) return false;
    return true;
}
static bool test_lexer_string1(void) {
    cnm_t *cnm = cnm_init(scratch_buf, sizeof(scratch_buf), code_area, code_size);
    cnm_set_errcb(cnm, default_print);
    cnm_parse(cnm, "\"hello \"", "test_lexer_string1.cnm", NULL, 0);
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_STRING) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 9) return false;
    if (!strview_eq(cnm->s.tok.src, SV("\"hello \""))) return false;
    if (strcmp(cnm->s.tok.s, "hello ") != 0) return false;
    return true;
}
static bool test_lexer_string2(void) {
    cnm_t *cnm = cnm_init(scratch_buf, sizeof(scratch_buf), code_area, code_size);
    cnm_set_errcb(cnm, default_print);
    cnm_parse(cnm, "\"hello \"  \"world\"", "test_lexer_string2.cnm", NULL, 0);
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_STRING) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 18) return false;
    if (!strview_eq(cnm->s.tok.src, SV("\"hello \"  \"world\""))) return false;
    if (strcmp(cnm->s.tok.s, "hello world") != 0) return false;
    return true;
}
static bool test_lexer_string3(void) {
    cnm_t *cnm = cnm_init(scratch_buf, sizeof(scratch_buf), code_area, code_size);
    cnm_set_errcb(cnm, default_print);
    cnm_parse(cnm, "\"hello \"\n"
                   "  \"world\"", "test_lexer_string3.cnm", NULL, 0);
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_STRING) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 2) return false;
    if (cnm->s.tok.end.col != 10) return false;
    if (!strview_eq(cnm->s.tok.src, SV("\"hello \"\n  \"world\""))) return false;
    if (strcmp(cnm->s.tok.s, "hello world") != 0) return false;
    return true;
}
static bool test_lexer_string4(void) {
    cnm_t *cnm = cnm_init(scratch_buf, sizeof(scratch_buf), code_area, code_size);
    cnm_set_errcb(cnm, default_print);
    cnm_parse(cnm, "\"hello \\\"world\"", "test_lexer_string4.cnm", NULL, 0);
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_STRING) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 16) return false;
    if (!strview_eq(cnm->s.tok.src, SV("\"hello \\\"world\""))) return false;
    if (strcmp(cnm->s.tok.s, "hello \"world") != 0) return false;
    return true;
}
static bool test_lexer_string5(void) {
    cnm_t *cnm = cnm_init(scratch_buf, sizeof(scratch_buf), code_area, code_size);
    cnm_set_errcb(cnm, default_print);
    cnm_parse(cnm, "\"hello \\u263A world\"", "test_lexer_string5.cnm", NULL, 0);
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_STRING) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 21) return false;
    if (!strview_eq(cnm->s.tok.src, SV("\"hello \\u263A world\""))) return false;
    if (strcmp(cnm->s.tok.s, "hello ☺ world") != 0) return false;
    return true;
}
static bool test_lexer_string6(void) {
    cnm_t *cnm = cnm_init(scratch_buf, sizeof(scratch_buf), code_area, code_size);
    cnm_set_errcb(cnm, default_print);
    cnm_parse(cnm, "\"hello ☺ world\"", "test_lexer_string6.cnm", NULL, 0);
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_STRING) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 16) return false;
    if (!strview_eq(cnm->s.tok.src, SV("\"hello ☺ world\""))) return false;
    if (strcmp(cnm->s.tok.s, "hello ☺ world") != 0) return false;
    return true;
}
static bool test_lexer_char1(void) {
    cnm_t *cnm = cnm_init(scratch_buf, sizeof(scratch_buf), code_area, code_size);
    cnm_set_errcb(cnm, default_print);
    cnm_parse(cnm, "'h'", "test_lexer_char1.cnm", NULL, 0);
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_CHAR) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 4) return false;
    if (!strview_eq(cnm->s.tok.src, SV("'h'"))) return false;
    if (cnm->s.tok.c != 'h') return false;
    return true;
}
static bool test_lexer_char2(void) {
    cnm_t *cnm = cnm_init(scratch_buf, sizeof(scratch_buf), code_area, code_size);
    cnm_set_errcb(cnm, default_print);
    cnm_parse(cnm, "'\\x41'", "test_lexer_char2.cnm", NULL, 0);
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_CHAR) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 7) return false;
    if (!strview_eq(cnm->s.tok.src, SV("'\\x41'"))) return false;
    if (cnm->s.tok.c != 'A') return false;
    return true;
}
static bool test_lexer_char3(void) {
    cnm_t *cnm = cnm_init(scratch_buf, sizeof(scratch_buf), code_area, code_size);
    cnm_set_errcb(cnm, default_print);
    cnm_parse(cnm, "'☺'", "test_lexer_char3.cnm", NULL, 0);
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_CHAR) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 4) return false;
    if (!strview_eq(cnm->s.tok.src, SV("'☺'"))) return false;
    if (cnm->s.tok.c != 0x263A) return false;
    return true;
}
static bool test_lexer_char4(void) {
    cnm_t *cnm = cnm_init(scratch_buf, sizeof(scratch_buf), code_area, code_size);
    cnm_parse(cnm, "'h '", "test_lexer_char4.cnm", NULL, 0);
    token_next(cnm);
    if (cnm->nerrs == 0) return false;
    return true;
}
static bool test_lexer_int1(void) {
    cnm_t *cnm = cnm_init(scratch_buf, sizeof(scratch_buf), code_area, code_size);
    cnm_set_errcb(cnm, default_print);
    cnm_parse(cnm, "120", "test_lexer_int1.cnm", NULL, 0);
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_INT) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 4) return false;
    if (!strview_eq(cnm->s.tok.src, SV("120"))) return false;
    if (cnm->s.tok.i.n != 120) return false;
    if (strcmp(cnm->s.tok.i.suffix, "") != 0) return false;
    return true;
}
static bool test_lexer_int2(void) {
    cnm_t *cnm = cnm_init(scratch_buf, sizeof(scratch_buf), code_area, code_size);
    cnm_set_errcb(cnm, default_print);
    cnm_parse(cnm, "69U", "test_lexer_int2.cnm", NULL, 0);
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_INT) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 4) return false;
    if (!strview_eq(cnm->s.tok.src, SV("69U"))) return false;
    if (cnm->s.tok.i.n != 69) return false;
    if (strcmp(cnm->s.tok.i.suffix, "u") != 0) return false;
    return true;
}
static bool test_lexer_int3(void) {
    cnm_t *cnm = cnm_init(scratch_buf, sizeof(scratch_buf), code_area, code_size);
    cnm_set_errcb(cnm, default_print);
    cnm_parse(cnm, "420ulL", "test_lexer_int3.cnm", NULL, 0);
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_INT) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 7) return false;
    if (!strview_eq(cnm->s.tok.src, SV("420ulL"))) return false;
    if (cnm->s.tok.i.n != 420) return false;
    if (strcmp(cnm->s.tok.i.suffix, "ull") != 0) return false;
    return true;
}
static bool test_lexer_int4(void) {
    cnm_t *cnm = cnm_init(scratch_buf, sizeof(scratch_buf), code_area, code_size);
    cnm_set_errcb(cnm, default_print);
    cnm_parse(cnm, "0xFF", "test_lexer_int4.cnm", NULL, 0);
    token_next(cnm);
    if (cnm->s.tok.type != TOKEN_INT) return false;
    if (cnm->s.tok.start.row != 1) return false;
    if (cnm->s.tok.start.col != 1) return false;
    if (cnm->s.tok.end.row != 1) return false;
    if (cnm->s.tok.end.col != 5) return false;
    if (!strview_eq(cnm->s.tok.src, SV("0xFF"))) return false;
    if (cnm->s.tok.i.n != 255) return false;
    if (strcmp(cnm->s.tok.i.suffix, "") != 0) return false;
    return true;
}
static bool test_lexer_int5(void) {
    cnm_t *cnm = cnm_init(scratch_buf, sizeof(scratch_buf), code_area, code_size);
    cnm_parse(cnm, "0xxF", "test_lexer_int5.cnm", NULL, 0);
    token_next(cnm);
    if (cnm->nerrs == 0) return false;
    return true;
}
static bool test_lexer_int6(void) {
    cnm_t *cnm = cnm_init(scratch_buf, sizeof(scratch_buf), code_area, code_size);
    cnm_parse(cnm, "420f", "test_lexer_int6.cnm", NULL, 0);
    token_next(cnm);
    if (cnm->nerrs == 0) return false;
    return true;
}
static bool test_lexer_double1(void) {
    cnm_t *cnm = cnm_init(scratch_buf, sizeof(scratch_buf), code_area, code_size);
    cnm_set_errcb(cnm, default_print);
    cnm_parse(cnm, "0.0", "test_lexer_double1.cnm", NULL, 0);
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
}
static bool test_lexer_double2(void) {
    cnm_t *cnm = cnm_init(scratch_buf, sizeof(scratch_buf), code_area, code_size);
    cnm_set_errcb(cnm, default_print);
    cnm_parse(cnm, "0.5", "test_lexer_double2.cnm", NULL, 0);
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
}
static bool test_lexer_double3(void) {
    cnm_t *cnm = cnm_init(scratch_buf, sizeof(scratch_buf), code_area, code_size);
    cnm_set_errcb(cnm, default_print);
    cnm_parse(cnm, "420.69", "test_lexer_double3.cnm", NULL, 0);
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
}
static bool test_lexer_double4(void) {
    cnm_t *cnm = cnm_init(scratch_buf, sizeof(scratch_buf), code_area, code_size);
    cnm_set_errcb(cnm, default_print);
    cnm_parse(cnm, "420.69f", "test_lexer_double4.cnm", NULL, 0);
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
}
static bool test_lexer_double5(void) {
    cnm_t *cnm = cnm_init(scratch_buf, sizeof(scratch_buf), code_area, code_size);
    cnm_set_errcb(cnm, default_print);
    cnm_parse(cnm, "42.", "test_lexer_double5.cnm", NULL, 0);
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
}
static bool test_lexer_double6(void) {
    cnm_t *cnm = cnm_init(scratch_buf, sizeof(scratch_buf), code_area, code_size);
    cnm_set_errcb(cnm, default_print);
    cnm_parse(cnm, "42.f", "test_lexer_double6.cnm", NULL, 0);
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
}
static bool test_lexer_double7(void) {
    cnm_t *cnm = cnm_init(scratch_buf, sizeof(scratch_buf), code_area, code_size);
    cnm_parse(cnm, "0.0asdf", "test_lexer_double7.cnm", NULL, 0);
    token_next(cnm);
    if (cnm->nerrs == 0) return false;
    return true;
}

#define TEST_LEXER_TOKEN(name_end, string, token_type) \
    static bool test_lexer_##name_end(void) { \
        cnm_t *cnm = cnm_init(scratch_buf, sizeof(scratch_buf), code_area, code_size); \
        cnm_set_errcb(cnm, default_print); \
        cnm_parse(cnm, string, "test_lexer_"#name_end".cnm", NULL, 0); \
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

static bool test_lexer_token_location(void) {
    cnm_t *cnm = cnm_init(scratch_buf, sizeof(scratch_buf), code_area, code_size);
    cnm_set_errcb(cnm, default_print);
    cnm_parse(cnm, ".,?:;()[]{}+ +=++\n"
                   "- -=--**=//=%%=^^=\n"
                   "~~=!!== ==& &=&&\n"
                   "| |=||< <=<<<<=> >=>>>>=\n"
                   "foo \"str\" \"ing\" 'A' 34u 0.0f",
                   "test_lexer_tokens.cnm", NULL, 0);
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

    return true;
}

typedef bool (*test_pfn_t)(void);
typedef struct test_s {
    test_pfn_t pfn;
    const char *name;
} test_t;

#define TEST(func_name) ((test_t){ .pfn = func_name, .name = #func_name })
static test_t tests[] = {
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
};

int main(int argc, char **argv) {
    printf("cnm tester\n");
   
    code_size = 2048;
    code_area = mmap(NULL, code_size, PROT_EXEC | PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANON, -1, 0);

    int passed = 0;
    for (int i = 0; i < arrlen(tests); i++) {
        printf("%-32s ", tests[i].name);
        if (tests[i].pfn()) {
            printf(" PASS \n");
            passed++;
        } else {
            printf("<FAIL>\n");
        }
    }

    printf("%d/%zu tests passing\n", passed, arrlen(tests));

    munmap(code_area, code_size);

    return 0;
}

