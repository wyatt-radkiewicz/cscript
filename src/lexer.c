#include <ctype.h>
#include <math.h>
#include <string.h>

#include "keywords.h"
#include "lexer.h"

static bool _parse_bin_u64(char *buf, uint64_t *x) {
    if (strlen(buf) > 64) return false;

    for (uint64_t b = (uint64_t)1 << (strlen(buf)-1), i = 0;
        i < strlen(buf);
        b >>= 1, i++) {
        if (buf[i] == '1') *x |= b;
    }

    return true;
}
static inline int _hextonum(char c) {
    switch (c) {
    case '0': return 0; case '1': return 1;
    case '2': return 2; case '3': return 3;
    case '4': return 4; case '5': return 5;
    case '6': return 6; case '7': return 7;
    case '8': return 8; case '9': return 9;
    case 'a': case 'A': return 10;
    case 'b': case 'B': return 11;
    case 'c': case 'C': return 12;
    case 'd': case 'D': return 13;
    case 'e': case 'E': return 14;
    case 'f': case 'F': return 15;
    default: return -1;
    }
}
static bool _parse_hex_u64(char *buf, uint64_t *x) {
    if (strlen(buf) > 16) return false;

    for (int s = (strlen(buf)-1)*4, i = 0;
        i < strlen(buf);
        s -= 4, i++) {
        *x |= (uint64_t)_hextonum(buf[i]) << s;
    }

    return true;
}
static bool sanitized_parse_u64(u8strview_t str, uint64_t *x) {
    char buf[64];
    {
        size_t n = u8_to_ascii(str, (strview_t){.str = buf, .len = arrsz(buf)});
        if (n == 0 || n >= arrsz(buf)) return false;
        for (int i = 0; i < strlen(buf); i++) {
            if (buf[i] != '_') continue;
            memmove(buf + i, buf + i + 1, strlen(buf) - i);
        }
    }
    *x = 0;
    if (buf[1] == 'x') return _parse_hex_u64(buf + 2, x);
    if (buf[1] == 'b') return _parse_bin_u64(buf + 2, x);

    uint64_t pow = 1;
    for (int32_t i = strlen(buf) - 1; i > -1; i--) {
        const uint64_t newx = *x + (uint64_t)(buf[i] - '0') * pow;
        if (newx < *x) return false;
        *x = newx;

        const uint64_t newpow = pow * 10;
        if (newpow < pow) return false;
        pow = newpow;
    }

    return true;
}
static bool sanitized_parse_f64(u8strview_t str, double *x) {
    char buf[64];
    {
        size_t n = u8_to_ascii(str, (strview_t){.str = buf, .len = arrsz(buf)});
        if (n == 0 || n >= arrsz(buf)) return false;
    }

    *x = 0.0;
    int32_t sep = 0;
    while (isdigit(buf[sep])) sep++;

    double p = 1.0f;
    for (int32_t i = sep - 1; i > -1; i--) {
        *x += p * (double)(buf[i] - '0');
        p *= 10.0f;
        if (*x == INFINITY) return false;
    }
    if (buf[sep++] == '.') {
        p = 0.1f;
        for (; isdigit(buf[sep]);) {
            *x += p * (double)(buf[sep++] - '0');
            p *= 0.1f;
            if (p == 0.0f) break;
        }
    }
    if (buf[sep++] == 'e') {
        p = 1.0f;
        bool neg = buf[sep] == '-';
        double exp = 0.0f;
        int32_t end = strlen(buf);
        for (int32_t i = end - 1; i > sep; i--) {
            exp += p * (double)(buf[i] - '0');
            p *= 10.0f;
            if (exp == INFINITY) return false;
        }
        exp *= neg ? -1.0f : 1.0f;
        *x *= pow(10.0, exp);
    }

    return true;
}
static bool _escape_c32(uint32_t c, uint32_t *dest) {
    switch (c) {
    case 'a': *dest = '\a'; return true;
    case 'b': *dest = '\b'; return true;
    case 'e': *dest = '\e'; return true;
    case 'f': *dest = '\f'; return true;
    case 'n': *dest = '\n'; return true;
    case 'r': *dest = '\r'; return true;
    case 't': *dest = '\t'; return true;
    case '\\': *dest = '\\'; return true;
    case '\'': *dest = '\''; return true;
    case '"': *dest = '"'; return true;
    case '?': *dest = '?'; return true;
    default: return false;
    }
}
static bool sanitized_parse_char(u8strview_t str, lex_token_char_t *x) {
    uint32_t i = 0, c, csize;

    x->bits = 8;
    x->c32 = 0;
    bool inside = false, unicode = false;
    char start[3];
    while (true) {
        if (str.size == 0) break;
        if (!u8next_char_len(&str.str, &c, &csize, str.size)) return false;
        str.size -= csize;

        if (inside && c == '\\') {
            if (str.size == 0) return false;
            if (!u8next_char_len(&str.str, &c, &csize, str.size)) return false;
            str.size -= csize;
            unicode = c == 'U' || c == 'x' || c == 'u';
        } else if (inside && c != '\\') {
            x->c32 = c;
            break;
        }
        if (inside && !unicode) {
            if (!_escape_c32(c, &x->c32)) return false;
            break;
        } else if (inside && unicode) {
            char buf[32];
            size_t n = u8_to_ascii(str, (strview_t){.str = buf, .len = arrsz(buf)});
            if (n == 0 || n >= arrsz(buf)) return false;
            uint64_t hex = 0;
            if (strlen(buf)) buf[strlen(buf)-1] = '\0';
            if (!_parse_hex_u64(buf, &hex)) return false;
            switch (x->bits) {
            case 16:
                if (hex >= 0x10000) return false;
                else break;
            case 32:
                if (hex >= 0x11000) return false;
                else break;
            default: if (hex >= 0x80) return false;
            }
            x->c32 = hex;
            break;
        }
        
        if (inside && c == '\'') break;
        if (!inside) {
            if (c >= 0x80) return false;
            if (c == '\'') {
                inside = true;
                i = 0;
                continue;
            }
            start[i] = c;
            if (i == 0 && start[0] != 'c') return false;
            if (i == 1 && start[1] == '8') x->bits = 8;
            if (i == 2 && start[2] == '6') x->bits = 16;
            if (i == 2 && start[2] == '2') x->bits = 32;
        }
        i++;
    }

    return true;
}

