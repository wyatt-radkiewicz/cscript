#include <stdalign.h>

#include "common.h"
#include "opcode.h"

//
// Utilities
//

static inline int size_class_i32(int32_t n) {
    return (n >= INT8_MIN && n <= INT8_MAX) ? 0 : 0x80;
}
static inline int size_class_i64(int64_t n) {
    return (n >= INT8_MIN && n <= INT8_MAX) ? 0 : 0x80;
}
static inline int size_class_u16(uint16_t n) {
    return (n & 0xff00) == 0 ? 0 : 0x80;
}
static inline int size_class_u32(uint32_t n) {
    return (n & 0xffffff00) == 0 ? 0 : 0x80;
}
static inline int size_class_u64(uint64_t n) {
    return (n & 0xffffffffffffff00) == 0 ? 0 : 0x80;
}
static inline void emit_u8(uint8_t **code, uint8_t x) {
    *(*code)++ = x;
}
static inline void emit_u16(uint8_t **code, uint16_t x) {
    emit_u8(code, x & 0xff);
    emit_u8(code, (x >> 8) & 0xff);
}
static inline void emit_u32(uint8_t **code, uint32_t x) {
    emit_u8(code, x & 0xff);
    emit_u8(code, (x >> 8) & 0xff);
    emit_u8(code, (x >> 16) & 0xff);
    emit_u8(code, (x >> 24) & 0xff);
}
static inline void emit_u64(uint8_t **code, uint64_t x) {
    emit_u8(code, x & 0xff);
    emit_u8(code, (x >> 8) & 0xff);
    emit_u8(code, (x >> 16) & 0xff);
    emit_u8(code, (x >> 24) & 0xff);
    emit_u8(code, (x >> 32) & 0xff);
    emit_u8(code, (x >> 40) & 0xff);
    emit_u8(code, (x >> 48) & 0xff);
    emit_u8(code, (x >> 56) & 0xff);
}
static inline void emit_i8(uint8_t **code, int8_t x) {
    emit_u8(code, *(uint8_t *)(&x));
}
static inline void emit_i16(uint8_t **code, int16_t x) {
    emit_u16(code, *(uint16_t *)(&x));
}
static inline void emit_i32(uint8_t **code, int32_t x) {
    emit_u32(code, *(uint32_t *)(&x));
}
static inline void emit_i64(uint8_t **code, int64_t x) {
    emit_u64(code, *(uint64_t *)(&x));
}
static inline void emit_f32(uint8_t **code, float x) {
    emit_u32(code, *(uint32_t *)(&x));
}
static inline void emit_f64(uint8_t **code, double x) {
    emit_u64(code, *(uint64_t *)(&x));
}
static inline void emit_ptr(uint8_t **code, const void *p) {
    emit_u64(code, (uintptr_t)p);
}

