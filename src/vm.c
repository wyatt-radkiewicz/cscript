#include <stdalign.h>
#include <string.h>

#include "vm.h"

static inline vm_error_t vm_error_init(vm_state_t *state, vm_error_code_t code) {
    return (vm_error_t){
        .errcode = code,
        .stack_pointer = state->sp,
        .stack_size = state->stack + state->stack_size - state->sp,
        .program_counter = (intptr_t)state->pc - (intptr_t)state->code,
        .callstack_len = state->rp - state->callstack,
    };
}

static inline uint8_t take_u8(vm_state_t *state) {
    return *state->pc++;
}
static inline uint16_t take_u16(vm_state_t *state) {
    uint16_t x = *state->pc++;
    x |= *state->pc++ << 8;
    return x;
}
static inline uint32_t take_u32(vm_state_t *state) {
    uint32_t x = *state->pc++;
    x |= *state->pc++ << 8;
    x |= *state->pc++ << 16;
    x |= *state->pc++ << 24;
    return x;
}
static inline uint64_t take_u64(vm_state_t *state) {
    uint64_t x = *state->pc++;
    x |= (uint64_t)(*state->pc++) << 8;
    x |= (uint64_t)(*state->pc++) << 16;
    x |= (uint64_t)(*state->pc++) << 24;
    x |= (uint64_t)(*state->pc++) << 32;
    x |= (uint64_t)(*state->pc++) << 40;
    x |= (uint64_t)(*state->pc++) << 48;
    x |= (uint64_t)(*state->pc++) << 56;
    return x;
}
#define cast_take_template(NAME, TYPE, WIDTH) \
    static inline TYPE take_##NAME(vm_state_t *state) { \
        TYPE x; \
        *(uint##WIDTH##_t *)(&x) = take_u##WIDTH(state); \
        return x; \
    }
cast_take_template(i8, int8_t, 8)
cast_take_template(i16, int16_t, 16)
cast_take_template(i32, int32_t, 32)
cast_take_template(i64, int64_t, 64)
cast_take_template(f32, float, 32)
cast_take_template(f64, double, 64)
cast_take_template(ptr, void *, 64)

#define vm_errout(COND, ERR) \
    if (COND) { \
        *err = vm_error_init(state, ERR); \
        return false; \
    }

