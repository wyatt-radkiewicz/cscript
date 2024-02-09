#ifndef _parser_h_
#define _parser_h_

#include "compile.h"

typedef enum prec {
	prec_grouping,
	prec_multiplicative,
	prec_additive,
	prec_expr,
} prec_t;

valref_t expression(compiler_t *const state, prec_t prec);

#endif