//
// Opcode creation functions
//
void emit_op_load_data(uint8_t **code, int32_t *sp, uint32_t offs, uint32_t n) {
    emit_u8(code, op_load_data | size_class_u32(offs) | size_class_u32(n));
    *sp -= n;
    if (size_class_u32(offs) || size_class_u32(n)) {
        emit_u32(code, offs);
        emit_u32(code, n);
    } else {
        emit_u8(code, offs & 0xff);
        emit_u8(code, n & 0xff);
    }
}
void emit_op_store_data(uint8_t **code, uint32_t offs, uint32_t n) {
    emit_u8(code, op_store_data | size_class_u32(offs) | size_class_u32(n));
    if (size_class_u32(offs) || size_class_u32(n)) {
        emit_u32(code, offs);
        emit_u32(code, n);
    } else {
        emit_u8(code, offs & 0xff);
        emit_u8(code, n & 0xff);
    }
}
void emit_op_load_data_indirect(uint8_t **code, int32_t *sp, uint32_t n) {
    emit_u8(code, op_load_data_indirect | size_class_u32(n));
    *sp -= n;
    if (size_class_u32(n)) emit_u32(code, n);
    else emit_u8(code, n & 0xff);
}
void emit_op_store_data_indirect(uint8_t **code, uint32_t n) {
    emit_u8(code, op_store_data_indirect | size_class_u32(n));
    if (size_class_u32(n)) emit_u32(code, n);
    else emit_u8(code, n & 0xff);
}
void emit_op_load_indirect(uint8_t **code, int32_t *sp, uint32_t n) {
    emit_u8(code, op_load_indirect | size_class_u32(n));
    *sp -= n;
    if (size_class_u32(n)) emit_u32(code, n);
    else emit_u8(code, n & 0xff);
}
void emit_op_store_indirect(uint8_t **code, uint32_t n) {
    emit_u8(code, op_store_indirect | size_class_u32(n));
    if (size_class_u32(n)) emit_u32(code, n);
    else emit_u8(code, n & 0xff);
}
void emit_op_load_stack(uint8_t **code, int32_t *sp, int32_t offs, uint32_t n) {
    emit_u8(code, op_load_stack | size_class_i32(offs) | size_class_i32(n));
    *sp -= n;
    if (size_class_i32(offs) || size_class_i32(n)) {
        emit_i32(code, offs);
        emit_i32(code, n);
    } else {
        emit_i8(code, offs);
        emit_i8(code, n);
    }
}
void emit_op_store_stack(uint8_t **code, int32_t offs, uint32_t n) {
    if (!offs || !n) return;
    emit_u8(code, op_store_stack | size_class_i32(offs) | size_class_i32(n));
    if (size_class_i32(offs) || size_class_i32(n)) {
        emit_i32(code, offs);
        emit_i32(code, n);
    } else {
        emit_i8(code, offs);
        emit_i8(code, n);
    }
}
void emit_op_load_stack_indirect(uint8_t **code, int32_t *sp, int32_t offs, uint32_t n) {
    emit_u8(code, op_load_stack_indirect | size_class_i32(offs) | size_class_i32(n));
    *sp -= n;
    if (size_class_i32(offs) || size_class_i32(n)) {
        emit_i32(code, offs);
        emit_i32(code, n);
    } else {
        emit_i8(code, offs);
        emit_i8(code, n);
    }
}
void emit_op_store_stack_indirect(uint8_t **code, int32_t offs, uint32_t n) {
    emit_u8(code, op_store_stack_indirect | size_class_i32(offs) | size_class_i32(n));
    if (size_class_i32(offs) || size_class_i32(n)) {
        emit_i32(code, offs);
        emit_i32(code, n);
    } else {
        emit_i8(code, offs);
        emit_i8(code, n);
    }
}
void emit_op_sub_stack(uint8_t **code, int32_t *sp, uint32_t n) {
    if (!n) return;
    emit_u8(code, op_sub_stack | size_class_u32(n));
    if (size_class_u32(n)) emit_u32(code, n);
    else emit_u8(code, n & 0xff);
    *sp -= n;
}
void emit_op_add_stack(uint8_t **code, int32_t *sp, uint32_t n) {
    if (!n) return;
    emit_u8(code, op_add_stack | size_class_u32(n));
    if (size_class_u32(n)) emit_u32(code, n);
    else emit_u8(code, n & 0xff);
    *sp += n;
}
void emit_op_add(uint8_t **code, int32_t *sp, bool bits64, bool u, bool fp) {
    assert(!(fp && u) && "Can not have unsigned bit and fp bit both set!");
    *sp += bits64 ? 8 : 4;
    emit_u8(code, op_add);
    emit_u8(code, bits64 | fp << 1 | u << 2);
}
void emit_op_sub(uint8_t **code, int32_t *sp, bool bits64, bool u, bool fp) {
    assert(!(fp && u) && "Can not have unsigned bit and fp bit both set!");
    *sp += bits64 ? 8 : 4;
    emit_u8(code, op_sub);
    emit_u8(code, bits64 | fp << 1 | u << 2);
}
void emit_op_mul(uint8_t **code, int32_t *sp, bool bits64, bool u, bool fp) {
    assert(!(fp && u) && "Can not have unsigned bit and fp bit both set!");
    *sp += bits64 ? 8 : 4;
    emit_u8(code, op_mul);
    emit_u8(code, bits64 | fp << 1 | u << 2);
}
void emit_op_div(uint8_t **code, int32_t *sp, bool bits64, bool u, bool fp) {
    assert(!(fp && u) && "Can not have unsigned bit and fp bit both set!");
    *sp += bits64 ? 8 : 4;
    emit_u8(code, op_div);
    emit_u8(code, bits64 | fp << 1 | u << 2);
}
void emit_op_and(uint8_t **code, bool bits64) {
    emit_u8(code, op_and | bits64 << 7);
}
void emit_op_or(uint8_t **code, bool bits64) {
    emit_u8(code, op_or | bits64 << 7);
}
void emit_op_xor(uint8_t **code, bool bits64) {
    emit_u8(code, op_xor | bits64 << 7);
}
void emit_op_sll(uint8_t **code, bool bits64) {
    emit_u8(code, op_sll | bits64 << 7);
}
void emit_op_srl(uint8_t **code, bool bits64) {
    emit_u8(code, op_srl | bits64 << 7);
}
void emit_op_sra(uint8_t **code, bool bits64) {
    emit_u8(code, op_sra | bits64 << 7);
}
void emit_op_not(uint8_t **code, bool bits64) {
    emit_u8(code, op_not | bits64 << 7);
}
void emit_op_mod(uint8_t **code, bool bits64, bool u) {
    emit_u8(code, op_mod);
    emit_u8(code, bits64 | u << 1);
}
void emit_op_neg(uint8_t **code, bool bits64, bool fp) {
    emit_u8(code, op_neg);
    emit_u8(code, bits64 | fp << 1);
}
void emit_op_extend(uint8_t **code, int32_t *sp, uint8_t size_class, bool u) {
    assert(size_class < 4 && "Extend size class is over 3!");
    emit_u8(code, op_extend | u << 7);
    emit_u8(code, size_class);
    *sp += 1 << size_class;
    *sp = alignid(*sp, 2 << size_class);
    *sp -= 2 << size_class;
}
void emit_op_reduce(uint8_t **code, int32_t *sp, uint8_t size_class) {
    assert(size_class > 0 && "Extend size class is 0!");
    emit_u8(code, op_reduce);
    emit_u8(code, size_class);
    *sp += 1 << (size_class - 1);
}
void emit_op_f2u(uint8_t **code, bool bits64) {
    emit_u8(code, op_f2u | bits64 << 7);
}
void emit_op_f2i(uint8_t **code, bool bits64) {
    emit_u8(code, op_f2i | bits64 << 7);
}
void emit_op_i2f(uint8_t **code, bool bits64) {
    emit_u8(code, op_i2f | bits64 << 7);
}
void emit_op_u2f(uint8_t **code, bool bits64) {
    emit_u8(code, op_u2f | bits64 << 7);
}
void emit_op_f2d(uint8_t **code, int32_t *sp) {
    emit_u8(code, op_f2d);
    *sp = alignid(*sp, 8);
}
void emit_op_d2f(uint8_t **code, int32_t *sp) {
    emit_u8(code, op_d2f);
    *sp += 4;
}
void emit_op_push_eq(uint8_t **code, int32_t *sp, bool bits64, bool fp) {
    emit_u8(code, op_push_eq);
    emit_u8(code, bits64 | fp << 1);
    *sp += bits64 ? 12 : 4;
}
void emit_op_push_ne(uint8_t **code, int32_t *sp, bool bits64, bool fp) {
    emit_u8(code, op_push_ne);
    emit_u8(code, bits64 | fp << 1);
    *sp += bits64 ? 12 : 4;
}
void emit_op_push_gt(uint8_t **code, int32_t *sp, bool bits64, bool fp, bool u) {
    assert(!(fp && u) && "Can not have unsigned bit and fp bit both set!");
    emit_u8(code, op_push_gt);
    emit_u8(code, bits64 | fp << 1 | u << 2);
    *sp += bits64 ? 12 : 4;
}
void emit_op_push_lt(uint8_t **code, int32_t *sp, bool bits64, bool fp, bool u) {
    assert(!(fp && u) && "Can not have unsigned bit and fp bit both set!");
    emit_u8(code, op_push_lt);
    emit_u8(code, bits64 | fp << 1 | u << 2);
    *sp += bits64 ? 12 : 4;
}
void emit_op_push_ge(uint8_t **code, int32_t *sp, bool bits64, bool fp, bool u) {
    assert(!(fp && u) && "Can not have unsigned bit and fp bit both set!");
    emit_u8(code, op_push_ge);
    emit_u8(code, bits64 | fp << 1 | u << 2);
    *sp += bits64 ? 12 : 4;
}
void emit_op_push_le(uint8_t **code, int32_t *sp, bool bits64, bool fp, bool u) {
    assert(!(fp && u) && "Can not have unsigned bit and fp bit both set!");
    emit_u8(code, op_push_le);
    emit_u8(code, bits64 | fp << 1 | u << 2);
    *sp += bits64 ? 12 : 4;
}
void emit_op_ret(uint8_t **code) {
    emit_u8(code, op_ret);
}
void emit_op_call(uint8_t **code, uint32_t codeoffs) {
    emit_u8(code, op_call);
    emit_u32(code, codeoffs);
}
void emit_op_call_indirect(uint8_t **code) {
    emit_u8(code, op_call_indirect);
}
void emit_op_extern_call(uint8_t **code, uint32_t fn_idx) {
    emit_u8(code, op_extern_call);
    emit_u32(code, fn_idx);
}
void emit_op_extern_call_indirect(uint8_t **code) {
    emit_u8(code, op_extern_call_indirect);
}
void emit_op_jump(uint8_t **code, int32_t offs) {
    if (offs == 5) return;
    emit_u8(code, op_jump);
    emit_i32(code, offs);
}
void emit_op_jump_indirect(uint8_t **code) {
    emit_u8(code, op_jump_indirect);
}
void emit_op_bne(uint8_t **code, int32_t *sp, int32_t offs, bool bits64) {
    emit_u8(code, op_bne | bits64 << 7);
    emit_i32(code, offs);
    *sp += bits64 ? 8 : 4;
}
void emit_op_beq(uint8_t **code, int32_t *sp, int32_t offs, bool bits64) {
    emit_u8(code, op_beq | bits64 << 7);
    emit_i32(code, offs);
    *sp += bits64 ? 8 : 4;
}
void emit_op_imm_i8(uint8_t **code, int32_t *sp, int8_t imm) {
    int32_t diff = *sp - alignid(*sp, alignof(int8_t));
    emit_u8(code, op_imm_i8);
    emit_i8(code, imm);
    *sp -= sizeof(imm) + diff;
}
void emit_op_imm_i16(uint8_t **code, int32_t *sp, int16_t imm) {
    int32_t diff = *sp - alignid(*sp, alignof(int16_t));
    emit_u8(code, op_imm_i16 | size_class_i32(imm));
    if (size_class_i32(imm)) emit_i16(code, imm);
    else emit_i8(code, imm);
    *sp -= sizeof(imm) + diff;
}
void emit_op_imm_i32(uint8_t **code, int32_t *sp, int32_t imm) {
    int32_t diff = *sp - alignid(*sp, alignof(int32_t));
    emit_u8(code, op_imm_i32 | size_class_i32(imm));
    if (size_class_i32(imm)) emit_i32(code, imm);
    else emit_i8(code, imm);
    *sp -= sizeof(imm) + diff;
}
void emit_op_imm_i64(uint8_t **code, int32_t *sp, int64_t imm) {
    int32_t diff = *sp - alignid(*sp, alignof(int64_t));
    emit_u8(code, op_imm_i64 | size_class_i64(imm));
    if (size_class_i64(imm)) emit_i64(code, imm);
    else emit_i8(code, imm);
    *sp -= sizeof(imm) + diff;
}
void emit_op_imm_u8(uint8_t **code, int32_t *sp, uint8_t imm) {
    int32_t diff = *sp - alignid(*sp, alignof(uint8_t));
    emit_u8(code, op_imm_u8);
    emit_u8(code, imm);
    *sp -= sizeof(imm) + diff;
}
void emit_op_imm_u16(uint8_t **code, int32_t *sp, uint16_t imm) {
    int32_t diff = *sp - alignid(*sp, alignof(uint16_t));
    emit_u8(code, op_imm_u16 | size_class_u16(imm));
    if (size_class_u16(imm)) emit_u16(code, imm);
    else emit_u8(code, imm & 0xff);
    *sp -= sizeof(imm) + diff;
}
void emit_op_imm_u32(uint8_t **code, int32_t *sp, uint32_t imm) {
    int32_t diff = *sp - alignid(*sp, alignof(uint32_t));
    emit_u8(code, op_imm_u32 | size_class_u32(imm));
    if (size_class_u32(imm)) emit_u32(code, imm);
    else emit_u8(code, imm & 0xff);
    *sp -= sizeof(imm) + diff;
}
void emit_op_imm_u64(uint8_t **code, int32_t *sp, uint64_t imm) {
    int32_t diff = *sp - alignid(*sp, alignof(uint64_t));
    emit_u8(code, op_imm_u64 | size_class_u64(imm));
    if (size_class_u64(imm)) emit_u64(code, imm);
    else emit_u8(code, imm & 0xff);
    *sp -= sizeof(imm) + diff;
}
void emit_op_imm_f32(uint8_t **code, int32_t *sp, float imm) {
    int32_t diff = *sp - alignid(*sp, alignof(float));
    emit_u8(code, op_imm_f32);
    emit_f32(code, imm);
    *sp -= sizeof(imm) + diff;
}
void emit_op_imm_f64(uint8_t **code, int32_t *sp, double imm) {
    int32_t diff = *sp - alignid(*sp, alignof(double));
    emit_u8(code, op_imm_f64);
    emit_f64(code, imm);
    *sp -= sizeof(imm) + diff;
}
void emit_op_imm_ptr(uint8_t **code, int32_t *sp, void *imm) {
    int32_t diff = *sp - alignid(*sp, alignof(void *));
    emit_u8(code, op_imm_ptr);
    emit_ptr(code, imm);
    *sp -= sizeof(imm) + diff;
}
void emit_op_alloc(uint8_t **code, uint32_t n) {
    emit_u8(code, op_alloc | size_class_u32(n));
    if (size_class_u32(n)) emit_u32(code, n);
    else emit_u8(code, n & 0xff);
}
void emit_op_alloc_indirect(uint8_t **code) {
    emit_u8(code, op_alloc_indirect);
}
void emit_op_free_indirect(uint8_t **code) {
    emit_u8(code, op_free_indirect);
}

