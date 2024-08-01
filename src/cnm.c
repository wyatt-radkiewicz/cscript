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
            char suffix[4];
            uintmax_t n;
        } i; // TOKEN_INT
        struct {
            char suffix[4];
            double n;
        } f; // TOKEN_DOUBLE
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

static inline bool strview_eq(const strview_t lhs, const strview_t rhs) {
    if (lhs.len != rhs.len) return false;
    return memcmp(lhs.str, rhs.str, lhs.len) == 0;
}

// Print out error to the error callback in the cnm state
static void cnm_doerr(cnm_t *cnm, bool critical, const char *desc, const char *caption) {
    static char buf[512];

    // Only increase error count if critical
    cnm->nerrs += critical;
    if (!cnm->cb.err) return;

    // Print header
    int len = snprintf(buf, sizeof(buf), "error: %s\n"
                                         "  --> %s:%d:%d\n"
                                         "   |\n"
                                         "%-3d|  ",
                                         desc, cnm->s.fname,
                                         cnm->s.tok.start.row, cnm->s.tok.start.col,
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
    if (len + cnm->s.tok.start.col + cnm->s.tok.src.len >= sizeof(buf)) goto overflow;
    memset(buf + len, ' ', cnm->s.tok.start.col); // spaces before caption
    len += cnm->s.tok.start.col;
    memset(buf + len, critical ? '~' : '-', cnm->s.tok.src.len); // underline
    len += cnm->s.tok.src.len;
    len += snprintf(buf + len, sizeof(buf) - len, " %s\n   |\n", caption); // caption

    cnm->cb.err(cnm->s.tok.start.row, buf, desc);
    return;
overflow:
    cnm->cb.err(cnm->s.tok.start.row, desc, desc);
    return;
}

// Allocate memory in the cnm state region
static void *cnm_alloc(cnm_t *cnm, size_t size) {
    if (cnm->next_alloc + size > cnm->buf + cnm->buflen) {
        cnm_doerr(cnm, true, "ran out of region memory", "at this point in parsing");
        return NULL;
    }
    void *const ptr = cnm->next_alloc;
    cnm->next_alloc += size;
    return ptr;
}

// Allocate memory at the end of the code region for a constant/global
static void *cnm_alloc_global(cnm_t *cnm, size_t size) {
    if (cnm->code.global_ptr - size < cnm->code.code_ptr) {
        cnm_doerr(cnm, true, "ran out of code space", "at this point in parsing");
        return NULL;
    }
    cnm->code.global_ptr -= size;
    return cnm->code.global_ptr;
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
            cnm_doerr(cnm, true, "\\x used with no following hex digits", "");
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
            cnm_doerr(cnm, true, "malformed character literal", "number format invalid");
            goto error;
        }
        result += pow * digit;
        pow *= base;
    }
    *str += nchrs;
    if (nsrc_cols) *nsrc_cols += nchrs;

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
    cnm_doerr(cnm, true, "unexpected eof when parsing character literal", "here");
error:
    cnm->s.tok.type = TOKEN_EOF;
    return 0;
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
            cnm_doerr(cnm, true, "unexpected eof while parsing string", "here");
            goto error_out;
        } else if (*src == '\n') {
            cnm_doerr(cnm, true, "unexpected new line while parsing string", "here");
            goto error_out;
        }

        size_t ncols, char_len = lex_char(cnm, &src, char_buf, &ncols);
        if (outncols) *outncols += ncols;
        if (!char_len) goto error_out;
        if (buf) memcpy(buf + len, char_buf, char_len);
        len += char_len;
    }
    src++;

    if (outlen) *outlen = len;
    if (outncols) ++*outncols;
    return src - src_start;

error_out:
    cnm->s.tok.type = TOKEN_EOF;
    return 0;
}

