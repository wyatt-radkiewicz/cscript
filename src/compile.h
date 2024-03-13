#ifndef _compile_h_
#define _compile_h_

#include "lexer.h"
#include "vm.h"
#include "parser.h"

#define MAX_PARAMS 32
#define MAX_FUNCS 256
#define MAX_VARS 64
#define MAX_SCOPES 16

typedef struct strview {
	const char *str;
	size_t len;
} strview_t;

typedef struct valref {
	unitty_t ty;
	int abs_idx;
} valref_t;

typedef struct func {
	const ast_t *def;
	unitty_t ret;
	unitty_t params[MAX_PARAMS];
	strview_t param_names[MAX_PARAMS];
	int nparams;
} func_t;

typedef struct scope {
	int stacktop;
	valref_t vars[MAX_VARS];
	ident_map_t *names;
} scope_t;

typedef struct compiler {
	codeunit_t *code;
	const codeunit_t *code_end;
	uint8_t *data;
	uint8_t *data_end;

	err_t errs[32];
	size_t nerrs;

	int stacktop;

	func_t funcs[MAX_FUNCS];
	ident_map_t *func_names;

	scope_t *scopes;
	scope_t *scope;
} compiler_t;

compiler_t *compile(
	ast_t *ast,
	codeunit_t *code,
	size_t codelen,
	uint8_t *dataseg,
	size_t datalen
);

#endif

