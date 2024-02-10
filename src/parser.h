#ifndef _parser_h_
#define _parser_h_

#include "compile.h"

typedef enum prec {
	// Left to Right
	prec_grouping,

	// Right to Left
	prec_2,

	// Left to right
	prec_multiplicative,
	prec_additive,
	prec_bitshifting,
	prec_relational,
	prec_eqeq,
	prec_bitand,
	prec_bitxor,
	prec_bitor,
	prec_and,
	prec_or,

	// Right to Left
	prec_ternary,
	prec_assign,

	// Left to Right
	prec_comma,
	prec_full,
	prec_null,
} prec_t;

typedef struct parser_ctx {
} parser_ctx_t;

typedef valref_t(parse_unary_fn)(compiler_t *const state, parser_ctx_t ctx);
typedef valref_t(parse_binary_fn)(
	compiler_t *const state,
	const prec_t prec,
	valref_t left,
	parser_ctx_t ctx
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
valref_t parse_expression(compiler_t *const state, const prec_t prec, parser_ctx_t ctx);

#endif

