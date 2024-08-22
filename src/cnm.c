#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "cnm.h"
#include "cnm_ir.h"

typedef struct strview_s {
    const char *str;
    size_t len;
} strview_t;

#define MAX_SCOPE_DEPTH 32

// Create a string view from a string literal
#define SV(s) ((strview_t){ .str = s, .len = sizeof(s) - 1 })

#define arrlen(a) (sizeof(a) / sizeof((a)[0]))

#define TOKENS \
    T0(TOKEN_IDENT) T0(TOKEN_STRING) T0(TOKEN_CHAR) T0(TOKEN_INT) \
    T0(TOKEN_UNINITIALIZED) T0(TOKEN_DOUBLE) T0(TOKEN_EOF) \
    T1('.', TOKEN_DOT) T1(',', TOKEN_COMMA) \
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
    T3('|', TOKEN_BIT_OR, '=', TOKEN_OR_EQ, '|', TOKEN_OR) \
    T3_2('<', TOKEN_LESS, '=', TOKEN_LESS_EQ, '<', TOKEN_SHIFT_L, '=', TOKEN_SHIFT_L_EQ) \
    T3_2('>', TOKEN_GREATER, '=', TOKEN_GREATER_EQ, '>', TOKEN_SHIFT_R, '=', TOKEN_SHIFT_R_EQ)

typedef enum token_type_e {
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
    TOKEN_MAX,
} token_type_t;

typedef struct token_s {
    // Type of the token
    token_type_t type;

    // The raw source of the token
    strview_t src; // TOKEN_IDENT

    // Position where the token starts
    struct {
        int row, col;
    } start;

    // Position after the token ends
    struct {
        int row, col;
    } end;

    // Data associated with token type
    union {
        char *s; // TOKEN_STRING
        uint32_t c; // TOKEN_CHAR
        struct {
            int base;
            uintmax_t n;
        } i; // TOKEN_INT
        double f; // TOKEN_DOUBLE
    };

    // Suffix for data token type
    char suffix[4];
} token_t;

typedef struct type_s {
    typeclass_t class : 5; // what type of type it is
    unsigned int isconst : 1; // is it const?
    unsigned int isstatic : 1;
    unsigned int isextern : 1;

    // Could mean type ID, array length, or how long this argument is in layers
    unsigned int n : 24;
} type_t;

// Types are composed of multiple 'layers'. For instance an int would take up
// 1 layer, but a int * would take up 2 layers. Type refrences are simply
// slices into an array of type 'layers'.
typedef struct typeref_s {
    type_t *type;
    size_t size;
} typeref_t;

#define typeref_isvalid(tr) ((tr).type)

// Simple size and alignment info of a type at runtime
typedef struct typeinf_s {
    size_t size;
    size_t align;
} typeinf_t;

// Field of a struct.
typedef struct field_s {
    strview_t name;
    size_t offs; // Offset from begining of struct
    size_t bit_offs; // Used in bitfields
    typeref_t type; // Actual base type (plus bit field width)
    struct field_s *next; // Next field (fields are stored in reverse order)
} field_t;

typedef enum userty_class_e {
    USER_STRUCT,
    USER_ENUM,
    USER_UNION,
} userty_class_t;

typedef struct userty_s {
    strview_t name;

    userty_class_t type;

    // Type ID assocciated with this user made type
    int typeid;

    // What scope ID this type is a part of
    int scope;

    // Cached size and alignment
    typeinf_t inf;

    // Next usertype in the list
    struct userty_s *next;

    uint8_t data[];
} userty_t;

typedef struct cnm_struct_s {
    // Fields
    field_t *fields;
} struct_t;

typedef struct variant_s {
    strview_t name;

    // What number the enum variant has associated with it
    union {
        intmax_t i;
        uintmax_t u;
    } id; // Unsigned or not based on base enum type
} variant_t;

typedef struct cnm_enum_s {
    // What the default enum type is
    type_t type;

    // Enum variants
    variant_t *variants;
    size_t nvariants;
} enum_t;

// Typedefs don't have type IDs assocciated with them since typedefs are
// substituted out for their real type during parsing of types in the source.
typedef struct typedef_s {
    strview_t name;
    typeref_t type;
    int typedef_id;

    // What scope ID this type is a part of
    int scope;

    // Next typedef in the typedef list
    struct typedef_s *next;
} typedef_t;

// String entries are refrences to strings stored in the code segement
typedef struct strent_s {
    char *str;
    struct strent_s *next;
} strent_t;

// A named variable in the current scope
typedef struct scope_s {
    strview_t name;
    typeref_t type;

    // The range data associated with this scope
    // This updates to the latest range data so that it can be updated
    uidref_t *range;

    // The actual UID name of the variable (last one used)
    unsigned uid;

    // What scope is this variable apart of?
    int scope;

    // Only used for global/static variables because we need to know their
    // location at compile time, otherwise this variable is unsused
    void *abs_addr;

    // The next scope refrence. This one is 'later' than that
    struct scope_s *next;
} scope_t;

// Represents functions in cscript that can be called
struct cnm_fn_s {
    // The name of the function
    strview_t name;

    // Type of the function
    typeref_t type;

    // Actual address of the code of the function
    void *addr;

    // Next function in list of funcs
    struct cnm_fn_s *next;
};
typedef struct cnm_fn_s func_t;

// A refrence to a variable in the current program. Since this is IR, it could
// be on the stack or in a register so there is no associated location
// information with this
typedef struct {
    // If this type also has the literal data associated with it
    bool isliteral;
    union {
        // Integer constants
        uintmax_t u;
        intmax_t i;
        double f;
        uint32_t c;

        // Address to global data if it is a global variable
        void *addr;
    } literal;

    // The type of the value, copy of the scope type if this is a scoped var
    typeref_t type;

    // The range data associated with this scope
    // This updates to the latest range data so that it can be updated
    uidref_t *range;

    // The actual UID name of the variable (last one used)
    unsigned uid;

    // Pointer to the actual scope data of this variable
    scope_t *scope;
} valref_t;

// Precedence levels of an expression going from evaluated last (comma) to
// evaluated first (factors)
typedef enum prec_e {
    PREC_NONE,      PREC_FULL,
    PREC_COMMA,     PREC_ASSIGN,
    PREC_COND,      PREC_OR,
    PREC_AND,       PREC_BIT_OR,
    PREC_BIT_XOR,   PREC_BIT_AND,
    PREC_EQUALITY,  PREC_COMPARE,
    PREC_SHIFT,     PREC_ADDI,
    PREC_MULT,      PREC_PREFIX,
    PREC_POSTFIX,   PREC_FACTOR,
} prec_t;

// Used in parsing rules to create correct precedences and associativity and to
// parse tokens correctly
typedef bool (expr_parse_prefix_t)(cnm_t *cnm, valref_t *out, bool gencode, bool gendata);
typedef bool (expr_parse_infix_t)(cnm_t *cnm, valref_t *out, bool gencode, bool gendata, valref_t *left);
typedef expr_parse_prefix_t *expr_parse_prefix_pfn_t;
typedef expr_parse_infix_t *expr_parse_infix_pfn_t;

// Defines a rule for when a specific token is lexed
typedef struct {
    prec_t prefix_prec;
    expr_parse_prefix_pfn_t prefix;

    prec_t infix_prec;
    expr_parse_infix_pfn_t infix;
} expr_rule_t;

struct cnm_s {
    // Where to store our executable code
    struct {
        uint8_t *buf;
        size_t len;

        // If set to non-NULL, all refrences to buf are offset to point to here
        void *real_addr;
        uint8_t *ptr; // grows upward like how instructions are exectued
    } code;

    // Where to store globals
    struct {
        uint8_t *buf;
        size_t len;

        uint8_t *next; // Where the next global will be allocated
    } globals;

    struct {
        // Logical 'file' name
        const char *fname;

        // Begining of the source
        const char *src;

        // Where we are in the source code
        token_t tok;
    } s;

    // Code generation
    struct {
        // When compiling a function/arguments start at this UID id because
        // the global variables will be under this uid
        unsigned uid_start;

        // SSA Variable 'name' next global id
        unsigned uid;

        // UID refrences used in the IR
        uidref_t *uids;
    } cg;

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
        userty_t *types;
        typedef_t *typedefs;
        int gid, typedef_gid;
    } type;

    // Functions in scope
    func_t *funcs;

    // Variables in scope
    scope_t *vars;

    // What the current scope level is (grows up)
    int scope;

    struct {
        // Where the next allocation will be served (grows up)
        uint8_t *next;

        // Where the current static (infinite lifetime) allocation is being
        // served (grows down, starts at top)
        uint8_t *curr_static;
    } alloc;

    // String entries
    strent_t *strs;

    // The actual buffer we use to allocate from
    size_t buflen;
    uint8_t buf[];
};

static bool expr_parse(cnm_t *cnm, valref_t *out, bool gencode, bool gendata, prec_t prec);

static expr_parse_prefix_t expr_char;
static expr_parse_prefix_t expr_str;
static expr_parse_prefix_t expr_int;
static expr_parse_prefix_t expr_double;
static expr_parse_infix_t expr_arith;
static expr_parse_prefix_t expr_prefix_arith;
static expr_parse_prefix_t expr_group;

static expr_rule_t expr_rules[TOKEN_MAX] = {
    [TOKEN_PAREN_L] = { .prefix_prec = PREC_FACTOR, .prefix = expr_group },
    [TOKEN_INT] = { .prefix_prec = PREC_FACTOR, .prefix = expr_int },
    [TOKEN_CHAR] = { .prefix_prec = PREC_FACTOR, .prefix = expr_char },
    [TOKEN_STRING] = { .prefix_prec = PREC_FACTOR, .prefix = expr_str },
    [TOKEN_DOUBLE] = { .prefix_prec = PREC_FACTOR, .prefix = expr_double },
    [TOKEN_PLUS] = { .infix_prec = PREC_ADDI, .infix = expr_arith },
    [TOKEN_MINUS] = { .infix_prec = PREC_ADDI, .infix = expr_arith,
                      .prefix_prec = PREC_PREFIX, .prefix = expr_prefix_arith },
    [TOKEN_STAR] = { .infix_prec = PREC_MULT, .infix = expr_arith },
    [TOKEN_DIVIDE] = { .infix_prec = PREC_MULT, .infix = expr_arith },
    [TOKEN_MODULO] = { .infix_prec = PREC_MULT, .infix = expr_arith },
    [TOKEN_BIT_NOT] = { .prefix_prec = PREC_PREFIX, .prefix = expr_prefix_arith },
    [TOKEN_NOT] = { .prefix_prec = PREC_PREFIX, .prefix = expr_prefix_arith },
    [TOKEN_BIT_OR] = { .infix_prec = PREC_BIT_OR, .infix = expr_arith },
    [TOKEN_BIT_AND] = { .infix_prec = PREC_BIT_OR, .infix = expr_arith },
    [TOKEN_BIT_XOR] = { .infix_prec = PREC_BIT_OR, .infix = expr_arith },
    [TOKEN_SHIFT_L] = { .infix_prec = PREC_SHIFT, .infix = expr_arith },
    [TOKEN_SHIFT_R] = { .infix_prec = PREC_SHIFT, .infix = expr_arith },
};

static inline bool strview_eq(const strview_t lhs, const strview_t rhs) {
    if (lhs.len != rhs.len) return false;
    return memcmp(lhs.str, rhs.str, lhs.len) == 0;
}

// Print out error to the error callback in the cnm state
static void cnm_doerr(cnm_t *cnm, bool critical, const char *desc) {
    static char buf[512];

    // Only increase error count if critical
    cnm->nerrs += critical;
    if (!cnm->cb.err) return;

    // Print header
    int len = snprintf(buf, sizeof(buf), "%s: %s\n"
                                         "  --> %s:%d:%d\n"
                                         "%-3d|  ",
                                         critical ? "error" : "warning",
                                         desc, cnm->s.fname,
                                         cnm->s.tok.start.row,
                                         cnm->s.tok.start.col,
                                         cnm->s.tok.start.row);

    // Get the source code line
    {
        const char *l = cnm->s.src;
        for (int line = 1; line != cnm->s.tok.start.row; line += *l++ == '\n');
        for (; *l != '\n' && *l != '\0' && len < sizeof(buf); buf[len++] = *l++);
        if (len == sizeof(buf)) goto overflow;
    }

    // Print source code and next line with caption
    len += snprintf(buf + len, sizeof(buf) - len, "\n   |  ");
    if (len + cnm->s.tok.start.col + cnm->s.tok.src.len + 2 >= sizeof(buf)) goto overflow;
    memset(buf + len, ' ', cnm->s.tok.start.col); // spaces before caption
    len += cnm->s.tok.start.col;
    memset(buf + len, critical ? '~' : '-', cnm->s.tok.src.len); // underline
    len += cnm->s.tok.src.len;
    buf[len++] = '\n', buf[len++] = '\0';

    cnm->cb.err(cnm->s.tok.start.row, buf, desc);
    return;
overflow:
    cnm->cb.err(cnm->s.tok.start.row, desc, desc);
    return;
}

