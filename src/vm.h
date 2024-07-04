#ifndef _vm_h_
#define _vm_h_

#include <assert.h>
#include "common.h"

// opcode format a
// opcode: 6
// lhs type: 4
// rhs type: 4
// lhs loc: 9
// rhs loc: 9
//
// opcode format i
// opcode: 6
// type: 4
// ind: 1
// imm: 21
//
// opcode format j
// opcode: 6
// retoffs: 9
// jmpoffs: 17
typedef enum {
	// a format
	op_add,
	op_addi,
	op_sub,
	op_mul,
	op_div,
	op_mod,
	op_neg,

	op_and,
	op_or,
	op_xor,
	op_not,

	op_cast,

	op_seq,
	op_sne,
	op_sgt,
	op_slt,
	op_sge,
	op_sle,

	// i format
	op_imm,
	op_ld,
	op_st,
	op_ldn,
	op_stn,

	op_pop,
	op_push,
	op_ldcp,
	op_ldsp,

	op_bne,
	op_beq,

	// j format
	op_jal,
	op_jp,

	op_max,
} opcode_t;

typedef struct {
	uint32_t op : 6;
	union {
		struct {
			uint32_t lhs_type : 4;
			uint32_t rhs_type : 4;
			uint32_t lhs_loc : 9;
			uint32_t rhs_loc : 9;
		} a;
		struct {
			uint32_t type : 4;
			uint32_t ind : 1;
			uint32_t imm : 21;
		} i;
		struct {
			uint32_t ret_offs : 9;
			uint32_t jmp_offs : 17;
		} j;
	};
} instr_t;

static_assert(op_max < (1 << 6), "All opcodes must fall under this limit.");

typedef struct {
	struct {
		uint8_t *buf;
		size_t len;
	} code;
	struct {
		uint8_t *buf;
		size_t len;
	} stack;

	uint8_t *sp;
	instr_t *cp;
} vm_t;



#endif

