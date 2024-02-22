#ifndef _vm_h_
#define _vm_h_

#include "common.h"

#define unitty_def \
	enumdef(unit_i8) \
	enumdef(unit_u8) \
	enumdef(unit_i16) \
	enumdef(unit_u16) \
	enumdef(unit_i32) \
	enumdef(unit_u32) \
	enumdef(unit_f32) \
	enumdef(unit_i64) \
	enumdef(unit_u64) \
	enumdef(unit_f64) \
	enumdef(unit_ptr) \
	enumdef(unit_undefined) \
	enumdef(unit_maxty)

typedef enum unitty {
#define enumdef(e) e,
unitty_def
#undef enumdef
} unitty_t;

typedef union unit {
	int8_t i8;
	uint8_t u8;
	int16_t i16;
	uint16_t u16;
	int32_t i32;
	uint32_t u32;
	int64_t i64;
	uint64_t u64;
	uintptr_t ptr;
} unit_t;

typedef struct typed_unit {
	unitty_t ty;
	unit_t unit;
} typed_unit_t;

#define opcodety_def \
	enumdef(opcode_nop) \
	enumdef(opcode_pushimm) \
	enumdef(opcode_pushunit) \
	enumdef(opcode_pop) \
	enumdef(opcode_call) \
	enumdef(opcode_ret) \
	enumdef(opcode_exit) \
	enumdef(opcode_loadunit) \
	enumdef(opcode_loadmem) \
	enumdef(opcode_moveunit) \
	enumdef(opcode_storemem) \
	\
	enumdef(opcode_cmp) \
	enumdef(opcode_tst) \
	enumdef(opcode_beq) \
	enumdef(opcode_bne) \
	enumdef(opcode_bgt) \
	enumdef(opcode_blt) \
	enumdef(opcode_bge) \
	enumdef(opcode_ble) \
	enumdef(opcode_bra) \
	enumdef(opcode_jmp) \
	\
	enumdef(opcode_add) \
	enumdef(opcode_sub) \
	enumdef(opcode_mul) \
	enumdef(opcode_div) \
	enumdef(opcode_mod) \
	enumdef(opcode_neg) \
	enumdef(opcode_ext) \
	enumdef(opcode_inc) \
	enumdef(opcode_dec) \
	\
	enumdef(opcode_and) \
	enumdef(opcode_or) \
	enumdef(opcode_eor) \
	enumdef(opcode_not) \
	enumdef(opcode_shl) \
	enumdef(opcode_shr)
typedef enum opcodety {
#define enumdef(e) e,
opcodety_def
#undef enumdef
} opcodety_t;

typedef struct opcode {
	uint8_t ty;
	uint8_t op1ty : 4;
	uint8_t op2ty : 4;
	union {
		struct {
			uint8_t op1, op2;
		};
		uint16_t umm16;
		int16_t imm16;
		uint8_t umm8;
		int8_t imm8;
	};
} opcode_t;

typedef union codeunit {
	opcode_t op;
	int32_t i32;
	uint32_t u32;
} codeunit_t;

typed_unit_t interpret(const codeunit_t *code, unit_t *stack, size_t stacklen);

void dbg_typed_unit_print(const typed_unit_t unit);
void dbg_opcode_print(const codeunit_t *unit);

const char *unitty_to_str(const unitty_t ty);

#endif