static inline size_t align_size(size_t x, size_t alignment) {
    return ((x - 1) / alignment + 1) * alignment;
}

// Allocate memory in the cnm state region
// Grows up
static void *cnm_alloc(cnm_t *cnm, size_t size, size_t align) {
    // Align the next alloc pointer
    cnm->alloc.next = (uint8_t *)(((uintptr_t)(cnm->alloc.next - 1) / align + 1) * align);

    // Check memory overflow
    if (cnm->alloc.next + size > cnm->alloc.curr_static) {
        cnm_doerr(cnm, true, "ran out of region memory");
        return NULL;
    }

    // Return new pointer
    void *const ptr = cnm->alloc.next;
    cnm->alloc.next += size;
    return ptr;
}

// Allocate memory in the cnm state region statically (IE whole program time)
// This region grows downward
static void *cnm_alloc_static(cnm_t *cnm, size_t size, size_t align) {
    // Align the next alloc pointer
    cnm->alloc.curr_static = (uint8_t *)(
        (uintptr_t)(cnm->alloc.curr_static - size) / align * align);

    // Check memory overflow
    if (cnm->alloc.curr_static <= cnm->alloc.next) {
        cnm_doerr(cnm, true, "ran out of region memory");
        return NULL;
    }

    // Return new pointer
    return cnm->alloc.curr_static;
}

// Allocate memory in the global buffer
// Grows up
static void *cnm_alloc_global(cnm_t *cnm, size_t size, size_t align) {
    // Align the next alloc pointer
    cnm->globals.next = (uint8_t *)(
            ((uintptr_t)(cnm->globals.next - 1) / align + 1) * align);

    // Check memory overflow
    if (cnm->globals.next + size > cnm->globals.buf + cnm->globals.len) {
        cnm_doerr(cnm, true, "ran out of globals memory");
        return NULL;
    }

    // Return new pointer
    void *const ptr = cnm->globals.next;
    cnm->globals.next += size;
    return ptr;
}

// Lex a identifier
static token_t *token_ident(cnm_t *cnm) {
    cnm->s.tok.type = TOKEN_IDENT;

    // Consume alphanumeric and _
    while (isalnum(cnm->s.tok.src.str[cnm->s.tok.src.len])
           || cnm->s.tok.src.str[cnm->s.tok.src.len] == '_') {
        ++cnm->s.tok.src.len;
    }

    // Set end of token
    cnm->s.tok.end.col += cnm->s.tok.src.len;
    return &cnm->s.tok;
}

// Checks a uint32_t to see if its in the range of a utf32
static bool token_utf32_validation(uint32_t cp) {
    return cp < 0xD800 || cp >= 0xE000 && cp < 0x110000;
}

// Checks a uint8_t to see if its in the range of a single utf8 code point
static bool token_utf8_single_validation(uint8_t cp) {
    return cp < 0x80;
}

// Lex a single character/escape code
// Returns how big the character is in bytes
// Will advance the string pointer to the next character
// nsrc_cols is the number of columns that the character takes up so A 
// ☺ is 1 column, while \u263A is 6 columns
static size_t lex_char(cnm_t *cnm, const uint8_t **str, uint8_t out[4], size_t *nsrc_cols) {
    // Lex normal character (no escape sequence)
    if (**str != '\\') {
        if (nsrc_cols) *nsrc_cols = 1;

        // UTF-8 formatting (straight copying bytes)
        if ((**str & 0x80) == 0x00) {
            if (!(out[0] = *(*str)++)) goto eof_error;
            return 1;
        } else if ((**str & 0xE0) == 0xC0) {
            if (!(out[0] = *(*str)++)) goto eof_error;
            if (!(out[1] = *(*str)++)) goto eof_error;
            return 2;
        } else if ((**str & 0xF0) == 0xE0) {
            if (!(out[0] = *(*str)++)) goto eof_error;
            if (!(out[1] = *(*str)++)) goto eof_error;
            if (!(out[2] = *(*str)++)) goto eof_error;
            return 3;
        } else if ((**str & 0xF8) == 0xF0) {
            if (!(out[0] = *(*str)++)) goto eof_error;
            if (!(out[1] = *(*str)++)) goto eof_error;
            if (!(out[2] = *(*str)++)) goto eof_error;
            if (!(out[3] = *(*str)++)) goto eof_error;
            return 4;
        }
    }

    // This is an escape sequence
    int base, nchrs;    // base of code point, and number of chars in code point number
    bool multi_byte;    // if this is a multi-byte (utf8) codepoint

    if (nsrc_cols) *nsrc_cols = 2;
    switch (*++(*str)) {
    // Check for eof
    case '\0': goto eof_error;

    // Normal escape sequences
    case 'a': out[0] = '\a'; ++(*str); return 1;
    case 'b': out[0] = '\b'; ++(*str); return 1;
    case 'f': out[0] = '\f'; ++(*str); return 1;
    case 'n': out[0] = '\n'; ++(*str); return 1;
    case 'r': out[0] = '\r'; ++(*str); return 1;
    case 't': out[0] = '\t'; ++(*str); return 1;
    case 'v': out[0] = '\v'; ++(*str); return 1;
    case '\\': out[0] = '\\'; ++(*str); return 1;
    case '\'': out[0] = '\''; ++(*str); return 1;
    case '\"': out[0] = '\"'; ++(*str); return 1;
    case '\?': out[0] = '\?'; ++(*str); return 1;

    // Codepoint sequences
    // Octal
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        base = 8;
        nchrs = 3;
        multi_byte = false;
        break;
    // Hex
    case 'x':
        base = 16;
        nchrs = 1;
        for (; (tolower((*str)[nchrs]) >= 'a' && tolower((*str)[nchrs]) <= 'f')
               || ((*str)[nchrs] >= '0' && (*str)[nchrs] <= '9'); nchrs++);
        nchrs--;
        if (!nchrs) {
            cnm_doerr(cnm, true, "\\x used with no following hex digits");
            goto error;
        }
        multi_byte = false;
        break;
    // UTF8 (all codepoints before 0x10000)
    case 'u':
        base = 16;
        nchrs = 4;
        multi_byte = true;
        break;
    // UTF8 (all codepoints)
    case 'U':
        base = 16;
        nchrs = 8;
        multi_byte = true;
        break;
    }

    // Read in the number into result
    // nchrs represents the number of characters left to read
    // if nchrs is 0, it will read until it finds a source char that doesn't
    // fit the base
    ++(*str);
    uint32_t result = 0, pow = 1;
    for (int i = nchrs - 1; i >= 0; i--) {
        const char c = tolower((*str)[i]);
        uint32_t digit = c - '0';
        if (c >= 'a' && c <= 'f' && base == 16) {
            digit = c - 'a' + 10;
        } else if (!isdigit(c)) {
            cnm_doerr(cnm, true, "malformed character literal char is invalid");
            goto error;
        }
        result += pow * digit;
        pow *= base;
    }
    *str += nchrs;
    if (nsrc_cols) *nsrc_cols += nchrs;

    if (!token_utf32_validation(result)) {
        cnm_doerr(cnm, true, "malformed character literal. "
                             "Does not exist in utf32 codespace");
        goto error;
    }

    // UTF8 codepoints
    if (multi_byte) {
        // UTF8 format (from u32 to bytes)
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
        // normal ASCII char
        out[0] = result;
        return 1;
    }

eof_error:
    cnm_doerr(cnm, true, "unexpected eof when parsing character literal");
error:
    cnm->s.tok.type = TOKEN_EOF;
    return 0;
}

// Set the token's string value (which is a uint32_t)
// Convert from UTF8 to codepoint
static bool utf8_buf_to_utf32(cnm_t *cnm, uint8_t buf[4], uint32_t *out) {
    *out = 0;
    if ((buf[0] & 0x80) == 0x00) {
        *out = buf[0];
    } else if ((buf[0] & 0xE0) == 0xC0) {
        *out |= (buf[0] & 0x1F) << 6;
        *out |= buf[1] & 0x3F;
    } else if ((buf[0] & 0xF0) == 0xE0) {
        *out |= (buf[0] & 0x0F) << 12;
        *out |= (buf[1] & 0x3F) << 6;
        *out |= buf[2] & 0x3F;
    } else if ((buf[0] & 0xF8) == 0xF0) {
        *out |= (buf[0] & 0x07) << 18;
        *out |= (buf[1] & 0x3F) << 12;
        *out |= (buf[2] & 0x3F) << 6;
        *out |= buf[3] & 0x3F;
    }

    if (!token_utf32_validation(*out)) {
        cnm_doerr(cnm, true, "malformed character literal, invalid utf32");
        return false;
    }

    return true;
}

// Lexes a single string so only the hello" part of "hello""world" 
// The src pointer should point to the start of the contents of the string
// Returns the length of the source of the string (including final " char)
// Puts the length of the string buffer into outlen (if its not NULL)
static size_t lex_single_string(cnm_t *cnm, const uint8_t *src, char *buf,
                                size_t *outlen, size_t *outncols) {
    const uint8_t *const src_start = src;
    uint8_t char_buf[4];
    size_t len = 0;

    // Consume all characters
    if (outncols) *outncols = 0;
    while (*src != '\"') {
        if (*src == '\0') {
            cnm_doerr(cnm, true, "unexpected eof while parsing string");
            goto error_out;
        } else if (*src == '\n') {
            cnm_doerr(cnm, true, "unexpected new line while parsing string");
            goto error_out;
        }

        size_t ncols, char_len = lex_char(cnm, &src, char_buf, &ncols);
        if (outncols) *outncols += ncols;
        if (!char_len) goto error_out;

        // Validate single-byte character strings
        if (cnm->s.tok.suffix[0] == '\0' && char_len > 1) {
            cnm_doerr(cnm, true, "character code point can not fit in normal string");
            goto error_out;
        }

        if (cnm->s.tok.suffix[0] == 'U') {
            // Handle utf-32
            uint32_t c;
            if (!utf8_buf_to_utf32(cnm, char_buf, &c)) goto error_out;
            if (buf) memcpy(buf + len, &c, sizeof(c));
            len += 4;
        } else {
            // Handle single byte or utf-8 encodings
            if (buf) memcpy(buf + len, char_buf, char_len);
            len += char_len;
        }
    }
    src++;

    if (outlen) *outlen = len;
    if (outncols) ++*outncols;
    return src - src_start;

error_out:
    cnm->s.tok.type = TOKEN_EOF;
    return 0;
}

// Move the token's src parameter and end and string start to after the prefix
static void token_eat_literal_prefix(cnm_t *cnm) {
    const unsigned len = strlen(cnm->s.tok.suffix);
    cnm->s.tok.end.col += len;
    cnm->s.tok.src.str += len;
}

// Parses a string token and automatically concatinates them
// this implies that the resultant token might span over multiple lines
static token_t *token_string(cnm_t *cnm) {
    token_eat_literal_prefix(cnm);

    size_t buflen = 0;
    const uint8_t *const start = (const uint8_t *)cnm->s.tok.src.str;
    const uint8_t *curr = start;

    // To revert the end spot and length if there wasn't another string to concatinate
    int row_backup, col_backup;
    size_t len_backup;

    // To revert global allocation buffer in case this string is already present
    uint8_t *const global_ptr = cnm->globals.next;

    // Get the length of the string token and the length of the stored string
    cnm->s.tok.type = TOKEN_STRING;
    cnm->s.tok.src.len = 0;
    do {
        curr++;
        cnm->s.tok.end.col++;
        cnm->s.tok.src.len++;

        size_t _buflen, _ncols, _srclen =
            lex_single_string(cnm, curr, NULL, &_buflen, &_ncols);
        if (!_srclen) goto error_scenario;

        cnm->s.tok.src.len += _srclen, curr += _srclen, buflen += _buflen;
        cnm->s.tok.end.col += _ncols;

        // Skip whitespace until we find the next string to concatinate (if there is one)
        row_backup = cnm->s.tok.end.row, col_backup = cnm->s.tok.end.col;
        len_backup = cnm->s.tok.src.len;
        while (isspace(*curr)) {
            cnm->s.tok.src.len++;
            cnm->s.tok.end.col++;
            if (*curr == '\n') {
                cnm->s.tok.end.row++;
                cnm->s.tok.end.col = 1;
            }
            ++curr;
        }
    } while (*curr == '\"');
    cnm->s.tok.end.row = row_backup, cnm->s.tok.end.col = col_backup;
    cnm->s.tok.src.len = len_backup;

    // Allocate space for the null terminator
    buflen += cnm->s.tok.suffix[0] == 'U' ? 4 : 1;

    // Allocate space for the string
    char *buf = cnm_alloc_global(cnm, buflen, cnm->s.tok.suffix[0] == 'U' ? 4 : 1);
    if (!buf) goto error_scenario;
    size_t bufloc = 0;

    // Fill out the new string buffer string by string
    curr = start;
    do {
        curr++;

        size_t len;
        curr += lex_single_string(cnm, curr, buf + bufloc, &len, NULL);
        bufloc += len;
        while (isspace(*curr)) ++curr;
    } while (*curr == '\"');
    if (cnm->s.tok.suffix[0] == 'U') memset(buf + bufloc, 0, 4);
    else buf[bufloc] = '\0';
    
    // Test to see if there is already a string with these contents
    for (const strent_t *ent = cnm->strs; ent; ent = ent->next) {
        if (strcmp(ent->str, buf)) continue;

        // We found exact copy of the string, so use that string instead
        cnm->globals.next = global_ptr;
        cnm->s.tok.s = ent->str;
        return &cnm->s.tok;
    }

    // String has not been found so add it to the string entries list
    strent_t *ent = cnm_alloc(cnm, sizeof(*ent), sizeof(void *));
    if (!ent) goto error_scenario;
    ent->str = buf;
    ent->next = cnm->strs;
    cnm->strs = ent;
    cnm->s.tok.s = ent->str;
    return &cnm->s.tok;

error_scenario:
    cnm->s.tok.type = TOKEN_EOF;
    return &cnm->s.tok;
}

