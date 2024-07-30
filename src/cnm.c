#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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
#define T3(c1, n1, c2, n2, c3, n3) n1, n2, n3
#define T3_2(c1, n1, c2, n2, c3, n3, c4, n4) n1, n2, n3, n4
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
        char c; // TOKEN_CHAR
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

struct cnm_s {
    // Where to store our executable code
    struct {
        uint8_t *buf;
        size_t len;
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

cnm_t *cnm_init(void *region, size_t regionsz, void *code, size_t codesz) {
    if (regionsz < sizeof(cnm_t)) return NULL;
    
    cnm_t *cnm = region;
    cnm->code.buf = code;
    cnm->code.len = codesz;
    cnm->buflen = regionsz - sizeof(cnm_t);
    cnm->next_alloc = cnm->buf;

    return cnm;
}

