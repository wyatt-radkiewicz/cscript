#ifndef _opcode_h_
#define _opcode_h_

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum vm_opcode {
	op_load_data,
	op_store_data,
	op_load_data_indirect,
	op_store_data_indirect,
    op_load_indirect,
    op_store_indirect,

    op_load_stack,
	op_store_stack,
    op_load_stack_indirect,
	op_store_stack_indirect,
	op_sub_stack,
	op_add_stack,

    // These operations assume 32bit stack values are pushed
	op_add,
	op_sub,
	op_mul,
	op_div,
	op_and,
	op_or,
	op_xor,
    op_sll,
    op_srl,
    op_sra,

    op_extend,
    op_reduce,
    op_f2u,
    op_f2i,
    op_i2f,
    op_u2f,

	op_push_eq,
	op_push_ne,
	op_push_gt,
	op_push_lt,
	op_push_ge,
	op_push_le,

	op_ret,
	op_call,
    op_call_indirect,
    op_extern_call,
    op_extern_call_indirect,
	op_jump,
	op_bne,
	op_beq,

    op_imm_i8,
    op_imm_i16,
    op_imm_i32,
    op_imm_i64,
    op_imm_u8,
    op_imm_u16,
    op_imm_u32,
    op_imm_u64,
    op_imm_f32,
    op_imm_f64,
    op_imm_ptr,

    op_alloc_indirect,
    op_free_indirect,

    vm_opcode_max,
} vm_opcode_t;
static_assert(vm_opcode_max < 64, "Number of opcodes must be under 64!");

void emit_op_load_data(uint8_t **code, uint32_t offs, uint32_t n);
void emit_op_store_data(uint8_t **code, uint32_t offs, uint32_t n);
// Expects the stack to have the offset
void emit_op_load_data_indirect(uint8_t **code, uint32_t n);
void emit_op_store_data_indirect(uint8_t **code, uint32_t n);
// Expects the stack to have the pointer
void emit_op_load_indirect(uint8_t **code, uint32_t n);
void emit_op_store_indirect(uint8_t **code, uint32_t n);
// Stack loading operations start from the bottom of the stack
void emit_op_load_stack(uint8_t **code, uint32_t offs, uint32_t n);
void emit_op_store_stack(uint8_t **code, uint32_t offs, uint32_t n);
// Expects the stack to have the offset pushed on
// (the offset after the offset itself is pushed on of course)
// The offs value there will be added onto the offset found on the
// stack as well
void emit_op_load_stack_indirect(uint8_t **code, uint32_t offs, uint32_t n);
void emit_op_store_stack_indirect(uint8_t **code, uint32_t offs, uint32_t n);
void emit_op_sub_stack(uint8_t **code, uint32_t n);
void emit_op_add_stack(uint8_t **code, uint32_t n);
void emit_op_add(uint8_t **code, bool bits64, bool fp);
void emit_op_sub(uint8_t **code, bool bits64, bool fp);
void emit_op_mul(uint8_t **code, bool bits64, bool fp);
void emit_op_div(uint8_t **code, bool bits64, bool fp);
void emit_op_and(uint8_t **code, bool bits64);
void emit_op_or(uint8_t **code, bool bits64);
void emit_op_xor(uint8_t **code, bool bits64);
void emit_op_sll(uint8_t **code, bool bits64);
void emit_op_srl(uint8_t **code, bool bits64);
void emit_op_sra(uint8_t **code, bool bits64);
// By default extends to 32bit
void emit_op_extend(uint8_t **code, uint8_t from, bool to64, bool fp);
// By default reduces from 32bit
void emit_op_reduce(uint8_t **code, bool from64, uint8_t to, bool fp);
void emit_op_f2u(uint8_t **code, bool bits64);
void emit_op_f2i(uint8_t **code, bool bits64);
void emit_op_i2f(uint8_t **code, bool bits64);
void emit_op_u2f(uint8_t **code, bool bits64);
void emit_op_push_eq(uint8_t **code, bool bits64, bool fp);
void emit_op_push_ne(uint8_t **code, bool bits64, bool fp);
void emit_op_push_gt(uint8_t **code, bool bits64, bool fp);
void emit_op_push_lt(uint8_t **code, bool bits64, bool fp);
void emit_op_push_ge(uint8_t **code, bool bits64, bool fp);
void emit_op_push_le(uint8_t **code, bool bits64, bool fp);
void emit_op_ret(uint8_t **code);
void emit_op_call(uint8_t **code, uint32_t codeoffs);
void emit_op_call_indirect(uint8_t **code);
void emit_op_extern_call(uint8_t **code, void *pfn);
void emit_op_extern_call_indirect(uint8_t **code);
// These instructions don't grow or shrink depending on the jump size
// to help with compiling
void emit_op_jump(uint8_t **code, int32_t offs);
void emit_op_bne(uint8_t **code, int32_t offs);
void emit_op_beq(uint8_t **code, int32_t offs);
void emit_op_imm_i8(uint8_t **code, int8_t imm);
void emit_op_imm_i16(uint8_t **code, int16_t imm);
void emit_op_imm_i32(uint8_t **code, int32_t imm);
void emit_op_imm_i64(uint8_t **code, int64_t imm);
void emit_op_imm_u8(uint8_t **code, uint8_t imm);
void emit_op_imm_u16(uint8_t **code, uint16_t imm);
void emit_op_imm_u32(uint8_t **code, uint32_t imm);
void emit_op_imm_u64(uint8_t **code, uint64_t imm);
void emit_op_imm_f32(uint8_t **code, float imm);
void emit_op_imm_f64(uint8_t **code, double imm);
void emit_op_imm_ptr(uint8_t **code, void *imm);
// Expects size as u32 to be pushed onto the stack first
void emit_op_alloc_indirect(uint8_t **code);
// Expects pointer to be pushed onto the stack first
void emit_op_free_indirect(uint8_t **code);

#endif

