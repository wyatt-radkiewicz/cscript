#include "common.h"

#define X(ENUM) case ENUM: return #ENUM;
static const char *error_category_as_str(error_category_t category) {
    switch (category) {
    ERROR_CATEGORIES
    default: return "error_category_undefined";
    }
}
#undef X

void error_log(const error_t *err, FILE *out) {
    fprintf(out, "ast_error_t{\n");
    fprintf(out, "    msg: \"%s\",\n", err->msg);
    fprintf(out, "    line: %d,\n", err->line);
    fprintf(out, "    chr: %d,\n", err->chr);
    fprintf(out, "    category: %s,\n", error_category_as_str(err->category));
    fprintf(out, "}\n");
}

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

