#include <stdlib.h>
#include <stdio.h>

#include "lexer.h"
#include "vm.h"
#include "ast.h"
#include "compiler.h"

// Testing function
static char *loadfile(const char *filepath) {
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

static void print_i32(vm_state_t *vm) {
    int32_t x = *(int32_t *)vm->sp;
    printf("print_i32: %d\n", x);
}
static void input_i32(vm_state_t *vm) {
    int32_t x;
    printf("input_i32: ");
    scanf("%d", &x);
    vm_state_push_i32(vm, x);
}

uint8_t stack[64];
vm_callstack_t callstack[8];

uint8_t code[128];
uint8_t data[128];
strview_t externfn_names[] = {
    (strview_t){ .str = "print_i32", .len = sizeof("print_i32")-1 },
    (strview_t){ .str = "input_i32", .len = sizeof("input_i32")-1 },
};
vm_extern_fn_t externfn_ptrs[] = {
    print_i32,
    input_i32,
};
strview_t internfn_names[32];
uint32_t internfn_locs[32];
comp_typebuf_t typebuf[64];
error_t errors[16];
comp_struct_t structs[32];
uint32_t typedefs[32];
comp_pfn_t pfns[32];
comp_fn_t fns[64];
comp_var_t scopevars[64];
comp_scope_t scopes[8];

ast_t ast[512];

int main(int argc, char **argv) {
    char *filestr = loadfile("test.bs");
    ast_state_t ast_state = {
        .lexer = lex_state_init((uint8_t *)filestr),
        .buf = ast,
        .buflen = arrsz(ast),
        .errbuf = errors,
        .errbuflen = arrsz(errors),
    };
    comp_resources_t res = {
        .code = code,
        .code_len = arrsz(code),

        .data = data,
        .data_len = arrsz(data),

        .ast = ast_build(&ast_state),

        .externfn_name = externfn_names,
        .externfn_ptr = externfn_ptrs,
        .externfn_len = arrsz(externfn_ptrs),

        .internfn_name = internfn_names,
        .internfn_loc = internfn_locs,
        .internfn_len = arrsz(internfn_names),

        .typebuf = typebuf,
        .typebuf_len = arrsz(typebuf),

        .error = errors,
        .error_len = arrsz(errors),
        .num_errors = ast_state.nerrs,

        .structs = structs,
        .structs_len = arrsz(structs),

        .typedefs = typedefs,
        .typedef_len = arrsz(typedefs),

        .pfns = pfns,
        .pfns_len = arrsz(pfns),

        .fns = fns,
        .fns_len = arrsz(fns),

        .scopevars = scopevars,
        .scopevars_len = arrsz(scopevars),

        .scopes = scopes,
        .scopes_len = arrsz(scopes),
    };
    //ast_log(res.ast, stdout);
    //printf("\n");

    if (!res.num_errors) compile(&res);
    for (int i = 0; i < res.num_errors; i++) {
        error_log(res.error + i, stdout);
        printf("\n");
    }
    
    vm_state_t vm = {
        .code = code,
        .code_size = arrsz(code),

        .data = data,
        .data_size = arrsz(data),

        .pfn = externfn_ptrs,
        .pfn_size = arrsz(externfn_ptrs),

        .callstack = callstack,
        .callstack_size = arrsz(callstack),
        
        .stack = stack,
        .stack_size = arrsz(stack),
    };
    //printf("loc: %d\n", res.internfn_loc[0]);
    for (const uint8_t *codeptr = code;
        codeptr - code < arrsz(code);) {
        printf("%04ld: ", codeptr - code);
        vm_opcode_log(&codeptr, stdout);
        printf("\n");
    }
    if (!res.num_errors) vm_error_log(vm_state_run(&vm, res.internfn_loc[1], true), stdout);

    free(filestr);
    return 0;
}
