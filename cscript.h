#ifndef _cscript_h_
#define _cscript_h_

#include <stdalign.h>
#include <stddef.h>

typedef struct csvm csvm_t;

#define CSSTACK_ALIGN (alignof(max_align_t))

typedef struct {
	alignas(max_align_t) unsigned char	raw[CSSTACK_ALIGN];
} csvm_ret_t;

//
// Called when an error occurred.
//
typedef void(*cs_error_writer_t)(const char *msg);

//
// Called when an public internal function is compiled.
// loc is the offset of the function's code
//
typedef void(*cs_func_callback_t)(const char *name, int loc);

//
// Functions that will be called from CScript
//
typedef void(*cs_extfunc_t)(csvm_t *vm);

//
// Returns the function pointer for a external function with the name name
//
typedef cs_extfunc_t(*cs_extfunc_callback_t)(const char *name);

//
// Compile code for the cscript interpreter.
//
int cs_compile(void *outbuf, size_t outlen,
	cs_error_writer_t writer,
	cs_func_callback_t funcs,
	cs_extfunc_callback_t extfuncs,
	const char *src);

typedef void *(cs_alloc_t)(void *user, size_t size);
typedef void (cs_free_t)(void *user, void *block);

//
// Runs the code starting at the code pointer.
// csvm_ret is just a buffer for the return type of the function (which can
// easily be cast pointer style to whatever type you need it as).
//
// Data can be persisted through multiple calls of cs_run by either the script
// editing globals found in the out buffer or dynamically allocated smart
// pointers in cscript.
//
csvm_ret_t cs_run(void *code,
	cs_alloc_t alloc,
	cs_free_t free,
	void *allocuser,
	void *args, size_t nargs);

// Use this inside of a function called from the virtual machine
csvm_ret_t cs_rerun(csvm_t *vm, void *code, void *args, size_t nargs);

// Get the current stack pointer (stack grows down)
void *cs_sp(csvm_t *vm);

#endif

