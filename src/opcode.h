#ifndef _opcode_h_
#define _opcode_h_

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#define VM_OPCODES \
	X(op_load_data) \
	X(op_store_data) \
	X(op_load_data_indirect) \
	X(op_store_data_indirect) \
    X(op_load_indirect) \
    X(op_store_indirect) \
    X(op_load_stack) \
	X(op_store_stack) \
    X(op_load_stack_indirect) \
	X(op_store_stack_indirect) \
	X(op_sub_stack) \
	X(op_add_stack) \
     \
    /* These operations assume 32bit stack values are pushed */ \
	X(op_add) \
	X(op_sub) \
	X(op_mul) \
	X(op_div) \
	X(op_and) \
	X(op_or) \
	X(op_xor) \
    X(op_sll) \
    X(op_srl) \
    X(op_sra) \
    X(op_not) \
    X(op_mod) \
    X(op_neg) \
     \
    X(op_extend) \
    X(op_reduce) \
    X(op_f2u) \
    X(op_f2i) \
    X(op_i2f) \
    X(op_u2f) \
    X(op_f2d) \
    X(op_d2f) \
     \
	X(op_push_eq) \
	X(op_push_ne) \
	X(op_push_gt) \
	X(op_push_lt) \
	X(op_push_ge) \
	X(op_push_le) \
     \
	X(op_ret) \
	X(op_call) \
    X(op_call_indirect) \
    X(op_extern_call) \
    X(op_extern_call_indirect) \
	X(op_jump) \
    X(op_jump_indirect) \
	X(op_bne) \
	X(op_beq) \
     \
    X(op_imm_i8) \
    X(op_imm_i16) \
    X(op_imm_i32) \
    X(op_imm_i64) \
    X(op_imm_u8) \
    X(op_imm_u16) \
    X(op_imm_u32) \
    X(op_imm_u64) \
    X(op_imm_f32) \
    X(op_imm_f64) \
    X(op_imm_ptr) \
     \
    X(op_alloc) \
    X(op_alloc_indirect) \
    X(op_free_indirect)

#define vm_opcode_max (op_free_indirect + 1)
#define op_illegal vm_opcode_max

#define X(ENUM) ENUM,
typedef enum vm_opcode {
    VM_OPCODES
} vm_opcode_t;
#undef X
static_assert(vm_opcode_max <= 64, "Number of opcodes must be under 64!");

// Any function with an sp paramter, uses that to update it as a virtual stack
// pointer.

