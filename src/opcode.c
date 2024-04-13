#include "opcode.h"

#define emit(BYTE) (*((*code)++) = (BYTE))

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
static inline void emit_u16(uint8_t **code, uint16_t x) {
    emit(x & 0xff);
    emit((x >> 8) & 0xff);
}
static inline void emit_u32(uint8_t **code, uint32_t x) {
    emit(x & 0xff);
    emit((x >> 8) & 0xff);
    emit((x >> 16) & 0xff);
    emit((x >> 24) & 0xff);
}
static inline void emit_u64(uint8_t **code, uint64_t x) {
    emit(x & 0xff);
    emit((x >> 8) & 0xff);
    emit((x >> 16) & 0xff);
    emit((x >> 24) & 0xff);
    emit((x >> 32) & 0xff);
    emit((x >> 40) & 0xff);
    emit((x >> 48) & 0xff);
    emit((x >> 56) & 0xff);
}
static inline void emit_ptr(uint8_t **code, const void *p) {
    emit_u64(code, (uintptr_t)p);
}

void emit_op_load_data(uint8_t **code, uint32_t offs, uint32_t n) {
    emit(op_load_data | size_class_u32(offs) | size_class_u32(n));
    if (size_class_u32(offs) || size_class_u32(n)) {
        emit_u32(code, offs);
        emit_u32(code, n);
    } else {
        emit(offs & 0xff);
        emit(n & 0xff);
    }
}
void emit_op_store_data(uint8_t **code, uint32_t offs, uint32_t n) {
    emit(op_store_data | size_class_u32(offs) | size_class_u32(n));
    if (size_class_u32(offs) || size_class_u32(n)) {
        emit_u32(code, offs);
        emit_u32(code, n);
    } else {
        emit(offs & 0xff);
        emit(n & 0xff);
    }
}
void emit_op_load_data_indirect(uint8_t **code, uint32_t n) {
    emit(op_load_data_indirect | size_class_u32(n));
    if (size_class_u32(n)) emit_u32(code, n);
    else emit(n & 0xff);
}
void emit_op_store_data_indirect(uint8_t **code, uint32_t n) {
    emit(op_store_data_indirect | size_class_u32(n));
    if (size_class_u32(n)) emit_u32(code, n);
    else emit(n & 0xff);
}
void emit_op_load_indirect(uint8_t **code, uint32_t n) {
    emit(op_load_indirect | size_class_u32(n));
    if (size_class_u32(n)) emit_u32(code, n);
    else emit(n & 0xff);
}
void emit_op_store_indirect(uint8_t **code, uint32_t n) {
    emit(op_store_indirect | size_class_u32(n));
    if (size_class_u32(n)) emit_u32(code, n);
    else emit(n & 0xff);
}
void emit_op_load_stack(uint8_t **code, uint32_t offs, uint32_t n) {
    emit(op_load_stack | size_class_u32(offs) | size_class_u32(n));
    if (size_class_u32(offs) || size_class_u32(n)) {
        emit_u32(code, offs);
        emit_u32(code, n);
    } else {
        emit(offs & 0xff);
        emit(n & 0xff);
    }
}
void emit_op_store_stack(uint8_t **code, uint32_t offs, uint32_t n) {
    emit(op_store_stack | size_class_u32(offs) | size_class_u32(n));
    if (size_class_u32(offs) || size_class_u32(n)) {
        emit_u32(code, offs);
        emit_u32(code, n);
    } else {
        emit(offs & 0xff);
        emit(n & 0xff);
    }
}
void emit_op_load_stack_indirect(uint8_t **code, uint32_t n) {
    emit(op_load_stack_indirect | size_class_u32(n));
    if (size_class_u32(n)) emit_u32(code, n);
    else emit(n & 0xff);
}
void emit_op_store_stack_indirect(uint8_t **code, uint32_t n) {
    emit(op_store_stack_indirect | size_class_u32(n));
    if (size_class_u32(n)) emit_u32(code, n);
    else emit(n & 0xff);
}
void emit_op_sub_stack(uint8_t **code, uint32_t n) {
    emit(op_sub_stack | size_class_u32(n));
    if (size_class_u32(n)) emit_u32(code, n);
    else emit(n & 0xff);
}
void emit_op_add_stack(uint8_t **code, uint32_t n) {
    emit(op_add_stack | size_class_u32(n));
    if (size_class_u32(n)) emit_u32(code, n);
    else emit(n & 0xff);
}
void emit_op_add(uint8_t **code, bool bits64, bool fp) {
    emit(op_add);
    emit(bits64 | (fp << 1));
}
void emit_op_sub(uint8_t **code, bool bits64, bool fp) {
    emit(op_sub);
    emit(bits64 | (fp << 1));
}
void emit_op_mul(uint8_t **code, bool bits64, bool fp) {
    emit(op_mul);
    emit(bits64 | (fp << 1));
}
void emit_op_div(uint8_t **code, bool bits64, bool fp) {
    emit(op_div);
    emit(bits64 | (fp << 1));
}
void emit_op_and(uint8_t **code, bool bits64) {
    emit(op_and | bits64 << 7);
}
void emit_op_or(uint8_t **code, bool bits64) {
    emit(op_or | bits64 << 7);
}
void emit_op_xor(uint8_t **code, bool bits64) {
    emit(op_xor | bits64 << 7);
}
void emit_op_sll(uint8_t **code, bool bits64) {
    emit(op_sll | bits64 << 7);
}
void emit_op_srl(uint8_t **code, bool bits64) {
    emit(op_srl | bits64 << 7);
}
void emit_op_sra(uint8_t **code, bool bits64) {
    emit(op_sra | bits64 << 7);
}
void emit_op_extend(uint8_t **code, uint8_t from, bool to64, bool fp) {
    emit(op_extend | fp << 7);
    emit((from & 0x7f) | (to64 << 7));
}
void emit_op_reduce(uint8_t **code, bool from64, uint8_t to, bool fp) {
    emit(op_reduce | fp << 7);
    emit((to & 0x7f) | (from64 << 7));
}
void emit_op_f2u(uint8_t **code, bool bits64) {
    emit(op_f2u | bits64 << 7);
}
void emit_op_f2i(uint8_t **code, bool bits64) {
    emit(op_f2i | bits64 << 7);
}
void emit_op_i2f(uint8_t **code, bool bits64) {
    emit(op_i2f | bits64 << 7);
}
void emit_op_u2f(uint8_t **code, bool bits64) {
    emit(op_u2f | bits64 << 7);
}
void emit_op_push_eq(uint8_t **code, bool bits64, bool fp) {
    emit(op_push_eq);
    emit(bits64 | (fp << 1));
}
void emit_op_push_ne(uint8_t **code, bool bits64, bool fp) {
    emit(op_push_ne);
    emit(bits64 | (fp << 1));
}
void emit_op_push_gt(uint8_t **code, bool bits64, bool fp) {
    emit(op_push_gt);
    emit(bits64 | (fp << 1));
}
void emit_op_push_lt(uint8_t **code, bool bits64, bool fp) {
    emit(op_push_lt);
    emit(bits64 | (fp << 1));
}
void emit_op_push_ge(uint8_t **code, bool bits64, bool fp) {
    emit(op_push_ge);
    emit(bits64 | (fp << 1));
}
void emit_op_push_le(uint8_t **code, bool bits64, bool fp) {
    emit(op_push_le);
    emit(bits64 | (fp << 1));
}
void emit_op_ret(uint8_t **code) {
    emit(op_ret);
}
void emit_op_call(uint8_t **code, uint32_t codeoffs) {
    emit(op_call);
    emit_u32(code, codeoffs);
}
void emit_op_call_indirect(uint8_t **code) {
    emit(op_call_indirect);
}
void emit_op_extern_call(uint8_t **code, void *pfn) {
    emit(op_extern_call);
    emit_ptr(code, pfn);
}
void emit_op_extern_call_indirect(uint8_t **code) {
    emit(op_extern_call_indirect);
}
void emit_op_jump(uint8_t **code, int32_t offs) {
    emit(op_jump);
    emit_u32(code, *(uint32_t *)(&offs));
}
void emit_op_bne(uint8_t **code, int32_t offs) {
    emit(op_bne);
    emit_u32(code, *(uint32_t *)(&offs));
}
void emit_op_beq(uint8_t **code, int32_t offs) {
    emit(op_beq);
    emit_u32(code, *(uint32_t *)(&offs));
}
void emit_op_imm_i8(uint8_t **code, int8_t imm) {
    emit(op_imm_i8);
    emit(*(uint8_t *)(&imm));
}
void emit_op_imm_i16(uint8_t **code, int16_t imm) {
    emit(op_imm_i16 | size_class_i32(imm));
    if (size_class_i32(imm)) emit_u16(code, *(uint16_t *)(&imm));
    else emit((int8_t)imm);
}
void emit_op_imm_i32(uint8_t **code, int32_t imm) {
    emit(op_imm_i32 | size_class_i32(imm));
    if (size_class_i32(imm)) emit_u32(code, *(uint32_t *)(&imm));
    else emit((int8_t)imm);
}
void emit_op_imm_i64(uint8_t **code, int64_t imm) {
    emit(op_imm_i64 | size_class_i64(imm));
    if (size_class_i64(imm)) emit_u64(code, *(uint64_t *)(&imm));
    else emit((int8_t)imm);
}
void emit_op_imm_u8(uint8_t **code, uint8_t imm) {
    emit(op_imm_u8);
    emit(imm);
}
void emit_op_imm_u16(uint8_t **code, uint16_t imm) {
    emit(op_imm_u16 | size_class_u16(imm));
    if (size_class_u16(imm)) emit_u16(code, imm);
    else emit(imm & 0xff);
}
void emit_op_imm_u32(uint8_t **code, uint32_t imm) {
    emit(op_imm_u32 | size_class_u32(imm));
    if (size_class_u32(imm)) emit_u32(code, imm);
    else emit(imm & 0xff);
}
void emit_op_imm_u64(uint8_t **code, uint64_t imm) {
    emit(op_imm_u64 | size_class_u64(imm));
    if (size_class_u64(imm)) emit_u64(code, imm);
    else emit(imm & 0xff);
}
void emit_op_imm_f32(uint8_t **code, float imm) {
    emit(op_imm_f32);
    emit_u32(code, *(uint32_t *)(&imm));
}
void emit_op_imm_f64(uint8_t **code, double imm) {
    emit(op_imm_f64);
    emit_u64(code, *(uint64_t *)(&imm));
}
void emit_op_imm_ptr(uint8_t **code, void *imm) {
    emit(op_imm_ptr);
    emit_ptr(code, imm);
}
void emit_op_alloc_indirect(uint8_t **code) {
    emit(op_alloc_indirect);
}
void emit_op_free_indirect(uint8_t **code) {
    emit(op_free_indirect);
}