static lex_error_t lex_error_init(lex_state_t *state, bool okay) {
    return (lex_error_t){
        .okay = okay,
        .line = state->line,
        .chr = state->chr,
    };
}
lex_state_t lex_state_init(const uint8_t *src) {
    lex_state_t state = {
        .src = src,
        .line = 1,
        .chr = 1,
        .tok.type = tok_undefined,
        .next.type = tok_undefined,
    };
    if (!lex_state_next(&state).okay) {
        state = (lex_state_t){
            .tok.type = tok_undefined,
            .next.type = tok_undefined,
        };
    }
    return state;
}
static inline void settok_len2(lex_state_t *state, lex_token_type_t type) {
    state->src++;
    state->chr++;
    state->next.type = type;
    state->next.data.str.len = 2;
}
static inline bool skip_line(lex_state_t *state) {
    uint32_t c;
    
    do {
        if (!u8next_char(&state->src, &c, NULL)) return false;
    } while (c != '\n');

    state->chr = 1;
    state->line++;
    return true;
}
static inline bool skip_whitespace(lex_state_t *state) {
    while (true) {
        uint32_t c, c_next;
        const uint8_t *src = state->src;
        if (!u8next_char(&src, &c, NULL)) return false;
        if (!isspace(c) && c != '/') return true;
        const uint8_t *next_src = src;
        if (!u8next_char(&next_src, &c_next, NULL)) return false;
        if (c == '/' && c_next != '/') return true;
        else if (c == '/') skip_line(state);
        else {
            state->src = src;
            state->chr++;
            if (c == '\n') {
                state->chr = 1;
                state->line++;
            }
        }
    }

    return true;
}

