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

	struct {
		cnms_error_handler_t fn;
		void *user;
		bool ok;
	} err;
};

bool cnms_error(cnms_t *st, const char *msg);

//
// Error functions
//
bool err_tybuf_max(cnms_t *st);
bool err_unknown_token(cnms_t *st);
bool err_unknown_ident(cnms_t *st, strview_t ident);
bool err_any_type(cnms_t *st);
bool err_slice_bracket(cnms_t *st);
bool err_no_void(cnms_t *st);

#endif

