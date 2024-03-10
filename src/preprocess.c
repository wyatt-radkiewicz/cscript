#include <ctype.h>
#include <string.h>

#include "mem.h"
#include "preprocess.h"

#define MAX_MACROS 512
#define MAX_PARAMS 32

static char *_buf;

typedef struct param {
	const char *name;
	size_t len;
} param_t;

typedef struct macro {
	const char *start;
	size_t len;

	param_t params[MAX_PARAMS];
} macro_t;

typedef struct state {
	const char *src, *srcend;
	char *buf, *bufend;
	param_t *param_names;
	param_t param_bufs[MAX_PARAMS];
} state_t;

typedef struct processor {
	macro_t macros[MAX_MACROS];
	ident_map_t *macro_names;
} processor_t;

inline static void skip_line(state_t *const state) {
	while (*state->src != '\n') {
		if (state->src >= state->srcend) return;
		state->src++;
	}
	state->src++;
}
inline static bool skip_whitespace(state_t *const state) {
	bool did_newline = false;
	while (isspace(*state->src)) {
		if (state->src >= state->srcend) break;
		if (*(state->src++) == '\n') did_newline = true;
	}
	return did_newline;
}

static void process_define(processor_t *const processor, state_t *const state) {
	if (processor->macro_names->len == MAX_MACROS) {
		skip_line(state);
		return;
	}

	macro_t *macro = &processor->macros[processor->macro_names->len];
	skip_whitespace(state);

	const char *name_start = state->src;
	size_t name_len = 0;
	while (isalnum(*state->src) || *state->src == '_') {
		state->src++;
		name_len++;
	}

	if (*state->src != '(') goto skip_params;
	if (*(++state->src) == ')') {
		state->src++;
		goto skip_params;
	}
	param_t *param = macro->params;
	*param = (param_t){
		.name = state->src,
		.len = 0,
	};
	skip_whitespace(state);
	while (*state->src) {
		const char c = *(state->src)++;
		if (c == ',') {
			if (++param == macro->params + MAX_MACROS) break;
			skip_whitespace(state);
			*param = (param_t){
				.name = state->src,
				.len = 0,
			};
			continue;
		}
		if (c == ')') break;
		if (isalnum(c) || c == '_') param->len++;
	}
	state->src++;
skip_params:

	if (!skip_whitespace(state)) {
		macro->start = state->src;
		macro->len = 0;
		while (*(state->src++) != '\n') macro->len++;
	} else {
		macro->start = NULL;
		macro->len = 0;
	}

	ident_map_add(
		processor->macro_names,
		name_start,
		name_len,
		processor->macro_names->len
	);
}

static void process_directive(processor_t *const processor, state_t *const state) {
	const char *start = state->src;
	size_t len = 0;
	while (isalpha(*(state->src++))) len++;

	if (len == 6 && memcmp(start, "define", 6) == 0) process_define(processor, state);
	else skip_line(state);
}

static void process(processor_t *const processor, state_t *const state);

static void process_macro_call(
	processor_t *const processor,
	state_t *const state,
	macro_t *const macro,
	char *const macro_start
) {
	state_t inner_state = (state_t){
		.src = macro->start,
		.srcend = macro->start + macro->len,
		.buf = macro_start,
		.bufend = state->bufend,
		.param_names = NULL,
	};

	// Get the parameters
	arena_push_zone("MACRO_PARAMS");
	if (*(state->src - 1) != '(') goto skip_params;
	if (*state->src == ')') {
		state->src++;
		goto skip_params;
	}
	inner_state.param_names = macro->params;
	const char *param_start = state->src;
	param_t *param_buf = inner_state.param_bufs;
	int depth = 1;
	while (state->src < state->srcend) {
		const char c = *(state->src)++;
		if (c == ',' || (depth == 1 && c == ')' && state->src - param_start > 0)) {
			char *buf = arena_alloc(4096);
			state_t param_state = (state_t){
				.src = param_start,
				.srcend = state->src - 1,
				.buf = buf,
				.bufend = buf + 4096,
				.param_names = state->param_names,
			};
			memcpy(param_state.param_bufs, state->param_bufs, sizeof(state->param_bufs));
			process(processor, &param_state);
			*(param_buf++) = (param_t){
				.name = buf,
				.len = param_state.buf - buf,
			};
			param_start = state->src;
			if (c == ')' && depth-- == 1) break;
			continue;
		}
		if (c == '(') depth++;
		if (c == ')' && depth-- == 1) break;
	}
skip_params:
	// Do the macro
	state->buf = macro_start;
	process(processor, &inner_state);
	state->buf = inner_state.buf;

	// Cleanup
	arena_pop_zone("MACRO_PARAMS");
}

static void process(processor_t *const processor, state_t *const state) {
	bool line_start = true;
	const char *ident_start = NULL;
	while (state->src < state->srcend && state->buf != state->bufend) {
		const char c = *(state->src++);
		if (line_start && c == '#') {
			process_directive(processor, state);
			continue;
		}
		if (c == '/' && *state->src == '/') {
			skip_line(state);
			continue;
		}
		bool is_alnum = false;
		if (isalnum(c) || c == '_') {
			if (!ident_start) {
				ident_start = state->src - 1;
			}
			is_alnum = true;
		}
		*(state->buf)++ = c;
		if (ident_start && (!is_alnum || (state->src >= state->srcend))) {
			if (state->param_names) { // Do macro parameter
				for (param_t *param = state->param_names, *buf = state->param_bufs;
					param != state->param_names + MAX_PARAMS;
					param++, buf++
				) {
					if (!param->name) continue;
					size_t ident_len = state->src - ident_start;
					if (ident_len == param->len
						&& memcmp(ident_start, param->name, ident_len) == 0) {
						state->buf -= ident_len;
						printf("\n%s\n", buf->name);
						memcpy(state->buf, buf->name, buf->len);
						state->buf += buf->len;
						ident_start = NULL;
						goto skip_buffer;
					}
				}
			}

			// Do macro
			int *macro_idx = NULL;
			size_t ident_len = state->src - ident_start - 1;
			ident_map_get(processor->macro_names, ident_start, ident_len, &macro_idx);
			if (macro_idx) {
				process_macro_call(
					processor,
					state,
					processor->macros + *macro_idx,
					state->buf - ident_len - 1
				);
				ident_start = NULL;
				goto skip_buffer;
			}

			ident_start = NULL;
		}

		line_start = c == '\n';
skip_buffer:
	}
}

size_t preprocess(const char *src, char *buf, size_t buflen) {
	_buf = buf;
	arena_push_zone("PREPROCESSOR");
	processor_t processor = (processor_t){
		.macro_names = arena_alloc(sizeof(ident_map_t) + sizeof(ident_ent_t) * MAX_MACROS),
	};
	*processor.macro_names = ident_map_init(MAX_MACROS);
	state_t state = (state_t){
		.src = src,
		.srcend = src + strlen(src),
		.buf = buf,
		.bufend = buf + buflen,
		.param_names = NULL,
	};
	process(&processor, &state);
	*state.buf = '\0';
	arena_pop_zone("PREPROCESSOR");
	return state.bufend - buf;
}

