#ifndef _common_h_
#define _common_h_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define arrsz(ARR) (sizeof(ARR)/sizeof((ARR)[0]))

typedef struct strview {
    const char *str;
    size_t len;
} strview_t;

typedef struct u8strview {
    const uint8_t *str;
    size_t size;
} u8strview_t;

static inline uint32_t clo(uint32_t x) {
    return ~x ? __builtin_clz(~x) : 32;
}

static inline bool u8next_char(const uint8_t **str, uint32_t *c32, uint32_t *size) {
    uint32_t n = clo(**str);
    *c32 = 0;
    if (size) *size = 1;
    switch (n) {
    case 0:
        *c32 = *(*str)++;
        if (!*c32) --*str;
        return true;
    case 2:
        if (size) *size = 2;

        *c32 |= *(*str)++ & 0x1f;
        if ((*(*str) & 0xc0) != 0x80) return false;
        *c32 |= *(*str)++ & 0x7f << 5;
        *c32 += 0x80;
        return true;
    case 3:
        if (size) *size = 3;
        *c32 |= *(*str)++ & 0x1f;
        if ((*(*str) & 0xc0) != 0x80) return false;
        *c32 |= *(*str)++ & 0x7f << 5;
        if ((*(*str) & 0xc0) != 0x80) return false;
        *c32 |= *(*str)++ & 0x7f << (5+6);
        *c32 += 0x800;
        return true;
    case 4:
        if (size) *size = 4;
        *c32 |= *(*str)++ & 0x1f;
        if ((*(*str) & 0xc0) != 0x80) return false;
        *c32 |= *(*str)++ & 0x7f << 5;
        if ((*(*str) & 0xc0) != 0x80) return false;
        *c32 |= *(*str)++ & 0x7f << (5+6);
        if ((*(*str) & 0xc0) != 0x80) return false;
        *c32 |= *(*str)++ & 0x7f << (5+12);
        *c32 += 0x10000;
        return true;
    default: return false;
    }
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

#endif

