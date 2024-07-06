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
	int indent;

	struct {
		userty_t buf[256];
		size_t len;
	} userty;

	struct {
		strview_t buf[256];
		size_t len;
	} str;

	struct {
		int buf[256];
		size_t len;
	} offs;

	struct {
		type_t buf[1024];
		size_t len;
	} type;

	struct {
		tyref_t buf[512];
		size_t len;
	} tyref;

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
bool err_userty_max(cnms_t *st);
bool err_tyref_max(cnms_t *st);
bool err_str_max(cnms_t *st);
bool err_offs_max(cnms_t *st);
bool err_expect_ident(cnms_t *st, const char *situation);
bool err_type_size_unknown(cnms_t *st, tyref_t ty);

#endif