// Parse a single character token
static token_t *token_char(cnm_t *cnm) {
    token_eat_literal_prefix(cnm);

    uint8_t buf[4];
    const uint8_t *c = (const uint8_t *)cnm->s.tok.src.str + 1;
    size_t ncols;
    if (!lex_char(cnm, &c, buf, &ncols)) goto error;

    // Set new token info
    cnm->s.tok.type = TOKEN_CHAR;
    cnm->s.tok.src.len = (uintptr_t)c - (uintptr_t)cnm->s.tok.src.str;

    // Error out
    if (cnm->s.tok.src.str[cnm->s.tok.src.len] != '\'') {
        cnm_doerr(cnm, true, "expected ending \' after character");
        cnm->s.tok.type = TOKEN_EOF;
        return &cnm->s.tok;
    }
    cnm->s.tok.src.len++;
    cnm->s.tok.end.col += ncols + 2;

    if (!utf8_buf_to_utf32(cnm, buf, &cnm->s.tok.c)) return false;

    if (cnm->s.tok.suffix[0] == 'u' && !token_utf8_single_validation(cnm->s.tok.c)) {
        // single utf8 byte
        cnm_doerr(cnm, true, "character literal goes over single utf8 byte");
    } else if (cnm->s.tok.suffix[0] == '\0' && cnm->s.tok.c > 0xff) {
        // Throw warning that this normal char literal is above 1-byte big
        cnm_doerr(cnm, false, "normal character literal is above range of 1 byte");
    }

error:
    return &cnm->s.tok;
}

// Parse a TOKEN_INT or TOKEN_DOUBLE
static token_t *token_number(cnm_t *cnm) {
    // Look for dot that signifies double and consume token length
    cnm->s.tok.type = TOKEN_INT;
    while (isalnum(cnm->s.tok.src.str[cnm->s.tok.src.len])
           || cnm->s.tok.src.str[cnm->s.tok.src.len] == '.') {
        if (cnm->s.tok.src.str[cnm->s.tok.src.len] == '.') {
            cnm->s.tok.type = TOKEN_DOUBLE;
        }
        ++cnm->s.tok.src.len;
    }

    // Get the the actual contents and parse suffix
    char *end;
    if (cnm->s.tok.type == TOKEN_INT) {
        size_t suffix_len = 0;

        if (cnm->s.tok.src.len > 2 && cnm->s.tok.src.str[0] == '0') {
            if (tolower(cnm->s.tok.src.str[1]) == 'x') cnm->s.tok.i.base = 16;
            else if (tolower(cnm->s.tok.src.str[1]) == 'b') cnm->s.tok.i.base = 2;
        } else if (cnm->s.tok.src.len > 1 && cnm->s.tok.src.str[0] == '0') {
            cnm->s.tok.i.base = 8;
        } else {
            cnm->s.tok.i.base = 10;
        }

        cnm->s.tok.i.n = strtoumax(cnm->s.tok.src.str + (cnm->s.tok.i.base == 2 ? 2 : 0),
                                   &end, cnm->s.tok.i.base);

        // Get suffix
        cnm->s.tok.suffix[suffix_len] = '\0';

        // Look for optional 'u'
        if (tolower(*end) == 'u') {
            cnm->s.tok.suffix[suffix_len++] = 'u';
            cnm->s.tok.suffix[suffix_len] = '\0';
            end++;
        }
        
        // Look for optional 2 l's
        for (int i = 0; i < 2; i++) {
            if (tolower(*end) == 'l') {
                cnm->s.tok.suffix[suffix_len++] = 'l';
                cnm->s.tok.suffix[suffix_len] = '\0';
                end++;
            }
        }
    } else {
        cnm->s.tok.f = strtod(cnm->s.tok.src.str, &end);

        // Look for optional 'f'
        if (tolower(*end) == 'f') {
            cnm->s.tok.suffix[0] = 'f';
            cnm->s.tok.suffix[1] = '\0';
            end++;
        } else {
            cnm->s.tok.suffix[0] = '\0';
        }
    }

    // Check for other characters after the suffix/numbers and error if so
    if (end != cnm->s.tok.src.str + cnm->s.tok.src.len) {
        cnm_doerr(cnm, true, "invalid number literal format, extra chars after digits");
        cnm->s.tok.type = TOKEN_EOF;
        return &cnm->s.tok;
    }

    cnm->s.tok.end.col += cnm->s.tok.src.len;
    return &cnm->s.tok;
}