static bool get_keyword(strview_t str, lex_token_type_t *type) {
    uint32_t hash = strview_hash(str);
    uint32_t psl = 0;
    uint32_t i = hash % KEYWORDMAP_SIZE;

    while (psl <= keywordmap[i].psl) {
        if (hash == keywordmap[i].hash) {
            *type = keywordmap[i].type;
            return true;
        }
        i = (i + 1) % KEYWORDMAP_SIZE;
        psl++;
    }

    return false;
}

static lex_error_t doident(lex_state_t *state) {
    state->src = state->next.data.u8str.str;
    state->line = state->next.line;
    state->chr = state->next.chr;
    state->next.data.str.len = 0;

    state->next.type = tok_ident;
    while (isalnum(*state->src) || *state->src == '_') {
        if ((*state->src & 0x80) != 0) return lex_error_init(state, false);
        
        state->src++;
        state->chr++;
        state->next.data.str.len++;
    }
    get_keyword(state->next.data.str, &state->next.type);
    return lex_error_init(state, true);
}
static lex_error_t dochar(lex_state_t *state) {
    state->src = state->next.data.u8str.str;
    state->line = state->next.line;
    state->chr = state->next.chr;
    state->next.data.str.len = 0;

    bool inside = false, escaped = false;
    uint32_t c = 0, len = 0;
    
    state->next.type = tok_literal_c;
    while (true) {
        const uint8_t *src = state->src;
        if (!u8next_char(&src, &c, NULL)) return lex_error_init(state, false);
        if (inside && c == '\'') {
            state->src = src;
            state->next.data.str.len++;
            if (len == 0) return lex_error_init(state, false);
            break;
        }
        if (inside) len++;
        if (c == '\'') inside = true;
        state->src = src;
        state->chr++;
        state->next.data.str.len++;

        if (len == 1 && c == '\\') escaped = true;
        if (!escaped && len > 1) return lex_error_init(state, false);
    }

    if (!sanitized_parse_char(state->next.data.u8str, &state->next.data.chr)) {
        return lex_error_init(state, false);
    }

    return lex_error_init(state, true);
}
static lex_error_t dostr(lex_state_t *state) {
    state->src = state->next.data.u8str.str;
    state->line = state->next.line;
    state->chr = state->next.chr;
    state->next.data.str.len = 0;

    bool inside = false;
    uint32_t c = 0, clast = 0;
    
    state->next.type = tok_literal_str;
    while (true) {
        clast = c;
        if (!u8next_char(&state->src, &c, NULL)) return lex_error_init(state, false);
        if (inside && clast != '\\' && c == '\"') {
            state->chr++;
            state->next.data.str.len++;
            break;
        }
        if (!inside && c == '\"') inside = true;
        state->chr++;
        state->next.data.str.len++;
    }

    return lex_error_init(state, true);
}
static lex_error_t donum(lex_state_t *state) {
    state->src = state->next.data.u8str.str;
    state->line = state->next.line;
    state->chr = state->next.chr;
    state->next.data.str.len = 0;

    bool hex = false, binary = false, sci = false;
    uint32_t sci_idx;

    state->next.type = tok_literal_u;
    while (true) {
        if ((*state->src & 0x80) != 0) return lex_error_init(state, false);
        if (!isdigit(*state->src) && *state->src != '.') {
            if (state->next.data.str.len == 1) {
                if (*state->src == 'x') hex = true;
                else if (*state->src == 'b') binary = true;
                else break;
            } else if (state->next.type == tok_literal_f && *state->src == 'e' && !sci) {
                sci = true;
                sci_idx = state->next.data.str.len;
            } else if (sci && *state->src == '-') {
                if (state->next.data.str.len != sci_idx+1) return lex_error_init(state, false);
            } else if ((hex || binary) && *state->src == '_' && state->src[1] != '_') {
            } else if (!hex || !isalpha(*state->src)) {
                break;
            }
        }
        if (*state->src == '.') {
            if (hex || binary) return lex_error_init(state, false);
            if (state->next.type == tok_literal_f) return lex_error_init(state, false);
            state->next.type = tok_literal_f;
        }

        state->src++;
        state->chr++;
        state->next.data.str.len++;

        if (binary && state->next.data.str.len > 2+64) return lex_error_init(state, false);
        if (hex && state->next.data.str.len > 2+16) return lex_error_init(state, false);
    }

    if (state->next.type == tok_literal_f
        && !sanitized_parse_f64(state->next.data.u8str, &state->next.data.f64)) {
        return lex_error_init(state, false);
    } else if (state->next.type == tok_literal_u
        && !sanitized_parse_u64(state->next.data.u8str, &state->next.data.u64)) {
        return lex_error_init(state, false);
    }

    return lex_error_init(state, true);
}