// Parses a string token and automatically concatinates them
// this implies that the resultant token might span over multiple lines
static token_t *token_string(cnm_t *cnm) {
    size_t buflen = 0;
    const uint8_t *const start = (const uint8_t *)cnm->s.tok.src.str;
    const uint8_t *curr = start;

    // To revert the end spot and length if there wasn't another string to concatinate
    int row_backup, col_backup;
    size_t len_backup;

    // Get the length of the string token and the length of the stored string
    cnm->s.tok.type = TOKEN_STRING;
    cnm->s.tok.src.len = 0;
    do {
        curr++;
        cnm->s.tok.end.col++;
        cnm->s.tok.src.len++;

        size_t _buflen, _ncols, _srclen = lex_single_string(cnm, curr, NULL,
                                                            &_buflen, &_ncols);
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
    ++buflen;

    // Allocate space for the string
    char *buf = cnm_alloc_global(cnm, buflen);
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
    buf[bufloc] = '\0';
    
    // Test to see if there is already a string with these contents
    for (const strent_t *ent = cnm->strs; ent; ent = ent->next) {
        if (strcmp(ent->str, buf)) continue;

        // We found exact copy of the string, so use that string instead
        cnm->code.global_ptr += buflen;
        cnm->s.tok.s = ent->str;
        return &cnm->s.tok;
    }

    // String has not been found so add it to the string entries list
    strent_t *ent = cnm_alloc(cnm, sizeof(*ent));
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
    uint8_t buf[4];
    const uint8_t *c = (const uint8_t *)cnm->s.tok.src.str + 1;
    size_t ncols;
    if (!lex_char(cnm, &c, buf, &ncols)) goto error;

    // Set new token info
    cnm->s.tok.type = TOKEN_CHAR;
    cnm->s.tok.src.len = (uintptr_t)c - (uintptr_t)cnm->s.tok.src.str;

    // Error out
    if (cnm->s.tok.src.str[cnm->s.tok.src.len] != '\'') {
        cnm_doerr(cnm, true, "expected ending \' after character", "here");
        cnm->s.tok.type = TOKEN_EOF;
        return &cnm->s.tok;
    }
    cnm->s.tok.src.len++;
    cnm->s.tok.end.col += ncols + 2;

    // Set the token's string value (which is a uint32_t)
    // Convert from UTF8 to codepoint
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
        cnm->s.tok.i.suffix[suffix_len] = '\0';
        cnm->s.tok.i.n = strtoumax(cnm->s.tok.src.str, &end, 0);

        // Look for optional 'u'
        if (tolower(*end) == 'u') {
            cnm->s.tok.i.suffix[suffix_len++] = 'u';
            cnm->s.tok.i.suffix[suffix_len] = '\0';
            end++;
        }
        
        // Look for optional 2 l's
        for (int i = 0; i < 2; i++) {
            if (tolower(*end) == 'l') {
                cnm->s.tok.i.suffix[suffix_len++] = 'l';
                cnm->s.tok.i.suffix[suffix_len] = '\0';
                end++;
            }
        }
    } else {
        cnm->s.tok.f.n = strtod(cnm->s.tok.src.str, &end);

        // Look for optional 'f'
        if (tolower(*end) == 'f') {
            cnm->s.tok.i.suffix[0] = 'f';
            cnm->s.tok.f.suffix[1] = '\0';
            end++;
        } else {
            cnm->s.tok.f.suffix[0] = '\0';
        }
    }

    // Check for other characters after the suffix/numbers and error if so
    if (end != cnm->s.tok.src.str + cnm->s.tok.src.len) {
        cnm_doerr(cnm, true, "invalid number literal format.",
                             "extra chars after digits");
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
        if (!isalpha(cnm->s.tok.src.str[0]) && cnm->s.tok.src.str[0] != '_') {
            cnm_doerr(cnm, true, "unknown character while lexing", "here");
            cnm->s.tok.type = TOKEN_EOF;
            return &cnm->s.tok;
        }
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

// Initialize a cnm state object to compile code in the space provided by the code argument
cnm_t *cnm_init(void *region, size_t regionsz, void *code, size_t codesz) {
    if (regionsz < sizeof(cnm_t)) return NULL;
    
    cnm_t *cnm = region;
    memset(cnm, 0, sizeof(*cnm));
    cnm->code.buf = code;
    cnm->code.len = codesz;
    cnm->code.global_ptr = cnm->code.buf + cnm->code.len;
    cnm->code.code_ptr = cnm->code.buf;
    cnm->buflen = regionsz - sizeof(cnm_t);
    cnm->next_alloc = cnm->buf;
    cnm->strs = NULL;

    return cnm;
}

void cnm_set_errcb(cnm_t *cnm, cnm_err_cb_t errcb) {
    cnm->cb.err = errcb;
}

bool cnm_parse(cnm_t *cnm, const char *src, const char *fname, const int *bpts, int nbpts) {
    cnm->s.src = src;
    cnm->s.fname = fname;
    cnm->s.tok = (token_t){
        .src = { .str = src, .len = 0 },
        .start = { .row = 1, .col = 1 },
        .end = { .row = 1, .col = 1 },
        .type = TOKEN_UNINITIALIZED,
    };

    return true;
}

