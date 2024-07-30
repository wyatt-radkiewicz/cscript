#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "cnm.h"

typedef struct strview_s {
    const char *str;
    size_t len;
} strview_t;

// Create a string view from a string literal
#define SV(s) ((strview_t){ .str = s, .len = sizeof(s) - 1 })

#define TOKENS \
    T0(TOKEN_IDENT) T0(TOKEN_STRING) T0(TOKEN_CHAR) T0(TOKEN_INT) T0(TOKEN_DOUBLE) \
    T1('\0', TOKEN_EOF) T1('.', TOKEN_DOT) T1(',', TOKEN_COMMA) \
    T1('?', TOKEN_COND) T1(':', TOKEN_COLON) T1(';', TOKEN_SEMICOLON) \
    T1('(', TOKEN_PAREN_L) T1(')', TOKEN_PAREN_R) \
    T1('[', TOKEN_BRACK_L) T1(']', TOKEN_BRACK_R) \
    T1('{', TOKEN_BRACE_L) T1('}', TOKEN_BRACE_R) \
    T3('+', TOKEN_PLUS, '=', TOKEN_PLUS_EQ, '+', TOKEN_PLUS_DBL) \
    T3('-', TOKEN_MINUS, '=', TOKEN_MINUS_EQ, '-', TOKEN_MINUS_DBL) \
    T2('*', TOKEN_STAR, '=', TOKEN_TIMES_EQ) \
    T2('/', TOKEN_DIVIDE, '=', TOKEN_DIVIDE_EQ) \
    T2('%', TOKEN_MODULO, '=', TOKEN_MODULO_EQ) \
    T2('^', TOKEN_BIT_XOR, '=', TOKEN_BIT_XOR_EQ) \
    T2('~', TOKEN_BIT_NOT, '=', TOKEN_BIT_NOT_EQ) \
    T2('!', TOKEN_NOT, '=', TOKEN_NOT_EQ) \
    T2('=', TOKEN_ASSIGN, '=', TOKEN_EQ_EQ) \
    T3('&', TOKEN_BIT_AND, '=', TOKEN_AND_EQ, '&', TOKEN_AND) \
    T3('|', TOKEN_BIT_OR, '=', TOKEN_OR_EQ, '&', TOKEN_OR) \
    T3_2('<', TOKEN_LESS, '=', TOKEN_LESS_EQ, '<', TOKEN_SHIFT_L, '=', TOKEN_SHIFT_L_EQ) \
    T3_2('>', TOKEN_GREATER, '=', TOKEN_GREATER_EQ, '<', TOKEN_SHIFT_R, '=', TOKEN_SHIFT_R_EQ)

typedef enum token_type_s {
#define T0(n1) n1,
#define T1(c1, n1) n1,
#define T2(c1, n1, c2, n2) n1, n2,
#define T3(c1, n1, c2, n2, c3, n3) n1, n2, n3,
#define T3_2(c1, n1, c2, n2, c3, n3, c4, n4) n1, n2, n3, n4,
TOKENS
#undef T3_2
#undef T3
#undef T2
#undef T1
#undef T0
} token_type_t;

typedef struct token_s {
    // Type of the token
    token_type_t type;

    // The raw source of the token
    strview_t src; // TOKEN_IDENT

    // Position where the token starts
    int row, col;

    // Data associated with token type
    union {
        char *s; // TOKEN_STRING
        uint32_t c; // TOKEN_CHAR
        uintmax_t i; // TOKEN_INT
        double f; // TOKEN_DOUBLE
    };
} token_t;

typedef enum typeclass_s {
    TYPE_VOID,

    // POD Types
    TYPE_CHAR,      TYPE_UCHAR,
    TYPE_SHORT,     TYPE_USHORT,
    TYPE_INT,       TYPE_UINT,
    TYPE_LONG,      TYPE_ULONG,
    TYPE_LLONG,     TYPE_ULLONG,
    TYPE_FLOAT,     TYPE_DOUBLE,

    // Special types
    TYPE_PTR,       TYPE_REF,
    TYPE_ANYREF,    TYPE_BOOL,
    TYPE_ARR,       TYPE_USER,
} typeclass_t;

typedef struct type_s {
    typeclass_t class : 5; // what type of type it is
    unsigned int isconst : 1; // is it const?
    
    // Could mean type ID or array length
    unsigned int n : 24;
} type_t;

// Types are composed of multiple 'layers'. For instance an int would take up
// 1 layer, but a int * would take up 2 layers. Type refrences are simply
// slices into an array of type 'layers'.
typedef struct typeref_s {
    type_t *type;
    size_t size;
} typeref_t;

