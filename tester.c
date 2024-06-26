#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define CS_IMPLEMENTATION
#define CS_PANIC(MSG) {printf(MSG); assert(0);}
#include "cscript.h"

void printerr(void *user, const char *msg) {
	printf("%s", msg);
}

int main(int argc, char **argv) {
	const char *src = "-1 * 5 + true";
	cs_state_t state = {
		.src		= src,
		.srcbegin	= src,
		.code		= NULL,
		.outbegin	= NULL,
		.data		= NULL,
		.outend		= NULL,
		.error_writer	= printerr,
	};

	cs_type_t type = cs_eval_type(&state);

	return 0;
}