lex_error_t lex_state_next(lex_state_t *state) {
    if (!state->src) {
        state->tok = state->next;
        return lex_error_init(state, false);
    }
    if (state->next.type == tok_eof) {
        state->tok = state->next;
        return lex_error_init(state, true);
    }

    if (!skip_whitespace(state)) return lex_error_init(state, false);

    state->tok = state->next;
    state->next = (lex_token_t){
        .line = state->line,
        .chr = state->chr,
        .type = tok_eof,
        .data.u8str = (u8strview_t){
            .str = state->src,
            .size = 1,
        },
    };

    state->chr++;
    const uint8_t c = *state->src++;
    switch (c) {
    case '\0': state->next.type = tok_eof; break;
    case '+':
        if (*state->src == '+') settok_len2(state, tok_plusplus);
        else if (*state->src == '=') settok_len2(state, tok_pluseq);
        else state->next.type = tok_plus;
        break;
    case '-':
        if (*state->src == '-') settok_len2(state, tok_minusminus);
        else if (*state->src == '=') settok_len2(state, tok_minuseq);
        else if (*state->src == '>') settok_len2(state, tok_arrow);
        else state->next.type = tok_minus;
        break;
    case '(': state->next.type = tok_lparen; break;
    case ')': state->next.type = tok_rparen; break;
    case '[': state->next.type = tok_lbrack; break;
    case ']': state->next.type = tok_rbrack; break;
    case '.': state->next.type = tok_dot; break;
    case '{': state->next.type = tok_lbrace; break;
    case '}': state->next.type = tok_rbrace; break;
    case '!':
        if (*state->src == '=') settok_len2(state, tok_noteq);
        else state->next.type = tok_not;
        break;
    case '~': state->next.type = tok_bnot; break;
    case '*':
        if (*state->src == '=') settok_len2(state, tok_timeseq);
        else state->next.type = tok_star;
        break;
    case '/':
        if (*state->src == '=') settok_len2(state, tok_diveq);
        else state->next.type = tok_slash;
        break;
    case '%':
        if (*state->src == '=') settok_len2(state, tok_modeq);
        else state->next.type = tok_modulo;
        break;
    case '<':
        if (*state->src == '=') settok_len2(state, tok_lesseq);
        else if (*state->src == '<') {
            state->src++;
            state->chr++;
            state->next.type = tok_lshift;
            state->next.data.str.len = 2;
            if (*state->src == '=') {
                state->src++;
                state->chr++;
                state->next.type = tok_lshifteq;
                state->next.data.str.len = 3;
            }
        } else state->next.type = tok_less;
        break;
    case '>':
        if (*state->src == '=') settok_len2(state, tok_greatereq);
        else if (*state->src == '>') {
            state->src++;
            state->chr++;
            state->next.type = tok_rshift;
            state->next.data.str.len = 2;
            if (*state->src == '=') {
                state->src++;
                state->chr++;
                state->next.type = tok_rshifteq;
                state->next.data.str.len = 3;
            }
        } else state->next.type = tok_greater;
        break;
    case '=':
        if (*state->src == '=') settok_len2(state, tok_eqeq);
        else if (*state->src & 0x80) return lex_error_init(state, false);
        else state->next.type = tok_eq;
        break;
    case '&':
        if (*state->src == '&') settok_len2(state, tok_and);
        else if (*state->src == '=') settok_len2(state, tok_bandeq);
        else state->next.type = tok_band;
        break;
    case '^':
        if (*state->src == '=') settok_len2(state, tok_bxoreq);
        else if (*state->src & 0x80) return lex_error_init(state, false);
        else state->next.type = tok_bxor;
        break;
    case '|':
        if (*state->src == '|') settok_len2(state, tok_or);
        else if (*state->src == '=') settok_len2(state, tok_boreq);
        else state->next.type = tok_bor;
        break;
    case '?': state->next.type = tok_qmark; break;
    case ':': state->next.type = tok_colon; break;
    case ',': state->next.type = tok_comma; break;
    case ';': state->next.type = tok_semicol; break;
    case '\'': return dochar(state);
    case '\"': return dostr(state);
    case 'c':
        if (state->src[0] == '8' && (state->src[1] == '\'' || state->src[1] == '\"')) {
            if (state->src[1] == '\'') {
                return dochar(state);
            } else {
                return dostr(state);
            }
        } else if ((state->src[0] == '1' && state->src[1] == '6' && (state->src[2] == '\'' || state->src[2] == '\"'))
            || (state->src[0] == '3' && state->src[1] == '2' && (state->src[2] == '\'' || state->src[2] == '\"'))) {
            if (state->src[2] == '\'') {
                return dochar(state);
            } else {
                return dostr(state);
            }
        } else {
            return doident(state);
        }
        break;
    default:
        // UTF-8 is only allowed in comments and strings in cscript
        if (c & 0x80) return lex_error_init(state, true);
        if (isalpha(c) || c == '_') return doident(state);
        if (isdigit(c)) return donum(state);

        break;
    }

    return lex_error_init(state, true);
}