// Move the cnm token to the next token
// If the current token is TOKEN_EOF, it will be repeated
static token_t *token_next(cnm_t *cnm) {
    if (cnm->s.tok.type == TOKEN_EOF) return &cnm->s.tok;

    // Skip whitespace
    cnm->s.tok.src.str += cnm->s.tok.src.len;
    cnm->s.tok.start.row = cnm->s.tok.end.row;
    cnm->s.tok.start.col = cnm->s.tok.end.col;
    for (; isspace(*cnm->s.tok.src.str); ++cnm->s.tok.src.str) {
        ++cnm->s.tok.start.col;
        if (*cnm->s.tok.src.str == '\n') {
            cnm->s.tok.start.col = 1;
            ++cnm->s.tok.start.row;
        }
    }
    cnm->s.tok.src.len = 1;
    cnm->s.tok.end.row = cnm->s.tok.start.row;
    cnm->s.tok.end.col = cnm->s.tok.start.col;
    
    switch (cnm->s.tok.src.str[0]) {
    // Special tokens
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        return token_number(cnm);
    case '\"':
        return token_string(cnm);
    case '\'':
        return token_char(cnm);
    case '\0':
        cnm->s.tok.type = TOKEN_EOF;
        cnm->s.tok.src.len = 0;
        break;
    default:
        // Unknown character
        if (!isalpha(cnm->s.tok.src.str[0]) && cnm->s.tok.src.str[0] != '_') {
            cnm_doerr(cnm, true, "unknown character while lexing");
            cnm->s.tok.type = TOKEN_EOF;
            return &cnm->s.tok;
        }

        // Prefix strings/characters
        if (cnm->s.tok.src.str[0] == 'u' && cnm->s.tok.src.str[1] == '8') {
            cnm->s.tok.suffix[0] = 'u';
            cnm->s.tok.suffix[1] = '8';
            cnm->s.tok.suffix[2] = '\0';
            if (cnm->s.tok.src.str[2] == '"') {
                return token_string(cnm);
            } else if (cnm->s.tok.src.str[2] == '\'') {
                return token_char(cnm);
            }
        } else if (cnm->s.tok.src.str[0] == 'U') {
            cnm->s.tok.suffix[0] = 'U';
            cnm->s.tok.suffix[1] = '\0';
            if (cnm->s.tok.src.str[1] == '"') {
                return token_string(cnm);
            } else if (cnm->s.tok.src.str[1] == '\'') {
                return token_char(cnm);
            }
        }

        // Identifiers
        cnm->s.tok.suffix[0] = '\0';
        return token_ident(cnm);

    // Simple tokens
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
            if (cnm->s.tok.src.str[2] == c4) { \
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

    cnm->s.tok.end.col += cnm->s.tok.src.len;
    return &cnm->s.tok;
}

static typeref_t type_parse(cnm_t *cnm, const type_t *base,
                            strview_t *name, bool allow_bitfields);
static bool type_parse_declspec(cnm_t *cnm, type_t *type, bool *istypedef);

// Returns true if this ast node can be used in an arithmetic operation
static bool type_is_arith(const type_t type) {
    switch (type.class) {
    case TYPE_ARR: case TYPE_USER: case TYPE_PTR: case TYPE_REF:
        return false;
    default:
        return true;
    }
}

// Returns whether or not a type is an integer type
static bool type_is_int(const type_t type) {
    return type.class >= TYPE_CHAR && type.class <= TYPE_ULLONG;
}

// Returns whether or not a type is a floating point type
static bool type_is_fp(const type_t type) {
    switch (type.class) {
    case TYPE_FLOAT: case TYPE_DOUBLE:
        return true;
    default:
        return false;
    }
}

// Returns whether or not the type is unsigned
static bool type_is_unsigned(const type_t type) {
    switch (type.class) {
    case TYPE_UCHAR: case TYPE_USHORT: case TYPE_UINT: case TYPE_ULONG: case TYPE_ULLONG:
        return true;
    default:
        return false;
    }
}

static bool type_is_pod(const type_t type) {
    switch (type.class) {
    case TYPE_UCHAR: case TYPE_USHORT: case TYPE_UINT: case TYPE_ULONG: case TYPE_ULLONG:
    case TYPE_CHAR: case TYPE_SHORT: case TYPE_INT: case TYPE_LONG: case TYPE_LLONG:
    case TYPE_FLOAT: case TYPE_DOUBLE: case TYPE_PTR: case TYPE_BOOL:
        return true;
    default:
        return false;
    }
}

static typeinf_t type_getinf(cnm_t *cnm, const type_t *type) {
    switch (type->class) {
    case TYPE_VOID: return (typeinf_t){ .size = 0, .align = 1 };
    case TYPE_CHAR: case TYPE_UCHAR: case TYPE_BOOL:
        return (typeinf_t){ sizeof(char), sizeof(char) };
    case TYPE_SHORT: case TYPE_USHORT:
        return (typeinf_t){ sizeof(short), sizeof(short) };
    case TYPE_INT: case TYPE_UINT:
        return (typeinf_t){ sizeof(int), sizeof(int) };
    case TYPE_LONG: case TYPE_ULONG:
        return (typeinf_t){ sizeof(long), sizeof(long) };
    case TYPE_LLONG: case TYPE_ULLONG:
        return (typeinf_t){ sizeof(long long), sizeof(long long) };
    case TYPE_FLOAT:
        return (typeinf_t){ sizeof(float), sizeof(float) };
    case TYPE_DOUBLE:
        return (typeinf_t){ sizeof(double), sizeof(double) };
    case TYPE_PTR:
        return (typeinf_t){ sizeof(void *), sizeof(void *) };
    case TYPE_REF:
        return (typeinf_t){ sizeof(cnmref_t), sizeof(void *) };
    case TYPE_ANYREF:
        return (typeinf_t){ sizeof(cnmanyref_t), sizeof(void *) };
    case TYPE_ARR: {
        typeinf_t inf = type_getinf(cnm, type + 1);
        inf.size *= type->n;
        return inf;
    }
    case TYPE_USER:
        for (userty_t *u = cnm->type.types; u; u = u->next) {
            if (u->typeid != type->n) continue;
            return u->inf;
        }
        return (typeinf_t){0};
    default:
        return (typeinf_t){ .size = 0, .align = 1 };
    }
}

// Helper function
static inline int type_default_bitwidth(cnm_t *cnm, const type_t type) {
    return type_getinf(cnm, &type).size * 8;
}

// Checks whether 2 type refrences are equal or not
// if test_toplvl is set to true, then it will check top level qualifiers
static bool type_eq(const typeref_t a, const typeref_t b, bool test_toplvl) {
    // If sizes aren't equal, can skip rest
    if (a.size != b.size) return false;

    for (int i = 0; i < a.size; i++) {
        // Check class
        if (a.type[i].class != b.type[i].class) return false;

        // Check constness (if not on top level)
        if ((i || test_toplvl) && a.type[i].isconst != b.type[i].isconst) return false;

        // Check for storage qualifiers
        if (a.type[i].isstatic != b.type[i].isstatic
            || a.type[i].isextern != b.type[i].isextern) return false;

        // Check for number
        if (a.type[i].n != b.type[i].n) return false;

        // Equate function arguments as their own type
        if (a.type[i].class == TYPE_FN_ARG) {
            // Equate function arguments
            size_t size = a.type[i].n;
            if (!type_eq((typeref_t){
                    .type = a.type + i + 1,
                    .size = size,
                }, (typeref_t){
                    .type = b.type + i + 1,
                    .size = size,
                }, false)) return false;
            i += size;
        }
    }

    return true;
}

// Promotes a type to atleast type of int. Behavior is undefined if the type is
// not already an arithmetic type
static inline void type_promote_to_int(typeref_t *ref) {
    if (ref->type[0].class < TYPE_INT) {
        ref->type[0].class = TYPE_INT;
        ref->type[0].n = 32;
    }
}

// Helper struct for type_parse_declspec
typedef struct declspec_options_s {
    bool is_u, is_short, is_fp; // flags for type
    bool set_type, set_u; // whether or not the type or signedness was already set
    int n_longs; // number of long keywords processed
    strview_t ident; // optional: used for typedefs
} declspec_options_t;

static bool type_parse_declspec_multiple_types_err(cnm_t *cnm) {
    cnm_doerr(cnm, true, "multiple data types in declaration specifiers");
    return false;
}
static bool type_parse_declspec_unsigned(cnm_t *cnm, type_t *type, bool *istypedef,
                                         declspec_options_t *options) {
    // Check if we've already used a signedness specifier
    if (options->set_u) {
        if (options->is_u) cnm_doerr(cnm, true, "duplicate unsigned specifiers");
        else cnm_doerr(cnm, true, "conflicting signed and unsigned specifiers");
        return false;
    }

    // Set qualifiers
    options->is_u = true;
    options->set_u = true;
    token_next(cnm);
    return true;
}
static bool type_parse_declspec_int(cnm_t *cnm, type_t *type, bool *istypedef,
                                    declspec_options_t *options) {
    if (options->set_type) return type_parse_declspec_multiple_types_err(cnm);
    options->set_type = true;
    type->class = TYPE_INT;
    token_next(cnm);
    return true;
}
static bool type_parse_declspec_double(cnm_t *cnm, type_t *type, bool *istypedef,
                                       declspec_options_t *options) {
    if (options->set_type) return type_parse_declspec_multiple_types_err(cnm);
    options->set_type = true;
    options->is_fp = true;
    type->class = TYPE_DOUBLE;
    token_next(cnm);
    return true;
}
static bool type_parse_declspec_float(cnm_t *cnm, type_t *type, bool *istypedef,
                                      declspec_options_t *options) {
    if (options->set_type) return type_parse_declspec_multiple_types_err(cnm);
    options->set_type = true;
    options->is_fp = true;
    type->class = TYPE_FLOAT;
    token_next(cnm);
    return true;
}
static bool type_parse_declspec_void(cnm_t *cnm, type_t *type, bool *istypedef,
                                     declspec_options_t *options) {
    if (options->set_type) return type_parse_declspec_multiple_types_err(cnm);
    options->set_type = true;
    options->is_fp = true;
    type->class = TYPE_VOID;
    token_next(cnm);
    return true;
}
static bool type_parse_declspec_bool(cnm_t *cnm, type_t *type, bool *istypedef,
                                    declspec_options_t *options) {
    if (options->set_type) return type_parse_declspec_multiple_types_err(cnm);
    options->set_type = true;
    options->is_fp = true;
    type->class = TYPE_BOOL;
    token_next(cnm);
    return true;
}
static bool type_parse_declspec_short(cnm_t *cnm, type_t *type, bool *istypedef,
                                      declspec_options_t *options) {
    if (options->is_short) {
        cnm_doerr(cnm, true, "duplicate 'short'");
        return false;
    }
    if (options->n_longs) {
        cnm_doerr(cnm, true, "conflicting 'short' and 'long' specifiers");
        return false;
    }
    options->is_short = true;
    token_next(cnm);
    return true;
}
static bool type_parse_declspec_static(cnm_t *cnm, type_t *type, bool *istypedef,
                                       declspec_options_t *options) {
    if (type->isstatic) {
        cnm_doerr(cnm, true, "duplicate 'static'");
        return false;
    }
    if (type->isextern) {
        cnm_doerr(cnm, true, "conflicting 'extern' and 'static' qualifiers");
        return false;
    }
    type->isstatic = true;
    token_next(cnm);
    return true;
}
static bool type_parse_declspec_signed(cnm_t *cnm, type_t *type, bool *istypedef,
                                       declspec_options_t *options) {
    // Check for multiple signedness qualifiers
    if (options->set_u) {
        if (!options->is_u) cnm_doerr(cnm, true, "duplicate signed specifiers");
        else cnm_doerr(cnm, true, "conflicting signed and unsigned specifiers");
        return false;
    }

    options->is_u = false;
    options->set_u = true;
    token_next(cnm);
    return true;
}
static bool type_parse_declspec_char(cnm_t *cnm, type_t *type, bool *istypedef,
                                     declspec_options_t *options) {
    if (options->set_type) return type_parse_declspec_multiple_types_err(cnm);
    options->set_type = true;
    type->class = TYPE_CHAR;
    token_next(cnm);
    return true;
}
static bool type_parse_declspec_const(cnm_t *cnm, type_t *type, bool *istypedef,
                                      declspec_options_t *options) {
    if (type->isconst) {
        cnm_doerr(cnm, true, "duplicate 'const'");
        return false;
    }
    type->isconst = true;
    token_next(cnm);
    return true;
}
static bool type_parse_declspec_long(cnm_t *cnm, type_t *type, bool *istypedef,
                                     declspec_options_t *options) {
    if (options->n_longs == 2) {
        cnm_doerr(cnm, true, "cnm script only supports up to long long ints");
        return false;
    }
    if (options->is_short) {
        cnm_doerr(cnm, true, "conflicting 'short' and 'long' specifiers");
        return false;
    }
    options->n_longs++;
    token_next(cnm);
    return true;
}
static bool type_parse_declspec_extern(cnm_t *cnm, type_t *type, bool *istypedef,
                                       declspec_options_t *options) {
    if (type->isextern) {
        cnm_doerr(cnm, true, "duplicate 'extern'");
        return false;
    }
    if (type->isstatic) {
        cnm_doerr(cnm, true, "conflicting 'extern' and 'static' qualifiers");
        return false;
    }
    type->isextern = true;
    token_next(cnm);
    return true;
}
static bool type_parse_declspec_typedef(cnm_t *cnm, type_t *type, bool *istypedef,
                                        declspec_options_t *options) {
    if (istypedef && *istypedef) {
        cnm_doerr(cnm, true, "duplicate 'typedef'");
        return false;
    }
    if (istypedef) *istypedef = true;
    token_next(cnm);
    return true;
}
// Will return false if an error occurred
// Will set outu to non-NULL if definition must be parsed, will put current token
// either at '{' or optionally ':' if docolon is true
static bool type_parse_declspec_user(cnm_t *cnm, type_t *type, bool docolon, userty_t **outu,
                                     size_t tysize, declspec_options_t *options) {
    *outu = NULL;

    // Check for previous type specs being used
    if (options->set_type) return type_parse_declspec_multiple_types_err(cnm);
    options->set_type = true;

    // Make sure that the user type name or body is present
    if (token_next(cnm)->type != TOKEN_IDENT
        && cnm->s.tok.type != TOKEN_BRACE_L
        && (!docolon || cnm->s.tok.type != TOKEN_COLON)) {
        cnm_doerr(cnm, true, "expected user type identifier");
        return false;
    }

    // Set type class
    type->class = TYPE_USER;
   
    // Look for struct with name that matches current token
    userty_t *u;
    for (u = cnm->type.types; u; u = u->next) {
        // Return already existing struct if we can
        if (strview_eq(u->name, cnm->s.tok.src)) {
            type->n = u->typeid;
            if (token_next(cnm)->type == TOKEN_BRACE_L
                || (docolon && cnm->s.tok.type == TOKEN_COLON)) {
                goto parse_def;
            }
            return true;
        }
    }

    // Create a new userty if we've checked all structs already
    if (!(u = cnm_alloc_static(cnm, sizeof(userty_t) + tysize, sizeof(void *)))) {
        return false;
    }
    *u = (userty_t){ .typeid = cnm->type.gid++ };
    u->next = cnm->type.types;
    cnm->type.types = u;
    memset(u->data, 0, tysize);
    type->n = u->typeid;
    if (cnm->s.tok.type == TOKEN_IDENT) {
        u->name = cnm->s.tok.src;
        token_next(cnm);
    }
    if (cnm->s.tok.type != TOKEN_BRACE_L
        && (!docolon || cnm->s.tok.type != TOKEN_COLON)) return true;

parse_def:
    // If s has already been defined, then throw an error
    if (u->inf.size) {
        cnm_doerr(cnm, true, "redefinition of type");
        return false;
    }

    *outu = u;
    return true;
}
static bool type_parse_declspec_struct(cnm_t *cnm, type_t *type, bool *istypedef,
                                       declspec_options_t *options) {
    userty_t *u;
    if (!type_parse_declspec_user(cnm, type, false, &u, sizeof(struct_t), options)) {
        return false;
    }
    if (!u) return true;
    struct_t *s = (struct_t *)u->data;
    token_next(cnm);
    // s now points to a new struct that we must fill out the members of

    // Info so that we can store bitfields correctly
    struct {
        typeinf_t inf; // size and alignment of current type
        int offs; // what bit field to allocate to next
        int byte_offs; // what byte offset this is at
    } bit = {0};

    // Now add members to the field
    while (cnm->s.tok.type != TOKEN_BRACE_R) {
        // Save this because if the userty pointer changes, that means that this
        // member is actually a new type definition, so if it has no name, don't
        // give an error out.
        const userty_t *const old_typtr = cnm->type.types;

        // Get type data (base type)
        type_t base;
        bool istypedef;
        if (!type_parse_declspec(cnm, &base, &istypedef)) return false;
        if (istypedef) {
            cnm_doerr(cnm, true, "can not have typedef in struct field");
            return false;
        }

        const bool not_defined_new_type = old_typtr == cnm->type.types;

        // Get derived type(s) and name(s)
        while (true) {
            // Use this so that the field can be destroyed if its a bitfield aligner
            uint8_t *const static_stack = cnm->alloc.curr_static;

            // Allocate new field member and add it to the list
            field_t *f = cnm_alloc_static(cnm, sizeof(field_t), sizeof(void *));
            if (!f) return false;
            f->next = s->fields;
            s->fields = f;

            // Save normal stack pointer for afterwards when we copy buffers over to
            // static region
            uint8_t *const stack_ptr = cnm->alloc.next;

            // Get the full type data
            f->type = type_parse(cnm, &base, &f->name, true);
            if (!f->type.size) {
                cnm_doerr(cnm, true, "field %s can not have 0 size, it's base type might"
                                     "also be undefined");
                return false;
            }
            if (!f->name.str && not_defined_new_type
                && !(type_is_int(*f->type.type) && f->type.type->n == 0)) {
                cnm_doerr(cnm, false, "declaration does not declare name");
            }

            // Move type data to static region
            cnm->alloc.next = stack_ptr; // free old data
            type_t *buf = cnm_alloc_static(cnm, sizeof(type_t) * f->type.size,
                                           sizeof(type_t));
            if (!buf) return false;
            memmove(buf, f->type.type, sizeof(type_t) * f->type.size); // move to new spot
            f->type.type = buf;

            // Get offset and new struct size and alignment
            typeinf_t inf = type_getinf(cnm, f->type.type);

            const bool bit_overflow = type_is_int(*f->type.type)
                && bit.offs + f->type.type->n > bit.inf.size * 8;

            if (type_is_int(*f->type.type) && f->type.type->n == 0) {
                // Align here, but don't do any size and also delete this field
                u->inf.size = align_size(u->inf.size, inf.align);
                if (inf.align > u->inf.align) u->inf.align = inf.align;
                bit.inf = (typeinf_t){0};
                bit.offs = 0;
                bit.byte_offs = u->inf.size;

                // Free the field
                s->fields = f->next;
                cnm->alloc.curr_static = static_stack;
            } else if (inf.size != bit.inf.size || inf.align != bit.inf.align
                       || bit_overflow) {
                // Allocate new area if bitfield is for different size type or
                // the bitfeild would overflow int boundaries
                u->inf.size = align_size(u->inf.size, inf.align);
                f->offs = u->inf.size;
                bit.byte_offs = u->inf.size;
                u->inf.size += inf.size;
                if (inf.align > u->inf.align) u->inf.align = inf.align;
                f->bit_offs = 0;

                // Set new bitfield offset
                if (type_is_int(*f->type.type) && f->type.type->n < inf.size * 8) {
                    bit.inf = inf;
                    bit.offs = f->type.type->n;
                } else {
                    bit.inf = (typeinf_t){0};
                    bit.offs = 0;
                }
            } else {
                // Allocate it to the same integer but in different bit position
                f->bit_offs = bit.offs;
                f->offs = bit.byte_offs;
                bit.offs += f->type.type->n;

                if (bit.offs >= bit.inf.size * 8) {
                    bit.inf = (typeinf_t){0};
                    bit.offs = 0;
                }
            }

            // Consume ',' token
            if (cnm->s.tok.type != TOKEN_COMMA) break;
            token_next(cnm);
        }

        // Consume ';' token
        if (cnm->s.tok.type != TOKEN_SEMICOLON) {
            cnm_doerr(cnm, true, "expected ';' after struct member");
            return false;
        }
        token_next(cnm);
    }

    if (!s->fields) {
        cnm_doerr(cnm, true, "can not have structure with no fields");
        return false;
    }

    // Align the struct size to the alignment size
    u->inf.size = align_size(u->inf.size, u->inf.align);

    token_next(cnm);
    return true;
}
static bool type_parse_declspec_enum(cnm_t *cnm, type_t *type, bool *istypedef,
                                     declspec_options_t *options) {
    userty_t *u;
    if (!type_parse_declspec_user(cnm, type, true, &u, sizeof(enum_t), options)) {
        return false;
    }
    if (!u) return true;
    enum_t *e = (enum_t *)u->data;
    // e now points to a new enum that we must fill out the variants for

    if (cnm->s.tok.type == TOKEN_COLON) {
        // Use custom enum type
        token_next(cnm);

        // Save pointer so we can free this type since its stored directly in enum
        uint8_t *const stack_ptr = cnm->alloc.next;
        
        // Get type data (base type)
        type_t base;
        bool istypedef;
        if (!type_parse_declspec(cnm, &base, &istypedef)) return false;
        if (istypedef) {
            cnm_doerr(cnm, true, "can not have typedef in struct field");
            return false;
        }

        // Get fully derived type
        strview_t name;
        typeref_t type = type_parse(cnm, &base, &name, false);

        // Make sure they don't use names in override type or define multiple times
        if (name.str) {
            cnm_doerr(cnm, true, "can not have name for enum override type");
            return false;
        }
        if (type.size != 1 || !type_is_int(type.type[0])) {
            cnm_doerr(cnm, true, "enum override type must be integer like type");
            return false;
        }
        if (cnm->s.tok.type == TOKEN_COMMA) {
            cnm_doerr(cnm, true, "only allow 1 default override type for enums");
            return false;
        }

        // Set new type
        e->type = type.type[0];

        // Reset stack and consume '{'
        cnm->alloc.next = stack_ptr;
        if (cnm->s.tok.type != TOKEN_BRACE_L) {
            cnm_doerr(cnm, true, "expect enum definition after enum override type");
            return false;
        }
        token_next(cnm);
    } else {
        // Default enum type (int)
        e->type = (type_t){ .class = TYPE_INT };
        token_next(cnm);
    }

    u->inf = type_getinf(cnm, &e->type);

    // Align variant area
    if (!cnm_alloc_static(cnm, 0, sizeof(void *))) return false;
    e->variants = (variant_t *)cnm->alloc.curr_static;

    // Current variant id
    union {
        intmax_t i;
        uintmax_t u;
    } variant_id = { .i = 0 };

    // Process enum variants
    while (cnm->s.tok.type != TOKEN_BRACE_R) {
        // Allocate new variant
        if (!cnm_alloc_static(cnm, sizeof(variant_t), 1)) return false;
        variant_t *v = --e->variants;
        e->nvariants++;

        // Make sure variant name is here
        if (cnm->s.tok.type != TOKEN_IDENT) {
            cnm_doerr(cnm, true, "expected enum variant name");
            return false;
        }
        v->name = cnm->s.tok.src;

        // Consume non '=' token and advance
        if (token_next(cnm)->type != TOKEN_ASSIGN) {
            if (type_is_unsigned(e->type)) v->id.u = variant_id.u++;
            else v->id.i = variant_id.i++;
            if (cnm->s.tok.type == TOKEN_COMMA) token_next(cnm);
            continue;
        }

        // Set variant number to custom number and reset stack position
        token_next(cnm);
        uint8_t *const stack_ptr = cnm->alloc.next;

        // Get number
        valref_t val;
        if (!expr_parse(cnm, &val, false, false, PREC_COND)) return false;
        if (!val.isliteral || !type_is_int(*val.type.type)) {
            cnm_doerr(cnm, true, "expected constant int expression for variant id");
            return false;
        }

        // Set number
        if (type_is_unsigned(*val.type.type)) variant_id.u = val.literal.u;
        else variant_id.i = val.literal.i;
        if (type_is_unsigned(e->type)) v->id.u = variant_id.u++;
        else v->id.i = variant_id.i++;

        // Consume ',' token (optionally)
        if (cnm->s.tok.type == TOKEN_COMMA) token_next(cnm);
    }

    if (!e->nvariants) {
        cnm_doerr(cnm, true, "can not have enum with no fields");
        return false;
    }

    token_next(cnm);
    return true;
}
static bool type_parse_declspec_union(cnm_t *cnm, type_t *type, bool *istypedef,
                                      declspec_options_t *options) {
    userty_t *u;
    if (!type_parse_declspec_user(cnm, type, false, &u, 0, options)) {
        return false;
    }
    if (!u) return true;
    token_next(cnm);
    // u now points to a new union that we must fill out the members of

    bool any_fields = false;

    // Now calculate union size. Don't process fields since you can access
    // union fields anyways in cnm script.
    while (cnm->s.tok.type != TOKEN_BRACE_R) {
        // Save normal stack pointer for afterwards when we copy buffers over to
        // static region
        uint8_t *const stack_ptr = cnm->alloc.next;

        // Get type data (base type)
        type_t base;
        bool istypedef;
        if (!type_parse_declspec(cnm, &base, &istypedef)) return false;
        if (istypedef) {
            cnm_doerr(cnm, true, "can not have typedef in union field");
            return false;
        }

        while (true) {
            // Get the type data
            typeref_t ref = type_parse(cnm, &base, NULL, true);
            if (!typeref_isvalid(ref)) {
                cnm_doerr(cnm, true, "can not have 0 sized type in union field");
                return false;
            }

            // Get offset and new struct size and alignment
            typeinf_t inf = type_getinf(cnm, ref.type);
            if (inf.size > u->inf.size) u->inf.size = inf.size;
            if (inf.align > u->inf.align) u->inf.align = inf.align;

            // Consume ',' token
            if (cnm->s.tok.type != TOKEN_COMMA) break;
            token_next(cnm);
        }

        cnm->alloc.next = stack_ptr; // free old data

        // Consume ';' token
        if (cnm->s.tok.type != TOKEN_SEMICOLON) {
            cnm_doerr(cnm, true, "expected ';' after union field");
            return false;
        }
        token_next(cnm);

        any_fields = true;
    }

    if (!any_fields) {
        cnm_doerr(cnm, true, "can not have union with no fields");
        return false;
    }
    
    token_next(cnm);
    return true;
}
static bool type_parse_declspec_ident(cnm_t *cnm, type_t *type, bool *istypedef,
                                      declspec_options_t *options) {
    type->class = TYPE_TYPEDEF;

    // Look for typedef with name that matches current token
    for (typedef_t *t = cnm->type.typedefs; t; t = t->next) {
        // Return already existing union if we can
        if (!strview_eq(t->name, cnm->s.tok.src)) continue;
        type->n = t->typedef_id;
        token_next(cnm);
        return true;
    }

    cnm_doerr(cnm, true, "use of unknown identifier");
    return false;
}

typedef bool(*declspec_parser_pfn_t)(cnm_t *cnm, type_t *type, bool *istypedef,
                                     declspec_options_t *options);

// Returns a parser for the declaration specifer where the cnm token is pointing to
// It will return NULL if cnm token is not pointing at a typedef or declspec
static declspec_parser_pfn_t cnm_at_declspec(cnm_t *cnm) {
    // All keywords that are valid for a declaration specifier
    static const struct {
        declspec_parser_pfn_t pfn;
        strview_t word;
    } keywords[] = {
        { .pfn = type_parse_declspec_int, .word = SV("int") },
        { .pfn = type_parse_declspec_bool, .word = SV("bool") },
        { .pfn = type_parse_declspec_char, .word = SV("char") },
        { .pfn = type_parse_declspec_enum, .word = SV("enum") },
        { .pfn = type_parse_declspec_long, .word = SV("long") },
        { .pfn = type_parse_declspec_void, .word = SV("void") },
        { .pfn = type_parse_declspec_const, .word = SV("const") },
        { .pfn = type_parse_declspec_float, .word = SV("float") },
        { .pfn = type_parse_declspec_short, .word = SV("short") },
        { .pfn = type_parse_declspec_union, .word = SV("union") },
        { .pfn = type_parse_declspec_double, .word = SV("double") },
        { .pfn = type_parse_declspec_extern, .word = SV("extern") },
        { .pfn = type_parse_declspec_signed, .word = SV("signed") },
        { .pfn = type_parse_declspec_static, .word = SV("static") },
        { .pfn = type_parse_declspec_struct, .word = SV("struct") },
        { .pfn = type_parse_declspec_typedef, .word = SV("typedef") },
        { .pfn = type_parse_declspec_unsigned, .word = SV("unsigned") },
    };

    // Look for keywords
    if (cnm->s.tok.type != TOKEN_IDENT) return NULL;
    for (int i = 0; i < arrlen(keywords); i++) {
        if (strview_eq(cnm->s.tok.src, keywords[i].word)) return keywords[i].pfn;
    }

    // Look for typedefs
    for (typedef_t *def = cnm->type.typedefs; def; def = def->next) {
        if (!strview_eq(def->name, cnm->s.tok.src)) continue;
        return type_parse_declspec_ident;
    }

    // Unknown identifier
    return NULL;
}

static typedef_t *type_get_typedef(cnm_t *cnm, const type_t *type) {
    typedef_t *t;
    for (t = cnm->type.typedefs; t->typedef_id != type->n; t = t->next);
    return t;
}

// Validate state and make integers unsigned
static bool declspec_apply_options(cnm_t *cnm, type_t *type, declspec_options_t *options) {
    if (type->class == TYPE_TYPEDEF) {
        type_t *newtype = type_get_typedef(cnm, type)->type.type;
        if (type_is_arith(*newtype)) *type = *newtype;
    }

    // short keyword can only be used on int type
    if (options->is_short) {
        if (type->class != TYPE_INT) {
            cnm_doerr(cnm, true, "used non int type with short specifier");
            return false;
        }
        type->class = TYPE_SHORT;
    }

    // long keyword can only be used on int type
    if (options->n_longs) {
        if (type->class != TYPE_INT) {
            cnm_doerr(cnm, true, "used non int type with long specifier");
            return false;
        }
        if (options->n_longs == 1) type->class = TYPE_LONG;
        else type->class = TYPE_LLONG;
    }

    // Can't use signedness qualifiers on floating point types or booleans
    if (options->set_u && !type_is_int(*type)) {
        cnm_doerr(cnm, true, "used non int type with signed specifiers");
        return false;
    }

    // Did we use unsigned on an already unsigned 
    if (options->set_u && type_is_unsigned(*type)) {
        cnm_doerr(cnm, true, "used unsigned on already unsigned type");
        return false;
    }

    // Make type unsigned if nessesary
    if (options->is_u) {
        switch (type->class) {
        case TYPE_CHAR: type->class = TYPE_UCHAR; break;
        case TYPE_SHORT: type->class = TYPE_USHORT; break;
        case TYPE_INT: type->class = TYPE_UINT; break;
        case TYPE_LONG: type->class = TYPE_ULONG; break;
        case TYPE_LLONG: type->class = TYPE_ULLONG; break;
        default:
            break;
        }
    }

    return true;
}

// Parses declaration specifiers for a type and store them in type
// It may also optionally parse a struct, enum, or union definition or declaration
// and allocate it in the static region.
static bool type_parse_declspec(cnm_t *cnm, type_t *type, bool *istypedef) {
    *type = (type_t){ .class = TYPE_INT };

    // Assume like any other compiler that a type is not a typedef
    if (istypedef) *istypedef = false;

    // Start with signed values like most C compilers
    // Since you can have multiple decl specifiers, these kind of 'accumulate'
    declspec_options_t options = {0};

    // Start consuming declaration specifiers
    bool no_keywords = true;
    declspec_parser_pfn_t parser;
    while ((parser = cnm_at_declspec(cnm))) {
        if (!parser(cnm, type, istypedef, &options)) return false;
        no_keywords = false;
    }

    if (no_keywords) {
        cnm_doerr(cnm, true, "use of undeclared identifier");
        return false;
    }

    if (!declspec_apply_options(cnm, type, &options)) return false;

    // Set bit width
    if (type_is_int(*type)) type->n = type_default_bitwidth(cnm, *type);

    return true;
}

// Parse only qualifiers and not type specifiers for type.
// This means only parsing const, static, or extern
static bool type_parse_qual_only(cnm_t *cnm, type_t *type, bool const_only) {
    type->isconst = false;
    type->isstatic = false;
    type->isextern = false;

    // Accumulate type qualifiers only
    while (cnm->s.tok.type == TOKEN_IDENT) {
        if (strview_eq(cnm->s.tok.src, SV("const"))) {
            if (type->isconst) {
                cnm_doerr(cnm, true, "duplicate 'const'");
                return false;
            }
            type->isconst = true;
        } else if (strview_eq(cnm->s.tok.src, SV("static"))) {
            if (type->isstatic) {
                cnm_doerr(cnm, true, "duplicate 'static'");
                return false;
            }
            if (type->isextern) {
                cnm_doerr(cnm, true, "conflicting 'extern' and 'static' qualifiers");
                return false;
            }
            type->isstatic = true;
        } else if (strview_eq(cnm->s.tok.src, SV("extern"))) {
            if (type->isextern) {
                cnm_doerr(cnm, true, "duplicate 'extern'");
                return false;
            }
            if (type->isstatic) {
                cnm_doerr(cnm, true, "conflicting 'extern' and 'static' qualifiers");
                return false;
            }
            type->isextern = true;
        } else {
            break;
        }

        token_next(cnm);
    }

    if (const_only && (type->isextern || type->isstatic)) {
        cnm_doerr(cnm, true, "did not expect storage class specifiers");
        return false;
    }

    return true;
}

// Helper function to add pointers in reverse order to a typeref thats on the
// top of the stack
static bool type_parse_add_ref_pointers(cnm_t *cnm, typeref_t *ref, type_t *ptrs, int nptrs) {
    for (; nptrs;) {
        if (!cnm_alloc(cnm, sizeof(type_t), 1)) return false;
        ref->type[ref->size++] = ptrs[--nptrs];
    }
    return true;
}

// Expand typedefs
static bool type_parse_append_base(cnm_t *cnm, typeref_t *type, const type_t *base) {
    if (base->class != TYPE_TYPEDEF) {
        if (!cnm_alloc(cnm, sizeof(type_t), 1)) return false;
        type->type[type->size++] = *base;
        return true;
    }

    typedef_t *t = type_get_typedef(cnm, base);
    if (!cnm_alloc(cnm, sizeof(type_t) * t->type.size, 1)) return false;
    memcpy(type->type + type->size, t->type.type, sizeof(type_t) * t->type.size);
    type->size += t->type.size;

    return true;
}

// Helper for type_parse function that will change depending on whether or not
// it is a function parameter
static typeref_t type_parse_ex(cnm_t *cnm, const type_t *base,
                               strview_t *name, bool isparam,
                               bool allow_bitfields) {
    // First align the allocation area
    typeref_t ref = { .type = cnm_alloc(cnm, 0, sizeof(type_t)) };
    if (!ref.type) goto return_error;

    // Set to name to NULL just in case there is no name here
    if (name) *name = (strview_t){0};

    // Save pointers processed and what 'group' we are on here so we can
    // parse with the correct associativity and precedence
    type_t ptrs[32];
    int nptrs[8];
    int grp = 0, base_ptr = 0;

    nptrs[grp] = 0;

    // Consume groupings and pointers
    while (true) {
        if (cnm->s.tok.type == TOKEN_STAR) {
            type_t *ptr = &ptrs[base_ptr + nptrs[grp]++];
            *ptr = (type_t){ .class = TYPE_PTR };
            token_next(cnm);
            if (!type_parse_qual_only(cnm, ptr, true)) goto return_error;
        } else if (cnm->s.tok.type == TOKEN_PAREN_L) {
            if (grp + 1 >= arrlen(nptrs)) {
                cnm_doerr(cnm, true, "too many grouping tokens in this type");
                goto return_error;
            }
            base_ptr += nptrs[grp];
            nptrs[++grp] = 0;
            token_next(cnm);
        } else {
            // Either variable name or start of array/function pointer section
            break;
        }
    }

    // Is there a name?
    if (cnm->s.tok.type == TOKEN_IDENT) {
        if (name) *name = cnm->s.tok.src;
        token_next(cnm);
    }

    // Start parsing arrays and functions
    while (true) {
        if (cnm->s.tok.type == TOKEN_BRACK_L) {
            // Create new type layer and set it to array
            if (!cnm_alloc(cnm, sizeof(type_t), 1)) goto return_error;
            type_t *type = ref.type + ref.size++;
            *type = (type_t){ .class = TYPE_ARR };
            if (isparam) type->class = TYPE_PTR;

            // Goto array size parameter/type qualifiers
            token_next(cnm);
            type_parse_qual_only(cnm, type, !isparam);
            if (cnm->s.tok.type == TOKEN_BRACK_R) {
                token_next(cnm);
                continue;
            }

            // Get size and store alloc pointer so we can free
            uint8_t *const stack_ptr = cnm->alloc.next;
            valref_t val;
            if (!expr_parse(cnm, &val, false, false, PREC_FULL)) goto return_error;
            if (!val.isliteral) {
                cnm_doerr(cnm, true, "array size must be constant integer expression");
                goto return_error;
            }
            if (!type_is_arith(*val.type.type) || type_is_fp(*val.type.type)) {
                cnm_doerr(cnm, true, "array size is non-integer type");
                goto return_error;
            }
            if (!type_is_unsigned(*val.type.type) && val.literal.i < 0) {
                cnm_doerr(cnm, true, "size of array is negative");
                goto return_error;
            }
            if (val.literal.u >= 1 << 24) {
                cnm_doerr(cnm, true, "length of array is over max array length");
                goto return_error;
            }

            // Set array size and free ast nodes
            type->n = val.literal.u;
            cnm->alloc.next = stack_ptr;

            // Check for ending ']'
            if (cnm->s.tok.type != TOKEN_BRACK_R) {
                cnm_doerr(cnm, true, "expected ']'");
                goto return_error;
            }
            token_next(cnm);
        } else if (cnm->s.tok.type == TOKEN_PAREN_L) {
            // Create new type layer and set it to function
            if (!cnm_alloc(cnm, sizeof(type_t), 1)) goto return_error;
            type_t *fntype = ref.type + ref.size++;
            *fntype = (type_t){ .class = TYPE_FN };

            // Consume parameters
            token_next(cnm);
            while (cnm->s.tok.type != TOKEN_PAREN_R) {
                fntype->n++; // Increase the number of parameters in this func

                // Allocate the argument header
                if (!cnm_alloc(cnm, sizeof(type_t), 1)) goto return_error;
                type_t *arg = ref.type + ref.size++;
                *arg = (type_t){ .class = TYPE_FN_ARG };
              
                // Get base type
                type_t base;
                bool is_arg_typedef;
                if (!type_parse_declspec(cnm, &base, &is_arg_typedef)) goto return_error;
                if (is_arg_typedef) {
                    cnm_doerr(cnm, true, "can not make function parameter a typedef");
                    goto return_error;
                }

                // Get the full derived parameter type
                typeref_t argref = type_parse_ex(cnm, &base, NULL, true, false);
                if (!argref.type) goto return_error;
                if (!argref.size) {
                    cnm_doerr(cnm, true, "expected function parameter");
                    goto return_error;
                }
                
                // Set the argument's header's length
                arg->n = argref.size;
                ref.size += argref.size;

                if (cnm->s.tok.type == TOKEN_COMMA) {
                    if (token_next(cnm)->type == TOKEN_PAREN_R) {
                        cnm_doerr(cnm, true,
                            "expected another function parameter after ,");
                        goto return_error;
                    }
                }
            }
            token_next(cnm);
        } else if (cnm->s.tok.type == TOKEN_PAREN_R) {
            // Groupings
            if (!grp) break; // If there wasn't a grouping then we're at the end
           
            // Copy the pointers into the type buffer in reverse order
            type_parse_add_ref_pointers(cnm, &ref, ptrs + base_ptr, nptrs[grp]);

            // Goto the next group
            base_ptr -= nptrs[--grp];
            token_next(cnm);
        } else {
            break;
        }
    }

    if (grp != 0) {
        cnm_doerr(cnm, true, "expected ')'");
        goto return_error;
    }

    // Copy the pointers into the type buffer in reverse order
    type_parse_add_ref_pointers(cnm, &ref, ptrs, nptrs[0]);

    type_t real_base = *base;

    // Do bitfields
    if (cnm->s.tok.type == TOKEN_COLON && allow_bitfields) {
        if (!type_is_int(*base)) {
            cnm_doerr(cnm, true, "bitfields must be of integer type");
            goto return_error;
        }

        token_next(cnm);

        valref_t val;
        if (!expr_parse(cnm, &val, false, false, PREC_COND)) goto return_error;
        if (!val.isliteral) {
            cnm_doerr(cnm, true, "expect constant value for bitfield width");
            goto return_error;
        }
        if (!type_is_int(*val.type.type)) {
            cnm_doerr(cnm, true, "expected bitfield width to be integer");
            goto return_error;
        }
        if (!type_is_unsigned(*val.type.type) && val.literal.i < 0) {
            cnm_doerr(cnm, true, "bitfield width must be positive");
            goto return_error;
        }
        if (val.literal.i == 0 && name && name->str) {
            cnm_doerr(cnm, true, "bitfield width with 0 width");
            goto return_error;
        }
        if (val.literal.i > (base->class == TYPE_BOOL ? 1 : base->n)) {
            cnm_doerr(cnm, true, "bitfield width exceeds its type");
            goto return_error;
        }
        real_base.n = val.literal.u;
    }

    // Add the base type on
    if (!type_parse_append_base(cnm, &ref, &real_base)) goto return_error;

    return ref;
return_error:
    return (typeref_t){0};
}

// Parses the derived types of a type expression and the name, so
// int *a[5];
// ^   ^----  this part of the equasion.
// +----  this is base
// the parameter base is a pointer to 1 type_t struct that represents the
// base type
static typeref_t type_parse(cnm_t *cnm, const type_t *base,
                            strview_t *name, bool allow_bitfields) {
    return type_parse_ex(cnm, base, name, false, allow_bitfields);
}

// Generates code and data for the expression being parsed
static bool expr_parse(cnm_t *cnm, valref_t *out, bool gencode, bool gendata, prec_t prec) {
    // Consume a prefix/unary ast node like an integer literal
    expr_rule_t *rule = &expr_rules[cnm->s.tok.type];
    if (!rule->prefix || prec > rule->prefix_prec) {
        cnm_doerr(cnm, true, "expected expression");
        return false;
    }
    if (!rule->prefix(cnm, out, gencode, gendata)) return false;

    // Continue to consume binary operators that fall under this precedence level
    rule = &expr_rules[cnm->s.tok.type];
    while (rule->infix && prec <= rule->infix_prec) {
        valref_t tmp;
        if (!rule->infix(cnm, &tmp, gencode, gendata, out)) return false;
        *out = tmp;
        rule = &expr_rules[cnm->s.tok.type];
    }

    return true;
}

// Constant fold cast an valref to the to type
static bool valref_cast_literal(cnm_t *cnm, valref_t *val, typeref_t cast_to) {
    type_t *from = val->type.type, *to = cast_to.type;

    if (!type_is_pod(*from) || !type_is_pod(*to)) {
        cnm_doerr(cnm, true, "can only do casting between pod data types");
        return false;
    }

    // Cache information about from and to types
    const bool fp_to = type_is_fp(*cast_to.type), fp_from = type_is_fp(*val->type.type),
        u_from = type_is_unsigned(*val->type.type) || val->type.type->class == TYPE_PTR;

    val->type = cast_to;

    // Convert to or convert from and to float types
    if (to->class == TYPE_BOOL) {
        if (fp_from) val->literal.u = !!val->literal.f;
        else val->literal.u = !!val->literal.u;
    } else if (fp_to && !fp_from) {
        val->literal.f = u_from ? val->literal.u : val->literal.i;
    } else if (!fp_to && fp_from) {
        if (u_from) val->literal.u = val->literal.f;
        else val->literal.i = val->literal.f;
    }

enforce_width:
    if (to->class == TYPE_FLOAT) {
        val->literal.f = (float)val->literal.f;
        return true;
    } else if (to->class == TYPE_DOUBLE || to->class == TYPE_PTR
        || to->class == TYPE_LONG || to->class == TYPE_LLONG
        || to->class == TYPE_ULONG || to->class == TYPE_ULLONG) {
        return true;
    } else if (to->class == TYPE_BOOL) {
        val->literal.u = !!val->literal.u;
        return true;
    }

    // Cap the integer to the bit width of what it is
    switch (to->class) {
    case TYPE_CHAR: val->literal.i = (intmax_t)(char)val->literal.i; break;
    case TYPE_SHORT: val->literal.i = (intmax_t)(short)val->literal.i; break;
    case TYPE_INT: val->literal.i = (intmax_t)(int)val->literal.i; break;
    case TYPE_UCHAR: val->literal.u &= 0xFF; break;
    case TYPE_USHORT: val->literal.u &= 0xFFFF; break;
    case TYPE_UINT: val->literal.u &= 0xFFFFFFFF; break;
    default: break;
    }

    return true;
}

// Generate code to cast the type and value of val to the type of to
static bool valref_cast_runtime(cnm_t *cnm, valref_t *val, const typeref_t to) {
    // TODO: Emit byte code to cast value
    return false;
}

// Cast val to 'to'
static bool valref_cast(cnm_t *cnm, valref_t *val, const typeref_t to, bool gencode) {
    if (val->isliteral) {
        if (!valref_cast_literal(cnm, val, to)) return false;
    } else if (gencode) {
        if (!valref_cast_runtime(cnm, val, to)) return false;
    }
    return true;
}

// Cast something, with token pointing to right after '(' in cast expr
static bool expr_cast(cnm_t *cnm, valref_t *out, bool gencode, bool gendata) {
    // Get the type to cast to first
    type_t base;
    bool istypedef;
    if (!type_parse_declspec(cnm, &base, &istypedef)) return false;
    if (istypedef) {
        cnm_doerr(cnm, true, "Can not declare typedef in cast expression.");
        return false;
    }
    strview_t name;
    typeref_t type = type_parse(cnm, &base, &name, false);
    if (!type.type) return false;
    if (name.str) {
        cnm_doerr(cnm, true, "Can not give type a identifier in cast expression");
        return false;
    }

    // Make sure there is the ending parenthesis
    if (cnm->s.tok.type != TOKEN_PAREN_R) {
        return false;
    }
    token_next(cnm);

    // Now parse the expression
    if (!expr_parse(cnm, out, gencode, gendata, PREC_PREFIX)) return false;

    // Now cast out to the type
    if (!valref_cast(cnm, out, type, gencode)) return false;

    return true;
}

// Group together operations into one group inbetween parentheses
static bool expr_group(cnm_t *cnm, valref_t *out, bool gencode, bool gendata) {
    // Consume the '(' token
    const token_t errtok = cnm->s.tok;
    token_next(cnm);
   
    // This might also be a cast operator so watch out
    if (cnm_at_declspec(cnm)) return expr_cast(cnm, out, gencode, gendata);

    // Get the inner grouping
    if (!expr_parse(cnm, out, gencode, gendata, PREC_FULL)) return false;

    // Consume the ')' token
    if (cnm->s.tok.type != TOKEN_PAREN_R) {
        cnm_doerr(cnm, true, "expected ')' after expression group");
        cnm->s.tok = errtok;
        cnm_doerr(cnm, false, "grouping started here");
        return NULL;
    }
    token_next(cnm);

    return true;
}

// Create a literal value valref for a string token
static bool expr_str(cnm_t *cnm, valref_t *out, bool gencode, bool gendata) {
    *out = (valref_t){ .isliteral = true, .type.size = 2 };

    // Allocate and initialize type
    if (!(out->type.type = cnm_alloc(cnm, sizeof(type_t) * 2, sizeof(type_t)))) return false;
    if (cnm->s.tok.suffix[0] == 'u') {
        out->type.type[0] = (type_t){ .class = TYPE_PTR };
        out->type.type[1] = (type_t){ .class = TYPE_UCHAR, .n = 8, .isconst = true };
    } else if (cnm->s.tok.suffix[0] == 'U') {
        out->type.type[0] = (type_t){ .class = TYPE_PTR };
        out->type.type[1] = (type_t){ .class = TYPE_UINT, .n = 32, .isconst = true };
    } else {
        out->type.type[0] = (type_t){ .class = TYPE_PTR };
        out->type.type[1] = (type_t){ .class = TYPE_CHAR, .n = 8, .isconst = true };
    }

    // Set the pointer
    out->literal.addr = cnm->s.tok.s;

    token_next(cnm);

    return true;
}

// Create a literal value valref for a character token
static bool expr_char(cnm_t *cnm, valref_t *out, bool gencode, bool gendata) {
    *out = (valref_t){ .isliteral = true, .type.size = 1 };

    // Allocate and initialize type
    if (!(out->type.type = cnm_alloc(cnm, sizeof(type_t), sizeof(type_t)))) return false;
    if (cnm->s.tok.suffix[0] == 'u') {
        *out->type.type = (type_t){ .class = TYPE_UCHAR, .n = 8 };
    } else if (cnm->s.tok.suffix[0] == 'U') {
        *out->type.type = (type_t){ .class = TYPE_UINT, .n = 32 };
    } else {
        *out->type.type = (type_t){ .class = TYPE_INT, .n = 32 };
    }

    // Set char
    out->literal.c = cnm->s.tok.c;

    // Goto next token for rest of expression
    token_next(cnm);

    return true;
}

// Create a literal value valref for the integer
static bool expr_int(cnm_t *cnm, valref_t *out, bool gencode, bool gendata) {
    *out = (valref_t){ .isliteral = true };

    // Allocate type
    if (!(out->type.type = cnm_alloc(cnm, sizeof(type_t), sizeof(type_t)))) return false;
    out->type.size = 1;

    // Get minimum allowed type by suffix
    typeclass_t mintype = TYPE_INT;
    if (cnm->s.tok.suffix[0] == 'u') {
        mintype = TYPE_UINT;
        if (cnm->s.tok.suffix[1] == 'l') {
            mintype = TYPE_ULONG;
            if (cnm->s.tok.suffix[2] == 'l') {
                mintype = TYPE_ULLONG;
            }
        }
    } else if (cnm->s.tok.suffix[0] == 'l') {
        mintype = TYPE_LONG;
        if (cnm->s.tok.suffix[1] == 'l') {
            mintype = TYPE_LLONG;
        }
    }

    const bool allow_u = cnm->s.tok.suffix[0] == 'u' || cnm->s.tok.i.base != 10;

    // Find smallest type that can represent the number and is bigger than the
    // minimum allowed type
    switch (mintype) {
    case TYPE_INT:
        if (cnm->s.tok.i.n <= INT_MAX && cnm->s.tok.suffix[0] != 'u') {
            out->type.type[0] = (type_t){ .class = TYPE_INT, .n = 32 };
            out->literal.i = cnm->s.tok.i.n;
            token_next(cnm); // Continue to next part in expression
            return true;
        }
    case TYPE_UINT:
        if (cnm->s.tok.i.n <= UINT_MAX && allow_u) {
            out->type.type[0] = (type_t){ .class = TYPE_UINT, .n = 32 };
            out->literal.u = cnm->s.tok.i.n;
            token_next(cnm); // Continue to next part in expression
            return true;
        }
    case TYPE_LONG:
        if (cnm->s.tok.i.n <= LONG_MAX && cnm->s.tok.suffix[0] != 'u') {
            out->type.type[0] = (type_t){ .class = TYPE_LONG, .n = 64 };
            out->literal.i = cnm->s.tok.i.n;
            token_next(cnm); // Continue to next part in expression
            return true;
        }
    case TYPE_ULONG:
        if (cnm->s.tok.i.n <= ULONG_MAX && allow_u) {
            out->type.type[0] = (type_t){ .class = TYPE_ULONG, .n = 64 };
            out->literal.u = cnm->s.tok.i.n;
            token_next(cnm); // Continue to next part in expression
            return true;
        }
    case TYPE_LLONG:
        if (cnm->s.tok.i.n <= LLONG_MAX && cnm->s.tok.suffix[0] != 'u') {
            out->type.type[0] = (type_t){ .class = TYPE_LLONG, .n = 64 };
            out->literal.i = cnm->s.tok.i.n;
            token_next(cnm); // Continue to next part in expression
            return true;
        }
    case TYPE_ULLONG:
        if (allow_u) {
            out->type.type[0] = (type_t){ .class = TYPE_ULLONG, .n = 64 };
            out->literal.u = cnm->s.tok.i.n;
            token_next(cnm); // Continue to next part in expression
            return true;
        }
    default:
        break;
    }

    // Couldn't find type to fix the number, default to int
    out->type.type[0] = (type_t){ .class = TYPE_INT, .n = 32 };
    out->literal.u = cnm->s.tok.i.n;
    cnm_doerr(cnm, false, "integer constant can not fit, defaulting to int");

    // Goto next token for the rest of the expression
    token_next(cnm);

    return true;
}

// Generate valref with literal double
static bool expr_double(cnm_t *cnm, valref_t *out, bool gencode, bool gendata) {
    *out = (valref_t){
        .isliteral = true,
        .literal.f = cnm->s.tok.f,
    };

    // Allocate ast type
    if (!(out->type.type = cnm_alloc(cnm, sizeof(type_t), sizeof(type_t)))) return false;
    out->type.size = 1;
    out->type.type[0] = (type_t){
        .class = cnm->s.tok.suffix[0] == 'f' ? TYPE_FLOAT : TYPE_DOUBLE,
    };

    // Goto next token for the rest of the expression
    token_next(cnm);

    return true;
}

// Helper function to set the type of an arithmetic valref
static bool set_arith_type(cnm_t *cnm, valref_t *out, valref_t *left, valref_t *right) {
    type_t *ltype = left->type.type, *rtype = right->type.type;

    // Binary arithmetic conversion ranks. Ranks have been built into the
    // enum definition of types
    // see https://en.cppreference.com/w/c/language/conversion for more
    out->type.type[0].n = ltype->class > rtype->class ? ltype->n : rtype->n;
    out->type.type[0].class = ltype->class > rtype->class ? ltype->class : rtype->class;
    type_promote_to_int(&out->type);

    return true;
}

// Perform math operation constant folding on valref
#define CF_OP(opname, optoken) \
    static void cf_##opname(valref_t *out, valref_t *left, valref_t *right) { \
        const typeclass_t class = out->type.type[0].class; \
        if (class == TYPE_DOUBLE || class == TYPE_FLOAT) { \
            out->literal.f = left->literal.f optoken right->literal.f; \
        } else if (type_is_unsigned(*out->type.type)) { \
            out->literal.u = left->literal.u optoken right->literal.u; \
        } else { \
            out->literal.i = left->literal.i optoken right->literal.i; \
        } \
    }
CF_OP(add, +)
CF_OP(sub, -)
CF_OP(mul, *)
CF_OP(div, /)

// Only perform math operation constant folding on integer ast nodes
#define CF_OP_INT(opname, optoken) \
    static void cf_##opname(valref_t *out, valref_t *left, valref_t *right) { \
        const typeclass_t class = out->type.type[0].class; \
        if (type_is_unsigned(*out->type.type)) { \
            out->literal.u = left->literal.u optoken right->literal.u; \
        } else { \
            out->literal.i = left->literal.i optoken right->literal.i; \
        } \
    }
