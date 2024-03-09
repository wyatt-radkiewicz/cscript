#include <string.h>

#include "mem.h"
#include "preprocess.h"

#define MAX_MACROS 512
#define MAX_PARAMS 32

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
	const char *src;
	char *bufend, *bufhead;

	macro_t macros[MAX_MACROS];
	ident_map_t *macro_names;
} state_t;

static void process_directive(state_t *const state) {
	while (*(state->src++) != '\n');
}

static void process(state_t *const state) {
	bool line_start = true;
	while (*state->src && state->bufhead != state->bufend) {
		const char c = *(state->src++);
		if (line_start && c == '#') {
			process_directive(state);
			continue;
		}
		if (c == '/' && *state->src == '/') {
			while (*(state->src++) != '\n');
			continue;
		}
		*(state->bufhead)++ = c;
		line_start = c == '\n';
	}
}

size_t preprocess(const char *src, char *buf, size_t buflen) {
	state_t state = (state_t){
		.src = src,
		.bufend = buf + buflen - 1,
		.bufhead = buf,
		.macro_names = arena_alloc(sizeof(ident_map_t) + sizeof(ident_ent_t) * MAX_MACROS),
	};
	*state.macro_names = ident_map_init(MAX_MACROS);
	process(&state);
	*state.bufhead = '\0';
	return state.bufend - buf;
}

