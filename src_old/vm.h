#ifndef _vm_h_
#define _vm_h_

#include <stddef.h>

#include "opcode.h"

typedef struct vm_state vm_state_t;
typedef struct vm_callstack {
    uint32_t ret_loc;
    uint32_t stack_base;
} vm_callstack_t;

typedef void *(*vm_alloc_t)(void *user, size_t sz);
typedef void (*vm_free_t)(void *user, void *ptr);
typedef void (*vm_extern_fn_t)(vm_state_t *state);

//
// Fill this struct out and then call run
// pc, sp, and rp don't need to be initialized by the user
//
struct vm_state {
    uint8_t *pc, *sp, *curr_instr;
    vm_callstack_t *rp;

    uint8_t *code;
    size_t code_size;

    uint8_t *data;
    size_t data_size;

    uint8_t *stack;
    size_t stack_size;

    vm_callstack_t *callstack;
    size_t callstack_size;

    vm_extern_fn_t *pfn;
    size_t pfn_size;

    void *alloc_user;
    vm_alloc_t alloc;
    vm_free_t free;
};

//
// Virtual Machine Error Codes
//
#define VM_ERROR_CODES \
    X(vm_err_okay) /* Code ran without memory faults */ \
    X(vm_err_stack_overflow) \
    X(vm_err_stack_underflow) \
    X(vm_err_callstack_overflow) \
    X(vm_err_callstack_underflow) \
    X(vm_err_code_segfault) /* The code pointer is out of bounds */ \
    X(vm_err_segfault) /* Data segment was accessed out of bounds */ \
    X(vm_err_stack_overrun) /* Stack is accessed out of bounds */ \
    X(vm_err_illegal_instr) \
    X(vm_err_invalid_pfn)
#define X(ENUM) ENUM,
typedef enum vm_error_code {
    VM_ERROR_CODES
} vm_error_code_t;
#undef X

//
// Captures the point at which the virual machine crashed and what happened
//
typedef struct vm_error {
    vm_error_code_t errcode;
    const uint8_t *instr_pointer;
    int64_t program_counter;
    size_t code_size;
    uint8_t *stack_pointer;
    size_t stack_size, callstack_len;
} vm_error_t;

vm_error_t vm_state_run(vm_state_t *state, uint32_t offs, bool reset);
// Push instructions will align the data to the alignment they are supposed to be
bool vm_state_push_i8(vm_state_t *state, int8_t x);
bool vm_state_push_i16(vm_state_t *state, int16_t x);
bool vm_state_push_i32(vm_state_t *state, int32_t x);
bool vm_state_push_i64(vm_state_t *state, int64_t x);
bool vm_state_push_u8(vm_state_t *state, uint8_t x);
bool vm_state_push_u16(vm_state_t *state, uint16_t x);
bool vm_state_push_u32(vm_state_t *state, uint32_t x);
bool vm_state_push_u64(vm_state_t *state, uint64_t x);
bool vm_state_push_ptr(vm_state_t *state, void *p);
bool vm_state_push_f32(vm_state_t *state, float x);
bool vm_state_push_f64(vm_state_t *state, double x);
// Does no alignment
bool vm_state_pop_i8(vm_state_t *state, int8_t *x);
bool vm_state_pop_i16(vm_state_t *state, int16_t *x);
bool vm_state_pop_i32(vm_state_t *state, int32_t *x);
bool vm_state_pop_i64(vm_state_t *state, int64_t *x);
bool vm_state_pop_u8(vm_state_t *state, uint8_t *x);
bool vm_state_pop_u16(vm_state_t *state, uint16_t *x);
bool vm_state_pop_u32(vm_state_t *state, uint32_t *x);
bool vm_state_pop_u64(vm_state_t *state, uint64_t *x);
bool vm_state_pop_ptr(vm_state_t *state, void **p);
bool vm_state_pop_f32(vm_state_t *state, float *x);
bool vm_state_pop_f64(vm_state_t *state, double *x);
//#ifdef DEBUG
#include <stdio.h>
void vm_error_log(const vm_error_t err, FILE *out);
vm_opcode_t vm_opcode_log(const uint8_t **code, FILE *out);
//#endif

#endif