CF_OP_INT(mod, %)
CF_OP_INT(bit_or, |)
CF_OP_INT(bit_and, &)
CF_OP_INT(bit_xor, ^)
CF_OP_INT(shift_l, <<)
CF_OP_INT(shift_r, >>)

// Perform math operation constant folding on unary valref
static void cf_neg(valref_t *var) {
    const typeclass_t class = var->type.type[0].class;
    if (class == TYPE_DOUBLE || class == TYPE_FLOAT) {
        var->literal.f = -var->literal.f;
    } else if (type_is_unsigned(*var->type.type)) {
        var->literal.u = -var->literal.u;
    } else {
        var->literal.i = -var->literal.i;
    }
}
static void cf_not(valref_t *var) {
    type_t *type = &var->type.type[0];
    if (type->class == TYPE_DOUBLE || type->class == TYPE_FLOAT) {
        var->literal.u = !var->literal.f;
    } else if (type_is_unsigned(*var->type.type)) {
        var->literal.u = !var->literal.u;
    } else {
        var->literal.u = !var->literal.i;
    }
    type->class = TYPE_BOOL;
}
static void cf_bit_not(valref_t *var) {
    const typeclass_t class = var->type.type[0].class;
    if (type_is_unsigned(*var->type.type)) {
        var->literal.u = ~var->literal.u;
    } else {
        var->literal.u = ~var->literal.i;
    }
}