// Field of a struct.
typedef struct field_s {
    strview_t name;
    size_t offs;
    typeref_t type;
} field_t;

typedef struct cnm_struct_s {
    strview_t name;

    // Type ID assocciated with this struct
    int typeid;

    // Cached size and alignment
    size_t size, align;

    // Fields
    field_t *fields;
    size_t nfields;

    // Next allocated struct
    struct cnm_struct_s *next;
} struct_t;

// Unions in cnm script are essentially unfunctional because unions without
// tags are very dangerous. You can recreate unions pretty easy through the use
// of anyrefs if you want them to be user extensible, or create functions that
// return pointers if you want to as well.
typedef struct union_s {
    strview_t name;

    // Type ID associated with this
    int typeid;

    // Cached size and alignment
    size_t size, align;

    // Next allocated union
    struct union_s *next;
} union_t;

typedef struct variant_s {
    strview_t name;

    // What number the enum variant has associated with it
    int id;
} variant_t;

typedef struct cnm_enum_s {
    strview_t name;

    // Type ID assocciated with this type
    int typeid;

    // Enum variants
    variant_t *variants;
    size_t nvariants;

    // Next enum in the enum list
    struct cnm_enum_s *next;
} enum_t;

// Typedefs don't have type IDs assocciated with them since typedefs are
// substituted out for their real type during parsing of types in the source.
typedef struct typedef_s {
    strview_t name;
    typeref_t type;

    // Next typedef in the typedef list
    struct typedef_s *next;
} typedef_t;

// String entries are refrences to strings stored in the code segement
typedef struct strent_s {
    char *str;
    struct strent_s *next;
} strent_t;

struct cnm_s {
    // Where to store our executable code
    struct {
        uint8_t *buf;
        size_t len;

        uint8_t *global_ptr;
        uint8_t *code_ptr;
    } code;

    struct {
        // Logical 'file' name
        const char *fname;

        // Begining of the source
        const char *src;

        // Where we are in the source code
        token_t tok;
    } s;

    // Callbacks
    struct {
        cnm_err_cb_t err;
        cnm_dbg_cb_t dbg;
        cnm_fnaddr_cb_t fnaddr;
    } cb;

    // Number of errors encountered
    int nerrs;

    // Custom type info
    struct {
        struct_t *structs;
        union_t *unions;
        enum_t *enums;
        typedef_t *typedefs;
    } type;

    // Where the next allocation will be served
    uint8_t *next_alloc;

    // String entries
    strent_t *strs;

    // The actual buffer we use to allocate from
    size_t buflen;
    uint8_t buf[];
};

static bool strview_eq(const strview_t lhs, const strview_t rhs) {
    if (lhs.len != rhs.len) return false;
    return memcmp(lhs.str, rhs.str, lhs.len) == 0;
}

static void *cnm_alloc(cnm_t *cnm, size_t size) {
    if (cnm->next_alloc + size > cnm->buf + cnm->buflen) {
        return NULL;
    }
    void *const ptr = cnm->next_alloc;
    cnm->next_alloc += size;
    return ptr;
}

static void cnm_doerr(cnm_t *cnm, bool critical, const char *desc, const char *caption) {
    static char buf[512];

    cnm->nerrs += critical;
    if (!cnm->cb.err) return;

    int len = snprintf(buf, sizeof(buf), "error: %s\n"
                                         "  --> %s:%d:%d\n"
                                         "   |\n"
                                         "%-3d|  ",
                                         desc, cnm->s.fname,
                                         cnm->s.tok.row, cnm->s.tok.col,
                                         cnm->s.tok.row);
    // Get the source code line
    {
        const char *l = cnm->s.src;
        for (int line = 1; line != cnm->s.tok.row; line += *l++ == '\n');
        for (; *l != '\n' && *l != '\0' && len < sizeof(buf); buf[len++] = *l++);
        if (len == sizeof(buf)) goto overflow;
    }

    len += snprintf(buf + len, sizeof(buf) - len, "\n   |  ");
    if (len + cnm->s.tok.col + cnm->s.tok.src.len >= sizeof(buf)) goto overflow;
    memset(buf + len, ' ', cnm->s.tok.col);
    len += cnm->s.tok.col;
    memset(buf + len, critical ? '~' : '-', cnm->s.tok.src.len);
    len += cnm->s.tok.src.len;
    len += snprintf(buf + len, sizeof(buf) - len, " %s\n   |\n", caption);

    cnm->cb.err(cnm->s.tok.row, buf, desc);
    return;
overflow:
    cnm->cb.err(cnm->s.tok.row, desc, desc);
    return;
}

