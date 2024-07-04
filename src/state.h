#ifndef _state_h_
#define _state_h_

#include "common.h"
#include "type.h"
#include "vm.h"
#include "lexer.h"
#include "cnms.h"

//
// Main cnms state
//
struct cnms_s {
	strview_t src;
	tok_t tok;

	cnms_error_handler_t err;
};

void cnms_error(cnms_t *state, const char *msg);

#endif