// Generate valref that runs arithmetic operation on left and right hand side
// and perform constant folding if nessesary
static bool expr_arith(cnm_t *cnm, valref_t *out, bool gencode, bool gendata, valref_t *left) {
    *out = (valref_t){0};

    // Allocate ast type
    if (!(out->type.type = cnm_alloc(cnm, sizeof(type_t), sizeof(type_t)))) return false;
    out->type.size = 1;
    out->type.type[0] = (type_t){0};

    // Get current precedence level
    const prec_t prec = expr_rules[cnm->s.tok.type].infix_prec;

    // Set to if the operation can only be for integers (only needed for
    // constant folding)
    bool int_only_op = false;

    // Save the operation type
    const token_type_t optype = cnm->s.tok.type;
    switch (cnm->s.tok.type) {
    case TOKEN_MODULO: case TOKEN_BIT_OR: case TOKEN_BIT_AND:
    case TOKEN_BIT_XOR: case TOKEN_SHIFT_L: case TOKEN_SHIFT_R:
        int_only_op = true;
        break;
    default: break;
    }

    // Skip past arithmetic token to be at start of right hand of equasion
    const token_t backup = cnm->s.tok; // Used for if an error happens
    token_next(cnm);

    // Evaluate right hand side with left to right associativity
    valref_t right;
    if (!expr_parse(cnm, &right, gencode, gendata, prec + 1)) return false;

    // Make sure operands can even perform the operation we want
    if (!type_is_arith(*left->type.type) || !type_is_arith(*right.type.type)) {
        cnm->s.tok = backup;
        cnm_doerr(cnm, true, "expect arithmetic types for both operators of operand");
        return false;
    }

    // Make sure that if we are doing something like a bit operation that
    // we don't use it on floating point types
    if (int_only_op && (type_is_fp(*left->type.type) || type_is_fp(*right.type.type))) {
        cnm->s.tok = backup;
        cnm_doerr(cnm, true, "expected integer operands for integer/bitwise operation");
        return false;
    }

    // Get the new type and convert both sides at compile time if we can
    if (!set_arith_type(cnm, out, left, &right)) return false;
    if (left->isliteral) valref_cast_literal(cnm, left, out->type);
    if (right.isliteral) valref_cast_literal(cnm, &right, out->type);

    // Start constant propogation if we can
    if (!left->isliteral || !right.isliteral) return true;

    // Do the operation in question
    switch (optype) {
    case TOKEN_PLUS: cf_add(out, left, &right); break;
    case TOKEN_MINUS: cf_sub(out, left, &right); break;
    case TOKEN_STAR: cf_mul(out, left, &right); break;
    case TOKEN_DIVIDE: cf_div(out, left, &right); break;
    case TOKEN_MODULO: cf_mod(out, left, &right); break;
    case TOKEN_BIT_OR: cf_bit_or(out, left, &right); break;
    case TOKEN_BIT_XOR: cf_bit_xor(out, left, &right); break;
    case TOKEN_BIT_AND: cf_bit_and(out, left, &right); break;
    case TOKEN_SHIFT_L: cf_shift_l(out, left, &right); break;
    case TOKEN_SHIFT_R: cf_shift_r(out, left, &right); break;
    default:
        // Should never be reached (dead code)
        break;
    }
    
    // Enforce bit width and class
    valref_cast_literal(cnm, out, out->type);
    out->isliteral = true;
    return true;
}

