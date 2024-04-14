#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "vm.h"
#include "lexer.h"

// Testing function
static char *load_file(const char *filepath) {
	FILE *file = fopen(filepath, "r");
	if (!file) return NULL;

	fseek(file, 0, SEEK_END);
	size_t len = ftell(file);
	fseek(file, 0, SEEK_SET);

	char *str = malloc(len + 1);
	if (fread(str, 1, len, file) != len) {
		free(str);
		fclose(file);
		return NULL;
	}
	str[len] = '\0';
	fclose(file);

	return str;
}

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

    char *filestr = load_file("test.bs");
    lex_state_t lexer = lex_state_init((uint8_t *)filestr);
    while (true) {
        if (!lex_state_next(&lexer).okay) break;
        lex_token_log(&lexer.tok, stdout);
        printf("\n");
        if (lexer.tok.type == tok_eof) break;
    }
    free(filestr);

    return 0;
}