static token_t *token_ident(cnm_t *cnm) {
    while (isalnum(cnm->s.tok.src.str[cnm->s.tok.src.len])
           || cnm->s.tok.src.str[cnm->s.tok.src.len] == '_') {
        ++cnm->s.tok.src.len;
    }
    return &cnm->s.tok;
}

// Returns how big the character is in bytes
static size_t lex_char(const uint8_t **str, uint8_t out[4]) {
    if (**str != '\\') {
        if ((**str & 0x80) == 0x00) {
            out[0] = *(*str)++;
            return 1;
        } else if ((**str & 0xE0) == 0xC0) {
            out[0] = *(*str)++;
            out[1] = *(*str)++;
            return 2;
        } else if ((**str & 0xF0) == 0xE0) {
            out[0] = *(*str)++;
            out[1] = *(*str)++;
            out[2] = *(*str)++;
            return 3;
        } else if ((**str & 0xF8) == 0xF0) {
            out[0] = *(*str)++;
            out[1] = *(*str)++;
            out[2] = *(*str)++;
            out[3] = *(*str)++;
            return 4;
        }
    }

    // This is an escape sequence
    int base, nchrs;
    bool multi_byte;
    switch (*(*str)++) {
    case 'a': out[0] = '\a'; return 1;
    case 'b': out[0] = '\b'; return 1;
    case 'f': out[0] = '\f'; return 1;
    case 'n': out[0] = '\n'; return 1;
    case 'r': out[0] = '\r'; return 1;
    case 't': out[0] = '\t'; return 1;
    case 'v': out[0] = '\v'; return 1;
    case '\\': out[0] = '\\'; return 1;
    case '\'': out[0] = '\''; return 1;
    case '\"': out[0] = '\"'; return 1;
    case '\?': out[0] = '\?'; return 1;
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        base = 8;
        nchrs = 3;
        multi_byte = false;
        break;
    case 'x':
        base = 16;
        nchrs = -1;
        multi_byte = false;
        break;
    case 'u':
        base = 16;
        nchrs = 4;
        multi_byte = true;
        break;
    case 'U':
        base = 16;
        nchrs = 8;
        multi_byte = true;
        break;
    }

    uint32_t result = 0, pow = 1;
    for (; nchrs != 0; nchrs--) {
        const char c = tolower(*(*str)++);
        uint32_t digit = c - '0';
        if ((c >= 'a' || c <= 'f') && base == 16) digit = c - 'a' + 10;
        else if (!isdigit(c)) break;
        result += pow * digit;
        pow *= base;
    }

    if (multi_byte) {
        if (result < 0x80) {
            out[0] = result;
            return 1;
        } else if (result < 0x800) {
            out[0] = 0xC0 | (result >> 6 & 0x1F);
            out[1] = 0x80 | (result & 0x3F);
            return 2;
        } else if (result < 0x10000) {
            out[0] = 0xE0 | (result >> 12 & 0x0F);
            out[1] = 0x80 | (result >> 6 & 0x3F);
            out[2] = 0x80 | (result & 0x3F);
            return 3;
        } else {
            out[0] = 0xF0 | (result >> 18 & 0x07);
            out[1] = 0x80 | (result >> 12 & 0x3F);
            out[2] = 0x80 | (result >> 6 & 0x3F);
            out[4] = 0x80 | (result & 0x3F);
            return 4;
        }
    } else {
        out[0] = result;
        return 1;
    }
}

static token_t *token_string(cnm_t *cnm) {
    // TODO: Do token strings
    return &cnm->s.tok;
}

static token_t *token_char(cnm_t *cnm) {
    uint8_t buf[4];
    const uint8_t *c = (const uint8_t *)cnm->s.tok.src.str + 1;
    lex_char(&c, buf);

    cnm->s.tok.type = TOKEN_CHAR;
    cnm->s.tok.src.len = (uintptr_t)c - (uintptr_t)cnm->s.tok.src.str;
    if (cnm->s.tok.src.str[cnm->s.tok.src.len] != '\'') {
        cnm_doerr(cnm, true, "expected ending \' after character", "here");
        cnm->s.tok.type = TOKEN_EOF;
        return &cnm->s.tok;
    }
    cnm->s.tok.src.len++;

    cnm->s.tok.c = 0;
    if ((buf[0] & 0x80) == 0x00) {
        cnm->s.tok.c = buf[0];
    } else if ((buf[0] & 0xE0) == 0xC0) {
        cnm->s.tok.c |= (buf[0] & 0x1F) << 6;
        cnm->s.tok.c |= buf[1] & 0x3F;
    } else if ((buf[0] & 0xF0) == 0xE0) {
        cnm->s.tok.c |= (buf[0] & 0x0F) << 12;
        cnm->s.tok.c |= (buf[1] & 0x3F) << 6;
        cnm->s.tok.c |= buf[2] & 0x3F;
    } else if ((buf[0] & 0xF8) == 0xF0) {
        cnm->s.tok.c |= (buf[0] & 0x07) << 18;
        cnm->s.tok.c |= (buf[1] & 0x3F) << 12;
        cnm->s.tok.c |= (buf[2] & 0x3F) << 6;
        cnm->s.tok.c |= buf[3] & 0x3F;
    }

    return &cnm->s.tok;
}