// Generate ast node that performs a math operation on its child and does
// constant folding if nessescary
static bool expr_prefix_arith(cnm_t *cnm, valref_t *out, bool gencode, bool gendata) {
    *out = (valref_t){0};

    // Allocate out type
    if (!(out->type.type = cnm_alloc(cnm, sizeof(type_t), sizeof(type_t)))) return false;
    out->type.size = 1;
    out->type.type[0] = (type_t){0};

    // Save the operation type
    const token_type_t optype = cnm->s.tok.type;

    // Set to true if the operation can only be for integers (only needed for
    // constant folding)
    bool int_only_op = optype == TOKEN_BIT_NOT;

    // Skip pout arithmetic token to be at start of right hand of equasion
    const token_t backup = cnm->s.tok; // Used for if an error happens
    token_next(cnm);

    // Evaluate child with right to left hand associativity. Use PREC_PREFIX
    // for every option because every token used by this function has that prec
    if (!expr_parse(cnm, out, gencode, gendata, PREC_PREFIX)) return false;

    // Make sure we can even perform the operation we want with this type
    if (!type_is_arith(*out->type.type)) {
        cnm->s.tok = backup;
        cnm_doerr(cnm, true, "expect arithmetic type for operand of operator");
        return NULL;
    }

    // Make sure that if we are doing something like a bit operation that
    // we don't use it on floating point types
    if (int_only_op && type_is_fp(*out->type.type)) {
        cnm->s.tok = backup;
        cnm_doerr(cnm, true, "expected integer operand for integer/bitwise operation");
        return NULL;
    }

    // Get the new type and convert both sides at compile time if we can
    out->type.type[0].class = out->type.type[0].class;
    if (optype == TOKEN_NOT) out->type.type[0].class = TYPE_BOOL;
    else type_promote_to_int(&out->type);

    // Now perform constant folding if we can
    if (!out->isliteral) return out;

    // Do the operation in question
    switch (optype) {
    case TOKEN_NOT: cf_not(out); break;
    case TOKEN_MINUS: cf_neg(out); break;
    case TOKEN_BIT_NOT: cf_bit_not(out); break;
    default:
        // Should never be reached (dead code)
        break;
    }
    
    // Enforce bit width and class
    valref_cast_literal(cnm, out, out->type);
    return true;
}