static inline bool run_op_load_data(vm_state_t *state, vm_error_t *err, bool size_class) {
    const uint32_t offs = size_class ? take_u32(state) : take_u8(state);
    const uint32_t n = size_class ? take_u32(state) : take_u8(state);
    vm_errout(offs + n > state->data_size, vm_err_segfault);
    vm_errout(state->sp - n < state->stack, vm_err_stack_overflow);
    state->sp -= n;
    memcpy(state->sp, state->data + offs, n);
    return true;
}
static inline bool run_op_store_data(vm_state_t *state, vm_error_t *err, bool size_class) {
    const uint32_t offs = size_class ? take_u32(state) : take_u8(state);
    const uint32_t n = size_class ? take_u32(state) : take_u8(state);
    vm_errout(offs + n > state->data_size, vm_err_segfault);
    vm_errout(state->sp + n > state->stack + state->stack_size, vm_err_stack_overrun);
    memcpy(state->data + offs, state->sp, n);
    return true;
}
static inline bool run_op_load_data_indirect(vm_state_t *state, vm_error_t *err, bool size_class) {
    const uint32_t n = size_class ? take_u32(state) : take_u8(state);
    uint32_t offs;
    vm_errout(!vm_state_pop_u32(state, &offs), vm_err_stack_underflow);
    vm_errout(state->sp - n < state->stack, vm_err_stack_overflow);
    vm_errout(offs + n > state->data_size, vm_err_segfault);
    state->sp -= n;
    memcpy(state->sp, state->data + offs, n);
    return true;
}
static inline bool run_op_store_data_indirect(vm_state_t *state, vm_error_t *err, bool size_class) {
    const uint32_t n = size_class ? take_u32(state) : take_u8(state);
    uint32_t offs;
    vm_errout(!vm_state_pop_u32(state, &offs), vm_err_stack_underflow);
    vm_errout(state->sp + n > state->stack + state->stack_size, vm_err_stack_overrun);
    vm_errout(offs + n > state->data_size, vm_err_segfault);
    memcpy(state->data + offs, state->sp, n);
    return true;
}
static inline bool run_op_load_indirect(vm_state_t *state, vm_error_t *err, bool size_class) {
    const uint32_t n = size_class ? take_u32(state) : take_u8(state);
    void *ptr;
    vm_errout(!vm_state_pop_ptr(state, &ptr), vm_err_stack_underflow);
    //vm_errout(state->sp + n > state->stack + state->stack_size, vm_err_stack_overrun);
    vm_errout(state->sp - n < state->stack, vm_err_stack_overflow);
    state->sp -= n;
    memcpy(state->sp, ptr, n);
    return true;
}
static inline bool run_op_store_indirect(vm_state_t *state, vm_error_t *err, bool size_class) {
    const uint32_t n = size_class ? take_u32(state) : take_u8(state);
    void *ptr;
    vm_errout(!vm_state_pop_ptr(state, &ptr), vm_err_stack_underflow);
    vm_errout(state->sp + n > state->stack + state->stack_size, vm_err_stack_overrun);
    memcpy(ptr, state->sp, n);
    return true;
}
static inline bool run_op_load_stack(vm_state_t *state, vm_error_t *err, bool size_class) {
    const uint32_t offs = size_class ? take_u32(state) : take_u8(state);
    const uint32_t n = size_class ? take_u32(state) : take_u8(state);
    vm_errout(state->sp - n < state->stack, vm_err_stack_overflow);
    vm_errout(offs + n > state->stack_size, vm_err_stack_overrun);
    state->sp -= n;
    memcpy(state->sp, state->stack + offs, n);
    return true;
}
static inline bool run_op_store_stack(vm_state_t *state, vm_error_t *err, bool size_class) {
    const uint32_t n = size_class ? take_u32(state) : take_u8(state);
    uint32_t offs;
    vm_errout(!vm_state_pop_u32(state, &offs), vm_err_stack_underflow);
    vm_errout(state->sp + n > state->stack + state->stack_size, vm_err_stack_overrun);
    vm_errout(offs + n > state->stack_size, vm_err_stack_overrun);
    memcpy(state->stack + offs, state->sp, n);
    return true;
}
static inline bool run_op_load_stack_indirect(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_store_stack_indirect(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_sub_stack(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_add_stack(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_add(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_sub(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_mul(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_div(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_and(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_or(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_xor(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_sll(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_srl(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_sra(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_extend(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_reduce(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_f2u(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_f2i(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_i2f(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_u2f(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_push_eq(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_push_ne(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_push_gt(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_push_lt(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_push_ge(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_push_le(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_ret(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_call(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_call_indirect(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_extern_call(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_extern_call_indirect(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_jump(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_bne(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_beq(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_imm_i8(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_imm_i16(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_imm_i32(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_imm_i64(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_imm_u8(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_imm_u16(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_imm_u32(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_imm_u64(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_imm_f32(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_imm_f64(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_imm_ptr(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_alloc_indirect(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_op_free_indirect(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}
static inline bool run_vm_opcode_max(vm_state_t *state, vm_error_t *err, bool flag) {
    return false;
}


static bool vm_state_run_once(vm_state_t *state, vm_error_t *err) {
    vm_errout(state->pc > state->code + state->code_size, vm_err_code_segfault);
    const uint8_t byte = *state->pc++;
    const bool flag = byte & 0x80;

    switch ((vm_opcode_t)(byte & 0x7f)) {
    case op_load_data: return run_op_load_data(state, err, flag);
    case op_store_data: return run_op_store_data(state, err, flag);
    case op_load_data_indirect: return run_op_load_data_indirect(state, err, flag);
    case op_store_data_indirect: return run_op_store_data_indirect(state, err, flag);
    case op_load_indirect: return run_op_load_indirect(state, err, flag);
    case op_store_indirect: return run_op_store_indirect(state, err, flag);
    case op_load_stack: return run_op_load_stack(state, err, flag);
    case op_store_stack: return run_op_store_stack(state, err, flag);
    case op_load_stack_indirect: return run_op_load_stack_indirect(state, err, flag);
    case op_store_stack_indirect: return run_op_store_stack_indirect(state, err, flag);
    case op_sub_stack: return run_op_sub_stack(state, err, flag);
    case op_add_stack: return run_op_add_stack(state, err, flag);
    case op_add: return run_op_add(state, err, flag);
    case op_sub: return run_op_sub(state, err, flag);
    case op_mul: return run_op_mul(state, err, flag);
    case op_div: return run_op_div(state, err, flag);
    case op_and: return run_op_and(state, err, flag);
    case op_or: return run_op_or(state, err, flag);
    case op_xor: return run_op_xor(state, err, flag);
    case op_sll: return run_op_sll(state, err, flag);
    case op_srl: return run_op_srl(state, err, flag);
    case op_sra: return run_op_sra(state, err, flag);
    case op_extend: return run_op_extend(state, err, flag);
    case op_reduce: return run_op_reduce(state, err, flag);
    case op_f2u: return run_op_f2u(state, err, flag);
    case op_f2i: return run_op_f2i(state, err, flag);
    case op_i2f: return run_op_i2f(state, err, flag);
    case op_u2f: return run_op_u2f(state, err, flag);
    case op_push_eq: return run_op_push_eq(state, err, flag);
    case op_push_ne: return run_op_push_ne(state, err, flag);
    case op_push_gt: return run_op_push_gt(state, err, flag);
    case op_push_lt: return run_op_push_lt(state, err, flag);
    case op_push_ge: return run_op_push_ge(state, err, flag);
    case op_push_le: return run_op_push_le(state, err, flag);
    case op_ret: return run_op_ret(state, err, flag);
    case op_call: return run_op_call(state, err, flag);
    case op_call_indirect: return run_op_call_indirect(state, err, flag);
    case op_extern_call: return run_op_extern_call(state, err, flag);
    case op_extern_call_indirect: return run_op_extern_call_indirect(state, err, flag);
    case op_jump: return run_op_jump(state, err, flag);
    case op_bne: return run_op_bne(state, err, flag);
    case op_beq: return run_op_beq(state, err, flag);
    case op_imm_i8: return run_op_imm_i8(state, err, flag);
    case op_imm_i16: return run_op_imm_i16(state, err, flag);
    case op_imm_i32: return run_op_imm_i32(state, err, flag);
    case op_imm_i64: return run_op_imm_i64(state, err, flag);
    case op_imm_u8: return run_op_imm_u8(state, err, flag);
    case op_imm_u16: return run_op_imm_u16(state, err, flag);
    case op_imm_u32: return run_op_imm_u32(state, err, flag);
    case op_imm_u64: return run_op_imm_u64(state, err, flag);
    case op_imm_f32: return run_op_imm_f32(state, err, flag);
    case op_imm_f64: return run_op_imm_f64(state, err, flag);
    case op_imm_ptr: return run_op_imm_ptr(state, err, flag);
    case op_alloc_indirect: return run_op_alloc_indirect(state, err, flag);
    case op_free_indirect: return run_op_free_indirect(state, err, flag);
    default:
        *err = vm_error_init(state, vm_err_illegal_instr);
        return false;
        break;
    }

    return true;
}
vm_error_t vm_state_run(vm_state_t *state, uint32_t offs) {
    state->pc = state->code + offs;
    state->sp = state->stack + state->stack_size;
    state->rp = state->callstack;

    vm_error_t err;
    while (vm_state_run_once(state, &err));
    return err;
}
#define vm_push_template(NAME, TYPE) \
    bool vm_state_push_##NAME(vm_state_t *state, TYPE x) { \
        if (state->sp - sizeof(x) < state->stack) return false; \
        state->sp -= ((uintptr_t)state->sp & (alignof(TYPE) - 1)) + sizeof(x); \
        *(TYPE *)state->sp = x; \
        return true; \
    }
vm_push_template(i8, int8_t);
vm_push_template(i16, int16_t);
vm_push_template(i32, int32_t);
vm_push_template(i64, int64_t);
vm_push_template(u8, uint8_t);
vm_push_template(u16, uint16_t);
vm_push_template(u32, uint32_t);
vm_push_template(u64, uint64_t);
vm_push_template(ptr, void *);
vm_push_template(f32, float);
vm_push_template(f64, double);
#define vm_pop_template(NAME, TYPE) \
    bool vm_state_pop_##NAME(vm_state_t *state, TYPE *x) { \
        if (state->sp + sizeof(*x) > state->stack + state->stack_size) return false; \
        *x = *(TYPE *)state->sp; \
        state->sp += sizeof(*x); \
        return true; \
    }
vm_pop_template(i8, int8_t)
vm_pop_template(i16, int16_t)
vm_pop_template(i32, int32_t)
vm_pop_template(i64, int64_t)
vm_pop_template(u8, uint8_t)
vm_pop_template(u16, uint16_t)
vm_pop_template(u32, uint32_t)
vm_pop_template(u64, uint64_t)
vm_pop_template(ptr, void *)
vm_pop_template(f32, float)
vm_pop_template(f64, double)

#define X(ENUM) case ENUM: return #ENUM;
const char *vm_error_code_to_str(vm_error_code_t code) {
    switch (code) {
    VM_ERROR_CODES
    }
}
#undef X
void vm_error_log(const vm_error_t *err, FILE *out) {
    fprintf(out, "vm_error_t {\n");
    fprintf(out, "\tprogram_counter: %ld,\n", err->program_counter);
    fprintf(out, "\tstack_pointer: %p,\n", err->stack_pointer);
    fprintf(out, "\tstack_size: %zu,\n", err->stack_size);
    fprintf(out, "}\n");

}