static token_t *token_number(cnm_t *cnm) {
    // Look for dot that signifies double
    cnm->s.tok.type = TOKEN_INT;
    while (isalnum(cnm->s.tok.src.str[cnm->s.tok.src.len])
           || cnm->s.tok.src.str[cnm->s.tok.src.len] == '.') {
        if (cnm->s.tok.src.str[cnm->s.tok.src.len] == '.') {
            cnm->s.tok.type = TOKEN_DOUBLE;
        }
        ++cnm->s.tok.src.len;
    }

    if (cnm->s.tok.type == TOKEN_INT) {
        cnm->s.tok.i = strtoumax(cnm->s.tok.src.str, NULL, 0);
    } else {
        cnm->s.tok.f = strtod(cnm->s.tok.src.str, NULL);
    }

    return &cnm->s.tok;
}

static token_t *token_next(cnm_t *cnm) {
    if (cnm->s.tok.type == TOKEN_EOF) return &cnm->s.tok;

    // Skip whitespace
    cnm->s.tok.src.str += cnm->s.tok.src.len;
    cnm->s.tok.col += cnm->s.tok.src.len;
    for (; isspace(*cnm->s.tok.src.str); ++cnm->s.tok.src.str) {
        ++cnm->s.tok.col;
        if (*cnm->s.tok.src.str == '\n') {
            cnm->s.tok.col = 1;
            ++cnm->s.tok.row;
        }
    }
    cnm->s.tok.src.len = 1;
    
    switch (cnm->s.tok.src.str[0]) {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        return token_number(cnm);
    case '\"':
        return token_string(cnm);
    case '\'':
        return token_char(cnm);
    default:
        if (!isalpha(cnm->s.tok.src.str[0]) && cnm->s.tok.src.str[0] != '_') {
            cnm_doerr(cnm, true, "unknown character while lexing", "here");
            cnm->s.tok.type = TOKEN_EOF;
            return &cnm->s.tok;
        }
        return token_ident(cnm);
#define T0(n1)
#define T1(c1, n1) \
    case c1: \
        cnm->s.tok.type = n1; \
        break;
#define T2(c1, n1, c2, n2) \
    case c1: \
        cnm->s.tok.type = n1; \
        if (cnm->s.tok.src.str[1] == c2) { \
            cnm->s.tok.type = n2; \
            cnm->s.tok.src.len = 2; \
        } \
        break;
#define T3(c1, n1, c2, n2, c3, n3) \
    case c1: \
        cnm->s.tok.type = n1; \
        if (cnm->s.tok.src.str[1] == c2) { \
            cnm->s.tok.type = n2; \
            cnm->s.tok.src.len = 2; \
        } else if (cnm->s.tok.src.str[1] == c3) { \
            cnm->s.tok.type = n3; \
            cnm->s.tok.src.len = 2; \
        } \
        break;
#define T3_2(c1, n1, c2, n2, c3, n3, c4, n4) \
    case c1: \
        cnm->s.tok.type = n1; \
        if (cnm->s.tok.src.str[1] == c2) { \
            cnm->s.tok.type = n2; \
            cnm->s.tok.src.len = 2; \
        } else if (cnm->s.tok.src.str[1] == c3) { \
            cnm->s.tok.type = n3; \
            cnm->s.tok.src.len = 2; \
            if (cnm->s.tok.src.str[1] == c4) { \
                cnm->s.tok.type = n4; \
                cnm->s.tok.src.len = 3; \
            } \
        } \
        break;
TOKENS
#undef T3_2
#undef T3
#undef T2
#undef T1
#undef T0
    }

    return &cnm->s.tok;
}

cnm_t *cnm_init(void *region, size_t regionsz, void *code, size_t codesz) {
    if (regionsz < sizeof(cnm_t)) return NULL;
    
    cnm_t *cnm = region;
    cnm->code.buf = code;
    cnm->code.len = codesz;
    cnm->code.global_ptr = cnm->code.buf + cnm->code.len;
    cnm->code.code_ptr = cnm->code.buf;
    cnm->buflen = regionsz - sizeof(cnm_t);
    cnm->next_alloc = cnm->buf;

    return cnm;
}

