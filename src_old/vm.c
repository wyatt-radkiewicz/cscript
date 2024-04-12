#include <stdio.h>
#include <string.h>

#include "vm.h"

static int vm_getfn(struct vm *vm, const char *fn);

void vm_init(struct vm *vm,
		struct vm_code *code,
		int codelen,
		vm_heap_alloc alloc,
		vm_heap_free free,
		union vm_untyped_var *data,
		int datalen,
		struct vm_typed_var *stack,
		int stacklen,
		struct vm_fn_entry *fn,
		int fnlen)
{
	vm->pc = 0;
	vm->code = code;
	vm->codelen = codelen;
	vm->data = data;
	vm->stack = stack + stacklen - 1;
	vm->stacktop = vm->stack + 1;
	vm->stackbottom = stack - 1;
	vm->alloc = alloc;
	vm->free = free;
	vm->fn = fn;
	vm->fnlen = fnlen;
	vm->nfns = 0;
	memset(vm->fn, 0, sizeof(*vm->fn) * vm->fnlen);
}
#include <unistd.h>
int vm_callfn(struct vm *vm, const char *fn)
{
	struct vm_typed_var tmp[2];
	struct vm_code code;
	int fnid;

	fnid = vm_getfn(vm, fn);
	if (fnid == -1) return VM_ERR_UNKNOWN_FUNC;
	if (!vm_push(vm, (struct vm_typed_var){ .type = VAR_INT, .data.i = -1 })) return VM_ERR_STACK_OVERFLOW;
	vm->pc = vm->fn[fnid].loc;

	while (1)
	{
	//usleep(10000);
	//printf("pc: %d\n", vm->pc);
	code = vm->code[vm->pc++];
	switch (code.op)
	{
	case OP_PUSH0:
		if (!vm_push(vm, (struct vm_typed_var){ .type = code.arg, .data.p = NULL })) return VM_ERR_STACK_OVERFLOW;
		break;
	case OP_PUSHNI:
		vm->stack -= code.arg;
		memset(vm->stack, 0, sizeof(struct vm_typed_var) * code.arg);
		break;
	case OP_POPN:
		vm->stack += code.arg;
		break;
	case OP_REPUSH:
		if (!vm_push(vm, vm->stack[code.arg])) return VM_ERR_STACK_OVERFLOW;
		break;
	case OP_TRANSFER:
		vm->stack[code.arg] = vm->stack[0];
		break;
	case OP_NEWRC:
		if (!vm_push(vm, (struct vm_typed_var){
			.type = VAR_REF,
			.data.p = vm->alloc(code.arg),
		})) return VM_ERR_STACK_OVERFLOW;
		break;
	case OP_ADDRC:
		((struct vm_ptr *)vm->stack->data.p)->nrefs++;
		break;
	case OP_DECRC:
		{
			struct vm_ptr *ptr = (struct vm_ptr *)vm->stack->data.p;
			if (--ptr->nrefs) vm->free(ptr);
		}
		break;
	case OP_STORE:
		vm->data[code.arg] = vm->stack->data;
		break;
	case OP_LOAD:
		vm->stack[0].data = vm->data[code.arg];
		break;
	case OP_STOREPTR:
		*(union vm_untyped_var *)(vm->stack[1].data.p) = vm->stack->data;
		break;
	case OP_LOADPTR:
		if (!vm_push(vm, (struct vm_typed_var){
			.type = code.arg,
			.data = *(union vm_untyped_var *)(vm->stack[1].data.p)
		})) return VM_ERR_STACK_OVERFLOW;
		break;
	case OP_STORECPTR:
		switch (code.arg)
		{
		case VAR_CHAR:
			memcpy(vm->stack[1].data.p, &vm->stack[0].data, 1);
		case VAR_INT:
		case VAR_UINT:
		case VAR_FLOAT:
			memcpy(vm->stack[1].data.p, &vm->stack[0].data, sizeof(int));
		case VAR_REF:
		case VAR_PFN:
		case VAR_CPTR:
		case VAR_PTR:
			memcpy(vm->stack[1].data.p, &vm->stack[0].data, sizeof(void *));
		}
		break;
	case OP_LOADCPTR:
		if (!vm_push(vm, (struct vm_typed_var){ .type = code.arg })) return VM_ERR_STACK_OVERFLOW;
		switch (code.arg)
		{
		case VAR_CHAR:
			memcpy(&vm->stack[0].data, vm->stack[1].data.p, 1);
		case VAR_INT:
		case VAR_UINT:
		case VAR_FLOAT:
			memcpy(&vm->stack[0].data, vm->stack[1].data.p, sizeof(int));
		case VAR_REF:
		case VAR_PFN:
		case VAR_CPTR:
		case VAR_PTR:
			memcpy(&vm->stack[0].data, vm->stack[1].data.p, sizeof(void *));
		}
		break;
	case OP_CALL:
		if (!vm_push(vm, (struct vm_typed_var){ .type = VAR_INT, .data.i = vm->pc })) return VM_ERR_STACK_OVERFLOW;
		vm->pc = code.arg;
		break;
	case OP_EXTERN_CALL:
		if (!vm_pop(vm, tmp + 0)) return VM_ERR_STACK_UNDERFLOW;
		((vm_extern_pfn)(tmp[0].data.p))(vm);
		break;
	case OP_RET:
		if (vm->stack + code.arg >= vm->stacktop) return VM_ERR_STACK_UNDERFLOW;
		tmp[0] = *(vm->stack + code.arg);
		vm->pc = tmp[0].data.i;
		if (code.arg) {
			memmove(vm->stack + code.arg, vm->stack + code.arg - 1, sizeof(*vm->stack) * code.arg);
		}
		vm->stack++;
		if (vm->pc == -1) return VM_ERR_OKAY;
		if (vm->pc < -1 || vm->pc >= vm->codelen) return VM_ERR_SEGFAULT;
		break;
	case OP_JMP:
		vm->pc = code.arg;
		break;
	case OP_CAST:
		switch (vm->stack->type) {
		case VAR_INT:
			switch ((enum vm_varty)code.arg) {
			case VAR_FLOAT: vm->stack->data.f = vm->stack->data.i; break;
			case VAR_UINT: vm->stack->data.u = vm->stack->data.i; break;
			case VAR_CHAR: vm->stack->data.c = vm->stack->data.i; break;
			default: break;
			}
			break;
		case VAR_UINT:
			switch ((enum vm_varty)code.arg) {
			case VAR_FLOAT: vm->stack->data.f = vm->stack->data.u; break;
			case VAR_INT: vm->stack->data.i = vm->stack->data.u; break;
			case VAR_CHAR: vm->stack->data.c = vm->stack->data.u; break;
			default: break;
			}
			break;
		case VAR_FLOAT:
			switch ((enum vm_varty)code.arg) {
			case VAR_UINT: vm->stack->data.u = vm->stack->data.f; break;
			case VAR_INT: vm->stack->data.i = vm->stack->data.f; break;
			case VAR_CHAR: vm->stack->data.c = vm->stack->data.f; break;
			default: break;
			}
			break;
		case VAR_CHAR:
			switch ((enum vm_varty)code.arg) {
			case VAR_UINT: vm->stack->data.u = vm->stack->data.c; break;
			case VAR_INT: vm->stack->data.i = vm->stack->data.c; break;
			case VAR_FLOAT: vm->stack->data.f = vm->stack->data.c; break;
			default: break;
			}
			break;
		default: break;
		}
		vm->stack->type = code.arg;
		break;
#define BRANCHOP(BNAME, OP)						\
	case BNAME:							\
		if (!vm_pop(vm, tmp + 1)) return VM_ERR_STACK_UNDERFLOW;\
		if (!vm_pop(vm, tmp + 0)) return VM_ERR_STACK_UNDERFLOW;\
		vm_push(vm, (struct vm_typed_var){.type = VAR_INT});	\
		switch (tmp[0].type)					\
		{							\
		case VAR_INT:						\
			vm->stack->data.i = tmp[0].data.i OP tmp[1].data.i;\
			break;						\
		case VAR_UINT:						\
			vm->stack->data.i = tmp[0].data.u OP tmp[1].data.u;\
			break;						\
		case VAR_CHAR:						\
			vm->stack->data.i = tmp[0].data.c OP tmp[1].data.c;\
			break;						\
		case VAR_FLOAT:						\
			vm->stack->data.i = tmp[0].data.f OP tmp[1].data.f;\
			break;						\
		default: break;						\
		}							\
		break;							
	BRANCHOP(OP_TAND, &&)
	BRANCHOP(OP_TOR, ||)
	BRANCHOP(OP_TEQ, ==)
	BRANCHOP(OP_TNE, !=)
	BRANCHOP(OP_TGT, >)
	BRANCHOP(OP_TLT, <)
	BRANCHOP(OP_TGE, >=)
	BRANCHOP(OP_TLE, <=)
	case OP_BEQ:
		if (!vm->stack->data.u) vm->pc = code.arg;
		if (vm->stack++ < vm->stackbottom) return VM_ERR_STACK_UNDERFLOW;
		break;
	case OP_BNE:
		if (vm->stack->data.u) vm->pc = code.arg;
		if (vm->stack++ < vm->stackbottom) return VM_ERR_STACK_UNDERFLOW;
		break;
#define FLOATOP(OP)
#define MATHOP(OPNAME, OP)						\
	case OPNAME:							\
		if (!vm_pop(vm, tmp + 1)) return VM_ERR_STACK_UNDERFLOW;\
		if (!vm_pop(vm, tmp + 0)) return VM_ERR_STACK_UNDERFLOW;\
		switch (tmp[0].type)					\
		{							\
		case VAR_INT:						\
			if (!vm_push(vm, (struct vm_typed_var){		\
				.type = tmp[0].type,			\
				.data.i = tmp[0].data.i OP tmp[1].data.i,\
			})) return VM_ERR_STACK_OVERFLOW;		\
			break;						\
		case VAR_CHAR:						\
			if (!vm_push(vm, (struct vm_typed_var){		\
				.type = tmp[0].type,			\
				.data.c = tmp[0].data.c OP tmp[1].data.c,\
			})) return VM_ERR_STACK_OVERFLOW;		\
			break;						\
		case VAR_UINT:						\
			if (!vm_push(vm, (struct vm_typed_var){		\
				.type = tmp[0].type,			\
				.data.u = tmp[0].data.u OP tmp[1].data.u,\
			})) return VM_ERR_STACK_OVERFLOW;		\
			break;						\
			FLOATOP(OP)					\
		default:						\
			if (!vm_push(vm, tmp[0])) return VM_ERR_STACK_OVERFLOW;\
			break;						\
		}							\
		break;							
	MATHOP(OP_AND, &)
	MATHOP(OP_OR, |)
	MATHOP(OP_EOR, ^)
	MATHOP(OP_SHL, <<)
	MATHOP(OP_SHR, >>)
#undef FLOATOP
#define FLOATOP(OP)						\
		case VAR_FLOAT:						\
			if (!vm_push(vm, (struct vm_typed_var){		\
				.type = VAR_FLOAT,			\
				.data.f = tmp[0].data.f OP tmp[1].data.f,\
			})) return VM_ERR_STACK_OVERFLOW;		\
			break;						
	MATHOP(OP_ADD, +)
	MATHOP(OP_SUB, -)
	MATHOP(OP_DIV, /)
	MATHOP(OP_MUL, *)
	}
	}

	return VM_ERR_OKAY;
}
int vm_push(struct vm *vm, const struct vm_typed_var var)
{
	if (vm->stack - 1 <= vm->stackbottom) return 0;
	*(--vm->stack) = var;
	return 1;
}
int vm_pop(struct vm *vm, struct vm_typed_var *var)
{
	if (vm->stack >= vm->stacktop) return 0;
	*var = *(vm->stack++);
	return 1;
}
int vm_addfn(struct vm *vm, const char *name, int namelen, int loc)
{
	struct vm_fn_entry ent, tmp;
	int i;

	ent.len = namelen;
	if (vm->nfns >= vm->fnlen || ent.len + 1 > sizeof(ent.name)) return 0;

	memset(ent.name, 0, sizeof(ent.name));
	memcpy(ent.name, name, namelen < sizeof(ent.name) - 1 ? namelen : sizeof(ent.name) - 1);
	ent.loc = loc;
	ent.psl = 0;
	ent.hash = ent.len * name[0];
	if (ent.len >= 3) ent.hash ^= name[1] | name[2] << 8;
	i = ent.hash % vm->fnlen;
	
	while (vm->fn[i].len)
	{
		if (ent.psl > vm->fn[i].psl)
		{
			tmp = vm->fn[i];
			vm->fn[i] = ent;
			ent = tmp;
		}

		i = (i + 1) % vm->fnlen;
	}

	vm->fn[i] = ent;

	//printf("adding fn \"%s\" to: %d\n", name, i);
	return 1;
}
static int vm_getfn(struct vm *vm, const char *fn)
{
	int len, hash, i, psl;

	len = strlen(fn);
	if (len + 1 > sizeof(vm->fn[0].name) || !len) return -1;

	hash = len * fn[0];
	if (len >= 3) hash ^= fn[1] | fn[2] << 8;
	i = hash % vm->fnlen;
	psl = 0;

	while (psl <= vm->fn[i].psl && vm->fn[i].len)
	{
		if (len == vm->fn[i].len
			&& hash == vm->fn[i].hash
			&& strcmp(fn, vm->fn[i].name) == 0)
		{
			return i;
		}

		i = (i + 1) % vm->fnlen;
	}

	return -1;
}