void emit_op_load_data(uint8_t **code, int32_t *sp, uint32_t offs, uint32_t n);
void emit_op_store_data(uint8_t **code, uint32_t offs, uint32_t n);
// Expects the stack to have the offset
void emit_op_load_data_indirect(uint8_t **code, int32_t *sp, uint32_t n);
void emit_op_store_data_indirect(uint8_t **code, uint32_t n);
// Expects the stack to have the pointer
void emit_op_load_indirect(uint8_t **code, int32_t *sp, uint32_t n);
void emit_op_store_indirect(uint8_t **code, uint32_t n);
void emit_op_load_stack(uint8_t **code, int32_t *sp, int32_t offs, uint32_t n);
void emit_op_store_stack(uint8_t **code, int32_t offs, uint32_t n);
// Stack loading operations start from the call stack base pointer
// (the callstack base pointer points to the top of the stack at when the
//  call operation was ran)
// Expects the stack to have the offset pushed on
// (the offset after the offset itself is pushed on of course)
// The offs value there will be added onto the offset found on the
// stack as well
void emit_op_load_stack_indirect(uint8_t **code, int32_t *sp, int32_t offs, uint32_t n);
void emit_op_store_stack_indirect(uint8_t **code, int32_t offs, uint32_t n);
void emit_op_sub_stack(uint8_t **code, int32_t *sp, uint32_t n);
void emit_op_add_stack(uint8_t **code, int32_t *sp, uint32_t n);
void emit_op_add(uint8_t **code, int32_t *sp, bool bits64, bool u, bool fp);
void emit_op_sub(uint8_t **code, int32_t *sp, bool bits64, bool u, bool fp);
void emit_op_mul(uint8_t **code, int32_t *sp, bool bits64, bool u, bool fp);
void emit_op_div(uint8_t **code, int32_t *sp, bool bits64, bool u, bool fp);
void emit_op_and(uint8_t **code, bool bits64);
void emit_op_or(uint8_t **code, bool bits64);
void emit_op_xor(uint8_t **code, bool bits64);
void emit_op_sll(uint8_t **code, bool bits64);
void emit_op_srl(uint8_t **code, bool bits64);
void emit_op_sra(uint8_t **code, bool bits64);
void emit_op_not(uint8_t **code, bool bits64);
void emit_op_mod(uint8_t **code, int32_t *sp, bool bits64, bool u);
void emit_op_neg(uint8_t **code, bool bits64, bool fp);
// Extends 1 rank above
// 0 -> 8bit
// 1 -> 16bit
// 2 -> 32bit
// 3 -> 64bit
void emit_op_extend(uint8_t **code, int32_t *sp, uint8_t size_class, bool u);
// Reduces 1 rank below
void emit_op_reduce(uint8_t **code, int32_t *sp, uint8_t size_class);
void emit_op_f2u(uint8_t **code, bool bits64);
void emit_op_f2i(uint8_t **code, bool bits64);
void emit_op_i2f(uint8_t **code, bool bits64);
void emit_op_u2f(uint8_t **code, bool bits64);
void emit_op_f2d(uint8_t **code, int32_t *sp);
void emit_op_d2f(uint8_t **code, int32_t *sp);
void emit_op_push_eq(uint8_t **code, int32_t *sp, bool bits64, bool fp);
void emit_op_push_ne(uint8_t **code, int32_t *sp, bool bits64, bool fp);
void emit_op_push_gt(uint8_t **code, int32_t *sp, bool bits64, bool fp, bool u);
void emit_op_push_lt(uint8_t **code, int32_t *sp, bool bits64, bool fp, bool u);
void emit_op_push_ge(uint8_t **code, int32_t *sp, bool bits64, bool fp, bool u);
void emit_op_push_le(uint8_t **code, int32_t *sp, bool bits64, bool fp, bool u);
void emit_op_ret(uint8_t **code);
void emit_op_call(uint8_t **code, uint32_t codeoffs);
void emit_op_call_indirect(uint8_t **code);
void emit_op_extern_call(uint8_t **code, uint32_t fn_idx);
void emit_op_extern_call_indirect(uint8_t **code);
// These instructions don't grow or shrink depending on the jump size
// to help with compiling
void emit_op_jump(uint8_t **code, int32_t offs);
void emit_op_jump_indirect(uint8_t **code);
// These pop off a 32bit value from the stack and check it, then branch
void emit_op_bne(uint8_t **code, int32_t *sp, int32_t offs, bool bits64);
void emit_op_beq(uint8_t **code, int32_t *sp, int32_t offs, bool bits64);
void emit_op_imm_i8(uint8_t **code, int32_t *sp, int8_t imm);
void emit_op_imm_i16(uint8_t **code, int32_t *sp, int16_t imm);
void emit_op_imm_i32(uint8_t **code, int32_t *sp, int32_t imm);
void emit_op_imm_i64(uint8_t **code, int32_t *sp, int64_t imm);
void emit_op_imm_u8(uint8_t **code, int32_t *sp, uint8_t imm);
void emit_op_imm_u16(uint8_t **code, int32_t *sp, uint16_t imm);
void emit_op_imm_u32(uint8_t **code, int32_t *sp, uint32_t imm);
void emit_op_imm_u64(uint8_t **code, int32_t *sp, uint64_t imm);
void emit_op_imm_f32(uint8_t **code, int32_t *sp, float imm);
void emit_op_imm_f64(uint8_t **code, int32_t *sp, double imm);
void emit_op_imm_ptr(uint8_t **code, int32_t *sp, void *imm);
// Expects size as u32 to be pushed onto the stack first
void emit_op_alloc(uint8_t **code, uint32_t n);
void emit_op_alloc_indirect(uint8_t **code);
// Expects pointer to be pushed onto the stack first
void emit_op_free_indirect(uint8_t **code);

#endif

