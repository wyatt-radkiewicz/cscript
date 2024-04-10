#include <stdio.h>
#include <stdlib.h>

#include "parser.h"
#include "compile.h"
#include "vm.h"

#define ARRSZ(A) (sizeof(A)/sizeof((A)[0]))

// Testing function
char *load_file(const char *file)
{
	long len;
	FILE *fp;
	char *str;

	fp = fopen(file, "r");
	if (!fp) return NULL;
	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	str = malloc(len + 1);
	if (fread(str, 1, len, fp) != len)
	{
		free(str);
		fclose(fp);
		return NULL;
	}
	str[len] = '\0';
	fclose(fp);

	return str;
}

struct vm_ptr *vm_alloc(int size)
{
	struct vm_ptr *ptr = malloc(size * sizeof(union vm_untyped_var) + sizeof(struct vm_ptr));
	ptr->var = (union vm_untyped_var *)(ptr + 1);
	ptr->size = size;
	ptr->nrefs = 1;
	return ptr;
}
void vm_free(struct vm_ptr *var)
{
	free(var);
}

void vm_print_opcode(struct vm_code code) {
	switch (code.op) {
	case OP_PUSH0: printf("OP_PUSH0"); break;
	case OP_PUSHNI: printf("OP_PUSHNI"); break;
	case OP_POPN: printf("OP_POPN"); break;
	case OP_REPUSH: printf("OP_REPUSH"); break;
	case OP_TRANSFER: printf("OP_TRANSFER"); break;
	case OP_NEWRC: printf("OP_NEWRC"); break;
	case OP_ADDRC: printf("OP_ADDRC"); break;
	case OP_DECRC: printf("OP_DECRC"); break;
	case OP_ADD: printf("OP_ADD"); break;
	case OP_SUB: printf("OP_SUB"); break;
	case OP_MUL: printf("OP_MUL"); break;
	case OP_DIV: printf("OP_DIV"); break;
	case OP_STORE: printf("OP_STORE"); break;
	case OP_LOAD: printf("OP_LOAD"); break;
	case OP_STOREPTR: printf("OP_STOREPTR"); break;
	case OP_LOADPTR: printf("OP_LOADPTR"); break;
	case OP_STORECPTR: printf("OP_STORECPTR"); break;
	case OP_LOADCPTR: printf("OP_LOADCPTR"); break;
	case OP_CALL: printf("OP_CALL"); break;
	case OP_EXTERN_CALL: printf("OP_EXTERN_CALL"); break;
	case OP_RET: printf("OP_RET"); break;
	case OP_JMP: printf("OP_JMP"); break;
	case OP_BEQ: printf("OP_BEQ"); break;
	case OP_BNE: printf("OP_BNE"); break;
	case OP_BGT: printf("OP_BGT"); break;
	case OP_BLT: printf("OP_BLT"); break;
	case OP_BGE: printf("OP_BGE"); break;
	case OP_BLE: printf("OP_BLE"); break;
	}
	printf(": %d\n", code.arg);
}

void vm_print_err(enum vm_error err)
{
	switch (err)
	{
	case VM_ERR_OKAY:
		printf("errcode: OKAY\n");
		break;
	case VM_ERR_SEGFAULT:
		printf("errcode: SEGFAULT\n");
		break;
	case VM_ERR_UNKNOWN_FUNC:
		printf("errcode: UNKNOWN FUNC\n");
		break;
	case VM_ERR_STACK_OVERFLOW:
		printf("errcode: STACK OVERFLOW\n");
		break;
	case VM_ERR_STACK_UNDERFLOW:
		printf("errcode: STACK UNDERFLOW\n");
		break;
	}
}
void vm_print_topvar(struct vm *vm)
{
	switch (vm->stack[0].type)
	{
	case VAR_INT:
		printf("top: int -> %d\n", vm->stack[0].data.i);
		break;
	case VAR_UINT:
		printf("top: uint -> %u\n", vm->stack[0].data.u);
		break;
	case VAR_CHAR:
		printf("top: char -> %c\n", vm->stack[0].data.c);
		break;
	case VAR_FLOAT:
		printf("top: float -> %f\n", vm->stack[0].data.f);
		break;
	case VAR_PTR:
		printf("top: ptr -> %p\n", vm->stack[0].data.p);
		break;
	case VAR_CPTR:
		printf("top: cptr -> %p\n", vm->stack[0].data.p);
		break;
	case VAR_REF:
		printf("top: ref -> %p\n", vm->stack[0].data.p);
		break;
	case VAR_PFN:
		printf("top: pfn -> %p\n", vm->stack[0].data.p);
		break;
	}
	vm_push(vm, (struct vm_typed_var){
		.type = VAR_INT,
		.data.i = 69 + vm->stack[0].data.i,
	});
}

static struct ast_node ast_buffer[512];

static struct vm_fn_entry fns[4];
static struct vm_code code[64];
static union vm_untyped_var data[512];

static struct vm_typed_var stack[128];

int main(int argc, char **argv)
{
	struct vm vm;
	char *src = load_file("test.bs");
	struct ast_node *root = ast_construct(src, ast_buffer, 512, NULL, 0);
	free(src);
	vm_init(
		&vm,		// vm
		code,		// code
		ARRSZ(code),	// codelen
		vm_alloc,	// alloc
		vm_free,	// free
		data,		// data
		ARRSZ(data),	// datalen
		stack,		// stack
		ARRSZ(stack),	// stacklen
		fns,		// funcs
		ARRSZ(fns)	// funcs_len
	);

	struct compile_error errs[32];
	struct state compiler;
	compile_init(&compiler, root, &vm, errs, ARRSZ(errs));
	compile_extern_fn(&compiler, "print_int", vm_print_topvar);
	code[ARRSZ(code)-1].op = OP_RET;
	compile(&compiler);
	for (int i = 0; i < ARRSZ(code); i++) {
		vm_print_opcode(code[i]);
	}
	vm_push(&vm, (struct vm_typed_var){ .type = VAR_INT });
	vm_push(&vm, (struct vm_typed_var){ .type = VAR_INT });
	vm_print_err(vm_callfn(&vm, "main"));

	return 0;
}

