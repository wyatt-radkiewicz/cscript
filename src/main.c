#include <stdio.h>

#include "common.h"
#include "vm.h"

uint8_t code[32];
uint8_t data[32];
uint8_t stack[64];
vm_callstack_t callstack[8];
vm_extern_fn_t pfn[8];

int main(int argc, char **argv) {
    vm_state_t vm = {
        .code = code,
        .code_size = arrsz(code),
        .data = data,
        .data_size = arrsz(code),
        .stack = stack,
        .stack_size = arrsz(code),
        .callstack = callstack,
        .callstack_size = arrsz(code),
        .pfn = pfn,
        .pfn_size = arrsz(pfn),
    };
    {
        uint8_t *head = code;
        emit_op_imm_i32(&head, 50);
        emit_op_imm_i32(&head, 19);
        emit_op_add(&head, false, false, false);
        emit_op_imm_i32(&head, 0);
        emit_op_push_eq(&head, false, false);
        emit_op_beq(&head, 4, false);
        emit_op_imm_i32(&head, 420);
        emit_op_add(&head, false, false, false);
        emit_op_ret(&head);
    }
    for (const uint8_t *head = code;;) {
        if (head >= code + arrsz(code)) break;
        printf("%4zu: ", head - code);
        if (vm_opcode_log(&head, stdout) == op_illegal) break;
        printf("\n");
    }
    vm_error_log(vm_state_run(&vm, 0), stdout);

    return 0;
}
