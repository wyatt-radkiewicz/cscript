#include <stdio.h>
#include <string.h>

#include "vm.h"

static int vm_getfn(struct vm *vm, const char *fn);

void vm_init(struct vm *vm,
		struct vm_code *code,
		vm_heap_alloc alloc,
		vm_heap_free free,
		union vm_untyped_var *data,
		struct vm_typed_var *stack,
		int stacklen,
		struct vm_fn_entry *fn,
		int fnlen)
{
	vm->pc = 0;
	vm->code = code;
	vm->data = data;
	vm->stack = stack;
	vm->alloc = alloc;
	vm->free = free;
	vm->fn = fn;
	vm->fnlen = fnlen;
	vm->nfns = 0;
	memset(vm->fn, 0, sizeof(*vm->fn) * vm->fnlen);
}
void vm_callfn(struct vm *vm, const char *fn)
{
	struct vm_typed_var tmp[2];
	struct vm_code code;
	int fnid;

	fnid = vm_getfn(vm, fn);
	if (fnid == -1) return;
	vm_push(vm, (struct vm_typed_var){ .type = VAR_INT, .data.i = -1 });
	vm->pc = vm->fn[fnid].loc;

	while (1)
	{
	code = vm->code[vm->pc++];
	switch (code.op)
	{
	case OP_PUSH0:
		vm_push(vm, (struct vm_typed_var){ .type = code.arg, .data.p = NULL });
		break;
	case OP_PUSHNI:
		vm->stack -= code.arg;
		memset(vm->stack, 0, sizeof(struct vm_typed_var) * code.arg);
		break;
	case OP_POPN:
		vm->stack += code.arg;
		break;
	case OP_REPUSH:
		vm_push(vm, vm->stack[code.arg]);
		break;
	case OP_TRANSFER:
		vm->stack[code.arg] = vm->stack[0];
		break;
	case OP_NEWRC:
		vm_push(vm, (struct vm_typed_var){
			.type = VAR_REF,
			.data.p = vm->alloc(code.arg),
		});
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
		vm_push(vm, (struct vm_typed_var){
			.type = code.arg,
			.data = *(union vm_untyped_var *)(vm->stack[1].data.p)
		});
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
		vm_push(vm, (struct vm_typed_var){ .type = code.arg });
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
		vm_push(vm, (struct vm_typed_var){ .type = VAR_INT, .data.i = vm->pc });
		vm->pc = code.arg;
		break;
	case OP_EXTERN_CALL:
		tmp[0] = vm_pop(vm);
		((vm_extern_pfn)(tmp[0].data.p))(vm);
		break;
	case OP_RET:
		vm->pc = vm_pop(vm).data.i;
		if (vm->pc == -1) return;
		break;
	case OP_JMP:
		vm->pc = code.arg;
		break;
#define BRANCHOP(BNAME, OP)						\
	case BNAME:							\
		switch (vm->stack[0].type)				\
		{							\
		case VAR_INT:						\
			if (vm->stack[0].data.i OP vm->stack[1].data.i) vm->pc = code.arg;\
			break;						\
		case VAR_UINT:						\
			if (vm->stack[0].data.u OP vm->stack[1].data.u) vm->pc = code.arg;\
			break;						\
		case VAR_CHAR:						\
			if (vm->stack[0].data.c OP vm->stack[1].data.c) vm->pc = code.arg;\
			break;						\
		case VAR_FLOAT:						\
			if (vm->stack[0].data.f OP vm->stack[1].data.f) vm->pc = code.arg;\
			break;						\
		default: break;						\
		}							\
		break;							
	BRANCHOP(OP_BEQ, ==)
	BRANCHOP(OP_BNE, !=)
	BRANCHOP(OP_BGT, >)
	BRANCHOP(OP_BLT, <)
	BRANCHOP(OP_BGE, >=)
	BRANCHOP(OP_BLE, <=)
#define MATHOP(OPNAME, OP)						\
	case OPNAME:							\
		tmp[1] = vm_pop(vm);					\
		tmp[0] = vm_pop(vm);					\
		switch (vm->stack->type)				\
		{							\
		case VAR_INT:						\
		case VAR_UINT:						\
		case VAR_CHAR:						\
			vm_push(vm, (struct vm_typed_var){		\
				.type = tmp[0].type,			\
				.data.u = tmp[0].data.u OP tmp[1].data.u,\
			});						\
			break;						\
		case VAR_FLOAT:						\
			vm_push(vm, (struct vm_typed_var){		\
				.type = VAR_FLOAT,			\
				.data.f = tmp[0].data.f OP tmp[1].data.f,\
			});						\
			break;						\
		default:						\
			vm_push(vm, tmp[0]);				\
			break;						\
		}							\
		break;							
	MATHOP(OP_ADD, +)
	MATHOP(OP_SUB, -)
	MATHOP(OP_DIV, /)
	MATHOP(OP_MUL, *)
	}
	}
}
void vm_push(struct vm *vm, const struct vm_typed_var var)
{
	*(--vm->stack) = var;
}
struct vm_typed_var vm_pop(struct vm *vm)
{
	return *(vm->stack++);
}
int vm_addfn(struct vm *vm, const char *name, int loc)
{
	struct vm_fn_entry ent, tmp;
	int i;

	ent.len = strlen(name);
	if (vm->nfns >= vm->fnlen || ent.len + 1 > sizeof(ent.name)) return 0;

	strcpy(ent.name, name);
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

	printf("adding fn \"%s\" to: %d\n", name, i);
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

