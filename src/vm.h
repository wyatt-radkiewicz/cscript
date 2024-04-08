#ifndef _vm_h_
#define _vm_h_

enum vm_varty
{
	VAR_INT,
	VAR_UINT,
	VAR_FLOAT,
	VAR_CHAR,
	VAR_PTR,
	VAR_CPTR,
	VAR_REF,
	VAR_PFN,
};

union vm_untyped_var
{
	int		i;
	unsigned int	u;
	float		f;
	char		c;
	void		*p;
};

struct vm_typed_var
{
	enum vm_varty type;
	union vm_untyped_var data;
};

struct vm_code
{
	unsigned int op		: 5;
	unsigned int arg	: (sizeof(int)*8)-5;
};

enum vm_opcode
{
	OP_PUSH0,		// Push [arg] type set to 0 to stack
	OP_PUSHNI,		// Push [arg] number of zero ints to stack
	OP_POPN,		// Pop [arg] number off the stack
	OP_REPUSH,		// Take [arg] from stack and copy it to top
	OP_TRANSFER,		// Take top of stack and copy it to [arg] of stack
	OP_NEWRC,		// New RC object of size [arg] (handle pushed to stack)
	OP_ADDRC,		// Add 1 refrence counted object at top
	OP_DECRC,		// Dec 1 from refrence counted object at top
	OP_ADD,			// Add the 2 top stack vars together
	OP_SUB,			// Subtract
	OP_MUL,			// Multiply
	OP_DIV,			// Divide
	OP_STORE,		// Store top of stack to [arg] in VM data
	OP_LOAD,		// Set top of stack data to VM data at [arg]
	OP_STOREPTR,		// Store top to RAM from 2nd to top ptr
	OP_LOADPTR,		// Push from RAM from top ptr (with type [arg])
	OP_STORECPTR,		// Store top to RAM from 2nd to top ptr
	OP_LOADCPTR,		// Push from RAM from top ptr (with type [arg])
	OP_CALL,		// Call VM function at [arg]
	OP_EXTERN_CALL,		// Call C function at stack top
	OP_RET,			// Return from function (top of stack is return)
	OP_JMP,			// Jump to [arg]
	OP_BEQ,			// Branch on EQual to [arg] (same below)
	OP_BNE,			// Branch on Not Equal
	OP_BGT,			// Branch on Greater Than (works for u an i)
	OP_BLT,			// Branch on Less Than (works for u an i)
	OP_BGE,			// Branch on Greater than or Equal to (both u i)
	OP_BLE,			// Branch on Less than or Equal to (both u i)
};

struct vm_fn_entry
{
	char name[32];
	int len;
	int psl;
	int loc;

	unsigned int hash;
};

struct vm_ptr
{
	union vm_untyped_var *var;
	int size;
	int nrefs;
};

typedef struct vm_ptr *(*vm_heap_alloc)(int size);
typedef void (*vm_heap_free)(struct vm_ptr *ptr);

struct vm
{
	int pc;

	struct vm_code *code;

	struct vm_typed_var *stack;
	union vm_untyped_var *data;
	vm_heap_alloc alloc;
	vm_heap_free free;

	struct vm_fn_entry *fn;
	int fnlen, nfns;
};

typedef void (*vm_extern_pfn)(struct vm *vm);

void vm_init(struct vm *vm,
		struct vm_code *code,
		vm_heap_alloc alloc,
		vm_heap_free free,
		union vm_untyped_var *data,
		struct vm_typed_var *stack,
		int stacklen,
		struct vm_fn_entry *fn,
		int fnlen);
void vm_callfn(struct vm *vm, const char *fn);
void vm_push(struct vm *vm, const struct vm_typed_var var);
struct vm_typed_var vm_pop(struct vm *vm);
int vm_addfn(struct vm *vm, const char *name, int loc);

#endif

