#ifndef _compile_h_
#define _compile_h_

#include "lexer.h"
#include "vm.h"
#include "parser.h"

typedef struct branchref {
	codeunit_t *branch;
	int branch_target;
} branchref_t;

typedef struct branch_target {
	const codeunit_t *target;
	int nrefs;
} branch_target_t;

typedef struct valref {
	unitty_t ty;
	int abs_idx;
} valref_t;

typedef struct compiler {
	codeunit_t *code;
	const codeunit_t *const code_end;
	uint8_t *data;
	uint8_t *const data_end;
	err_t errs[32];
	size_t nerrs;

	int stacktop;

	branchref_t branch_refs[64];
	int brefs_top;
	const codeunit_t *branch_targets[64];
	int btargets_top;
} compiler_t;

compiler_t compile(
	codeunit_t *code,
	size_t codelen,
	uint8_t *dataseg,
	size_t datalen
);

#endif