// Initialize a cnm state object to compile code in the space provided by the code argument
cnm_t *cnm_init(void *region, size_t regionsz,
                void *code, size_t codesz,
                void *globals, size_t globalsz) {
    if (regionsz < sizeof(cnm_t)) return NULL;
    
    cnm_t *cnm = region;
    memset(cnm, 0, sizeof(*cnm));
    cnm->code.buf = code;
    cnm->code.len = codesz;
    cnm->code.ptr = cnm->code.buf;
    cnm->globals.buf = globals;
    cnm->globals.len = globalsz;
    cnm->globals.next = cnm->globals.buf;
    cnm->buflen = regionsz - sizeof(cnm_t);
    cnm->alloc.next = cnm->buf;
    cnm->alloc.curr_static = cnm->buf + cnm->buflen;
    cnm->strs = NULL;

    return cnm;
}

void cnm_set_errcb(cnm_t *cnm, cnm_err_cb_t errcb) {
    cnm->cb.err = errcb;
}

bool cnm_set_real_code_addr(cnm_t *cnm, void *addr) {
    cnm->code.real_addr = addr;
    return true;
}

size_t cnm_get_global_size(const cnm_t *cnm) {
    return cnm->globals.next - cnm->globals.buf;
}

static void cnm_set_src(cnm_t *cnm, const char *src, const char *fname) {
    cnm->s.src = src;
    cnm->s.fname = fname;
    cnm->s.tok = (token_t){
        .src = { .str = src, .len = 0 },
        .start = { .row = 1, .col = 1 },
        .end = { .row = 1, .col = 1 },
        .type = TOKEN_UNINITIALIZED,
    };
}

// Makes sure that a literal is allocated to the global data buffer
static bool val_enforce_global(cnm_t *cnm, valref_t *val) {
    if (!val->isliteral) return true;

    // Move the data to the global region
    const typeinf_t inf = type_getinf(cnm, val->type.type);
    uint8_t *buf = cnm_alloc_global(cnm, inf.size, inf.align);
    if (!buf) return false;
    memcpy(buf, &val->literal, inf.size);

    // Update the variable info
    val->scope->abs_addr = buf;
    val->isliteral = false;

    return true;
}

// Parse and generate code for a function
static bool parse_func(cnm_t *cnm, func_t *func) {
    return true;
}

// Parse variable declaration/definition
static bool parse_file_decl_var(cnm_t *cnm, strview_t name, typeref_t type) {
    scope_t *var = NULL;

    // Find existing variable with this name
    for (scope_t *iter = cnm->vars; iter && iter->scope == cnm->scope; iter = iter->next) {
        if (!strview_eq(iter->name, name)) continue;

        // Don't add duplicate variables
        if (type_eq(type, iter->type, true)) {
            var = iter;
            break;
        }

        // Types don't match, throw error instead
        cnm_doerr(cnm, true, "redeclaration of function with different types");
        return false;
    }

    // Generate a new scope entry for that variable
    if (!var) {
        if (!(var = cnm_alloc_static(cnm, sizeof(scope_t), sizeof(void *)))) return false;
        *var = (scope_t){
            .name = name,
            .type = type,
            .scope = cnm->scope,
            .uid = cnm->cg.uid_start++,
            .range = NULL,
            .abs_addr = NULL,
            .next = cnm->vars,
        };
        cnm->vars = var;
    }

    if (cnm->s.tok.type != TOKEN_ASSIGN) return true;
    token_next(cnm);

    // Actually allocate the variable
    if (var->abs_addr) {
        cnm_doerr(cnm, true, "redefinition of variable");
        return false;
    }

    // Get the contents of the variable
    valref_t val;
    if (!expr_parse(cnm, &val, false, true, PREC_COND)) return false;
   
    // Implicity cast arithmetic types (int -> long, double -> int, etc)
    if (type_is_arith(val.type.type[0]) && type_is_arith(type.type[0])
        && !valref_cast_literal(cnm, &val, type)) return false;

    // Make sure types match
    if (!type_eq(val.type, type, false)) {
        cnm_doerr(cnm, true, "initializing variable with expression of different type");
        return false;
    }

    val.scope = var;
    if (!val_enforce_global(cnm, &val)) return false;

    return true;
}

// Parse function definition
static bool parse_file_decl_func(cnm_t *cnm, strview_t name, typeref_t type) {
    func_t *func = NULL;

    // Find existing function with this name
    for (func_t *iter = cnm->funcs; iter; iter = iter->next) {
        if (!strview_eq(iter->name, name)) continue;

        // Don't add duplicate functions
        if (type_eq(type, iter->type, true)) {
            func = iter;
            break;
        }

        // Types don't match, throw error instead
        cnm_doerr(cnm, true, "redeclaration of function with different types");
        return false;
    }

    // Create a new function if there isn't already one with this name and type
    if (!func) {
        if (!(func = cnm_alloc_static(cnm, sizeof(func_t), sizeof(void *)))) return false;
        *func = (func_t){
            .next = cnm->funcs,
            .name = name,
            .type = type,
            .addr = NULL,
        };
        cnm->funcs = func;
    }

    if (cnm->s.tok.type != TOKEN_BRACE_L) return true;
    token_next(cnm);
    if (func->addr) {
        cnm_doerr(cnm, true, "redefinition of function!");
        return false;
    }

    // Do function definition (code)
    return parse_func(cnm, func);
}

// Parse typedef definition
static bool parse_file_decl_typedef(cnm_t *cnm, strview_t name, typeref_t type) {
    if (!name.str) {
        cnm_doerr(cnm, true, "can not have anonymous typedef");
        return false;
    }

    // Find existing typedef with this name
    for (typedef_t *iter = cnm->type.typedefs; iter; iter = iter->next) {
        if (!strview_eq(iter->name, name)) continue;

        // Don't add duplicate typedefs
        if (type_eq(type, iter->type, true)) return true;

        // Types don't match, throw error instead
        cnm_doerr(cnm, true, "redefinition of typedef");
        return false;
    }

    // Add new typedef definition
    typedef_t *td = cnm_alloc(cnm, sizeof(typedef_t), sizeof(void *));
    if (!td) return false;
    *td = (typedef_t){
        .type = type,
        .name = name,
        .typedef_id = cnm->type.typedef_gid++,
        .next = cnm->type.typedefs,
        .scope = cnm->scope,
    };
    cnm->type.typedefs = td;
    return true;
}

// Parse and handle a file scope declaration. When done, current cnm token will
// point to start of next file scope declaration, end of file, or something
// else if the file is not a proper c-script file.
static bool parse_file_decl(cnm_t *cnm) {
    // Hold old number of user types so that we don't post erronious warnings
    const userty_t *const userty_old = cnm->type.types;

    // Get the base type
    bool istypedef, was_fn = false;
    type_t base;
    if (!type_parse_declspec(cnm, &base, &istypedef)) return false;

    // Get full types by aquiring the derived type
    while (true) {
        // Parse the type
        strview_t name;
        typeref_t type = type_parse(cnm, &base, &name, false);
        if (!typeref_isvalid(type)) return false;

        // Now branch depending on what we parsed
        if (istypedef) {
            // Do typedef
            if (!parse_file_decl_typedef(cnm, name, type)) return false;
        } else if (type.type[0].class == TYPE_FN) {
            // Do function
            was_fn = true;
            if (!parse_file_decl_func(cnm, name, type)) return false;
        } else if (!name.str) {
            // Do nothing (empty declaration, that declares nothing)
            if (userty_old == cnm->type.types) {
                cnm_doerr(cnm, false, "declaration does not declare anything");
            }
        } else {
            // Allocate variable
            if (!parse_file_decl_var(cnm, name, type)) return false;
        }

        // Goto next one
        if (cnm->s.tok.type != TOKEN_COMMA) break;
        token_next(cnm);
    }

    // DON'T consume ';' because function definitions don't end in them
    if (!was_fn && cnm->s.tok.type != TOKEN_SEMICOLON) {
        cnm_doerr(cnm, true, "expected ';'");
        return false;
    }
    if (cnm->s.tok.type == TOKEN_SEMICOLON) token_next(cnm);

    return true;
}

bool cnm_parse(cnm_t *cnm, const char *src, const char *fname) {
    cnm_set_src(cnm, src, fname);
    token_next(cnm);
    while (cnm->s.tok.type != TOKEN_EOF) {
        if (!parse_file_decl(cnm)) return false;
    }
    return true;
}

