#include <stdlib.h>
#include <stdio.h>

#include "lexer.h"
#include "vm.h"
#include "ast.h"

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

ast_t ast[512];
ast_error_t ast_errors[10];

int main(int argc, char **argv) {
    char *filestr = load_file("test.bs");
    ast_state_t ast_state = {
        .lexer = lex_state_init((uint8_t *)filestr),
        .buf = ast,
        .buflen = arrsz(ast),
        .errbuf = ast_errors,
        .errbuflen = arrsz(ast_errors),
    };
    ast_log(ast_build(&ast_state), stdout);
    printf("\n");
    for (int i = 0; i < ast_state.nerrs; i++) ast_error_log(ast_errors + i, stdout);
    free(filestr);

    return 0;
}
