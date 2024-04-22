#ifndef _common_h_
#define _common_h_

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define arrsz(ARR) (sizeof(ARR)/sizeof((ARR)[0]))

typedef struct strview {
    const char *str;
    size_t len;
} strview_t;

typedef struct u8strview {
    const uint8_t *str;
    size_t size;
} u8strview_t;

#define ERROR_CATEGORIES \
    X(error_category_lexer) \
    X(error_category_parser) \
    X(error_category_compiler)

#define X(ENUM) ENUM,
typedef enum error_category {
    ERROR_CATEGORIES
} error_category_t;
#undef X

typedef struct error {
    char msg[512];

    error_category_t category;
    int32_t line, chr;
} error_t;

#include <stdio.h>
void error_log(const error_t *err, FILE *out);

static inline uint32_t alignu(uint32_t x, uint32_t a) {
    assert(a);
    return x ? ((x - 1) / a + 1) * a : 0;
}

static inline uint32_t clo(uint32_t x) {
    return ~x ? __builtin_clz(~x) : 32;
}

static inline bool strview_eq(const strview_t a, const strview_t b) {
    return a.len == b.len && memcmp(a.str, b.str, a.len) == 0;
}

static inline bool u8next_char_len(const uint8_t **str, uint32_t *c32, uint32_t *size, size_t len) {
    uint32_t n = clo(**str);
    *c32 = 0;
    if (size) *size = 1;
    if (!len) {
        *size = 0;
        return false;
    }
    switch (n) {
    case 32:
    case 0:
        *c32 = *(*str)++;
        if (!*c32) --*str;
        return true;
    case 2:
        if (len < 2) {
            *size = len;
            return false;
        }
        if (size) *size = 2;

        *c32 |= *(*str)++ & 0x1f;
        if ((*(*str) & 0xc0) != 0x80 || *(*str) == 0) return false;
        *c32 |= *(*str)++ & 0x7f << 5;
        *c32 += 0x80;
        return true;
    case 3:
        if (len < 3) {
            *size = len;
            return false;
        }
        if (size) *size = 3;

        *c32 |= *(*str)++ & 0x1f;
        if ((*(*str) & 0xc0) != 0x80 || *(*str) == 0) return false;
        *c32 |= *(*str)++ & 0x7f << 5;
        if ((*(*str) & 0xc0) != 0x80 || *(*str) == 0) return false;
        *c32 |= *(*str)++ & 0x7f << (5+6);
        *c32 += 0x800;
        return true;
    case 4:
        if (len < 4) {
            *size = len;
            return false;
        }
        if (size) *size = 4;

        *c32 |= *(*str)++ & 0x1f;
        if ((*(*str) & 0xc0) != 0x80 || *(*str) == 0) return false;
        *c32 |= *(*str)++ & 0x7f << 5;
        if ((*(*str) & 0xc0) != 0x80 || *(*str) == 0) return false;
        *c32 |= *(*str)++ & 0x7f << (5+6);
        if ((*(*str) & 0xc0) != 0x80 || *(*str) == 0) return false;
        *c32 |= *(*str)++ & 0x7f << (5+12);
        *c32 += 0x10000;
        return true;
    default: return false;
    }
}
static inline bool u8next_char(const uint8_t **str, uint32_t *c32, uint32_t *size) {
    return u8next_char_len(str, c32, size, 4);
}

// Implementation of FNV-1a
static inline uint32_t strview_hash(strview_t str) {
    uint32_t hash = 2166136261;
    while (str.len >= 4) {
        hash ^= *(uint32_t *)str.str;
        hash *= 16777619;
        str.str += sizeof(uint32_t);
        str.len -= 4;
    }
    switch (str.len) {
    case 3:
        hash ^= (uint32_t)(str.str[0] << 0) | (uint32_t)(str.str[1] << 8) | (uint32_t)(str.str[2] << 16);
        break;
    case 2:
        hash ^= (uint32_t)(str.str[0] << 0) | (uint32_t)(str.str[1] << 8);
        break;
    case 1:
        hash ^= (uint32_t)(str.str[0] << 0);
        break;
    case 0: return hash;
    }

    hash *= 16777619;
    return hash;
}

// Returns how many characters it would right to the buffer if it were the
// size of an infintley sized buffer (including the null terminator)
//
// Returns 0 if it couldn't convert the utf-8 string to ascii
//
// Will set a null terminator in buf if it can
size_t u8_to_ascii(u8strview_t str, strview_t buf);

#endif

