#ifndef _vm_h_
#define _vm_h_

#include <stddef.h>

#include "opcode.h"

//
// Fill this struct out and then call run
//
typedef struct vm_state {
    uint8_t *pc, *sp;
    uint32_t *rp;

    uint8_t *code;
    size_t code_size;

    uint8_t *data;
    size_t data_size;

    uint8_t *stack;
    size_t stack_size;

    uint32_t *callstack;
    size_t callstack_size;
} vm_state_t;

//
// Virtual Machine Error Codes
//
#define VM_ERROR_CODES \
    X(vm_err_okay) /* Code ran without memory faults */ \
    X(vm_err_stack_overflow) \
    X(vm_err_stack_underflow) \
    X(vm_err_callstack_overflow) \
    X(vm_err_code_segfault) /* The code pointer is out of bounds */ \
    X(vm_err_segfault) /* Data segment was accessed out of bounds */ \
    X(vm_err_stack_overrun) /* Stack is accessed out of bounds */ \
    X(vm_err_illegal_instr)
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
    int64_t program_counter;
    uint8_t *stack_pointer;
    size_t stack_size, callstack_len;
} vm_error_t;

vm_error_t vm_state_run(vm_state_t *state, uint32_t offs);
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
void vm_error_log(const vm_error_t *err, FILE *out);
//#endif

#endif