#define X(ENUM) case ENUM: return #ENUM;
static const char *tok_to_str(lex_token_type_t type) {
    switch (type) {
    LEX_TOKENS
    default: return "tok_***";
    }
}
#undef X

void lex_token_log(const lex_token_t *tok, FILE *out) {
    fprintf(out, "%s(line: %d, chr: %d", tok_to_str(tok->type), tok->line, tok->chr);
    switch (tok->type) {
    case tok_literal_f:
        fprintf(out, ", f: %f", tok->data.f64);
        break;
    case tok_literal_u:
        fprintf(out, ", u: %lu", tok->data.u64);
        break;
    case tok_literal_c:
        switch (tok->data.chr.bits) {
        case 8: fprintf(out, ", c: c8'"); break;
        case 16: fprintf(out, ", c: c16'"); break;
        case 32: fprintf(out, ", c: c32'"); break;
        default: fprintf(out, ", c: '"); break;
        }
        if (tok->data.chr.c32 < ' ') fprintf(out, "\\%d'", (char)tok->data.chr.c32);
        else if (tok->data.chr.c32 < 0x80) fprintf(out, "%c'", (char)tok->data.chr.c32);
        else fprintf(out, "\\%x'", tok->data.chr.c32);
        break;
    case tok_literal_str:
    case tok_ident: {
        uint8_t buf[5];
        uint32_t c, size;
        const uint8_t *str = tok->data.u8str.str;
        fprintf(out, ", lit: \"");
        while (str - tok->data.u8str.str < tok->data.u8str.size) {
            const uint8_t *const start = str;
            if (!u8next_char(&str, &c, &size)) return;
            memcpy(buf, start, size);
            buf[size] = '\0';
            fprintf(out, "%s", buf);
        }
        fprintf(out, "\"");
        } break;
    default: break;
    }
    fprintf(out, ")");
}

