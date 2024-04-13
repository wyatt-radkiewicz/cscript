#include <stdio.h>

#include "common.h"
#include "vm.h"

uint8_t code[512];
uint8_t data[512];
uint8_t stack[512];
vm_callstack_t callstack[64];
vm_extern_fn_t pfn[32];

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
    for (const uint8_t *head = code;;) {
        if (head >= code + arrsz(code)) break;
        if (vm_opcode_log(&head, stdout) == op_illegal) break;
        printf("\n");
    }
    vm_error_log(vm_state_run(&vm, 0), stdout);

    return 0;
}
