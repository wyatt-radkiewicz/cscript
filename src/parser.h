#ifndef _parser_h_
#define _parser_h_

#include "compile.h"

typedef enum prec {
	prec_grouping,
	prec_2,
	prec_multiplicative,
	prec_additive,
	prec_bitshifting,
	prec_expr,
	prec_null,
} prec_t;

typedef valref_t(parse_unary_fn)(compiler_t *const state);
typedef valref_t(parse_binary_fn)(
	compiler_t *const state,
	const prec_t prec,
	valref_t left
);
typedef parse_unary_fn *parse_unary_pfn;
typedef parse_binary_fn *parse_binary_pfn;

typedef struct parse_rule {
	parse_unary_pfn prefix;
	parse_binary_pfn infix;
	prec_t prec;
} parse_rule_t;


parse_unary_fn parse_number;
parse_unary_fn parse_unary;
parse_binary_fn parse_binary;
valref_t parse_expression(compiler_t *const state, const prec_t prec);

#endif

