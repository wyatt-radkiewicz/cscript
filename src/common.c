#include "common.h"

size_t u8_to_ascii(u8strview_t str, strview_t buf) {
    size_t nchars = 0;
    uint32_t c, csize;

    while (true) {
        if (str.size == 0) break;
        if (!u8next_char_len(&str.str, &c, &csize, str.size)) return 0;
        str.size -= csize;
        
        if (c >= 0x80) return 0;
        if (nchars < buf.len) {
            ((char *)buf.str)[nchars] = (char)c;
        }
        nchars++;
    }

    if (nchars < buf.len) {
        ((char *)buf.str)[nchars] = '\0';
    }
    nchars++;

    return nchars;
}

