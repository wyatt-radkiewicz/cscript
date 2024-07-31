//
// CNM Script by __ e k l 1 p 3 d __
//
// CNM script is a subset of C with refrences/slices added. Some features
// removed from C (to help keep the language safe) include:
// - no pointer arithmetic
// - can not assign pointer to another pointer
// - can not cast pointers
// - can set pointers through external C functions and structs retruned from
//   them
// - union members can not be accessed
// On the other hand here are language additions:
// - refrences with built in length (so they double up as slices)
// - void refrences that also have built in type information
//
// Even though this might seem like a lot was removed from C this was done in
// the name of simplicity and safety. The only things that can break a C
// program that embeds CNM script are the functions exposed to CNM script
// itself. Most C functions should be straight ports, but due to this extra
// layer of safety unfortunatly some hooks/wrapper functions may have to be
// created for full effectiveness of cnm script.
//
#ifndef _cnm_h_
#define _cnm_h_

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>

// Used by the cnm macro
#define cnmstr(x) #x
#define cnmxstr(x) cnmstr(x)

// Use this to refrence the source string of declarations you wrapped in cnm()
#define cnmsymb(symbol_name) cnm_csrc_##symbol_name

// With the cnm macro you can automatically create the stringized version of
// the source code that you can parse for cscript. Note that cscript is a
// set of C where most but not all features overlap, so not all C
// functionality might work.
#define cnm(symbol_name, ...) \
    __VA_ARGS__ \
    const char *const cnmsymb(symbol_name) = cnmxstr(__VA_ARGS__);

// TODO: Add branches for other cpus
#if ULONG_MAX == 4294967295
#define cnmcall __attribute__((cdecl))
#else
#define cnmcall
#endif

// Internal cnm refrence structure. Since this can't hold info on its inner
// type, it can not be used in conjunction with the cnm macro by itself but it
// can be used in the cnm macro if you use a typedef or struct with same
// layout with appended information in this format:
//
// if returning a refrence to an int you would change:
// > cnmref_t get_int_array(void);
// to something like:
// > cnmref_int_t get_int_array(void);
// or any other type you want:
// > cnmref_struct_hash_entry_t get_hash_entry(int x);
//
// It can only handle 1 level of inderection. Anything above that will have to
// be a normal cnmref_t and then also have seperate script written instead of
// using the cnm macro.
typedef struct cnmref_s {
    void *ptr;
    size_t len;
} cnmref_t;

// Since you can not use arithmetic on pointers or cast them in cscript, you
// have to use any refrences to acheive type generic code. Any refrences can
// only point to concrete types like structs or POD data. You can get type ids
// from the main cnm state info.
typedef struct cnmanyref_s {
    cnmref_t ref;
    int type;
} cnmanyref_t;

// Main CNM state information. This must be present as long as you want to be
// able to get rtti or use debug features. If you don't need those at the time
// of running the code, you can actually free the memory used by the CNM state.
typedef struct cnm_s cnm_t;

// Pointers to user defined types
typedef struct cnm_struct_s cnm_struct_t;
typedef struct cnm_enum_s cnm_enum_t;
typedef struct cnm_fn_s cnm_fn_t;

// Called when an error occurred in cnm
typedef void(cnmcall *cnm_err_cb_t)(int line, const char *verbose, const char *simple);

// Called when a breakpoint is hit or you stepped
typedef void(cnmcall *cnm_dbg_cb_t)(cnm_t *cnm, int line);

// Called when an external function is parsed. If NULL is returned, the state
// will error out.
typedef void *(*cnm_fnaddr_cb_t)(cnm_t *cnm, const char *fn);

// Initiailze CNM state
cnm_t *cnm_init(void *region, size_t regionsz, void *code, size_t codesz);

// Will copy type definitions and function declarations from one instance to
// another (already initialized but with no compiled code) cnm state. Returns
// false if there is not enough space or the to state has already started
// compiling code. It will also override any already existing types in the to
// cnm state object.
bool cnm_copy(cnm_t *to, const cnm_t *from);

// Adds typedefs from stdint.h into the cnm state
bool cnm_add_stdint_h(cnm_t *cnm);

// Adds select functions from string.h into the cnm state
bool cnm_add_string_h(cnm_t *cnm);

// Adds declarations from stddef.h into the cnm state
bool cnm_add_stddef_h(cnm_t *cnm);

// Adds functions from ctype.h to the cnm state
bool cnm_add_ctype_h(cnm_t *cnm);

// Sets callback so user can receive errors
void cnm_set_errcb(cnm_t *cnm, cnm_err_cb_t errcb);

// Sets callback for when breakpoints are hit or hit the next line of code.
void cnm_set_dbgcb(cnm_t *cnm, cnm_dbg_cb_t errcb);

// Sets callback for when an external function is parsed in cnm script
void cnm_set_fnaddrcb(cnm_t *cnm, cnm_fnaddr_cb_t fnaddrcb);

// Returns false if debug mode is changed after the first part of compiled code.
bool cnm_set_debug(cnm_t *cnm, bool debug_mode);

// Returns false if debug mode was not enabled before compilation.
// If set to true, it will insert calls to the debug callback after every line
// in the source code.
bool cnm_set_stepmode(cnm_t *cnm, bool step_mode);

// Returns false if it was changed after compilation began.
// If set to true, it will insert detailed error messages when NULL is
// derefrenced or slices are accessed out of bounds.
// Detailed error messages include the line and 'file' where the
// error occurred.
bool cnm_set_rterr_detail(cnm_t *cnm, bool detailed);

// If the new type id can not be set because there is already a type occupying
// that id or if the new id is out of bounds, it will return false. If it
// succeeded it will return true.
bool cnm_set_structid(cnm_t *cnm, int old_type_id, int new_type_id);

// Returns false when compilation or parsing failed
bool cnm_parse(cnm_t *cnm, const char *src, const char *fname, const int *bpts, int nbpts);

// Can return NULL if the function at id is just declared or if id is out of
// bounds.
void *cnm_fn_addr(const cnm_fn_t *fn);

// Returns NULL if the function in question does not exist.
const cnm_fn_t *cnm_get_fn(cnm_t *cnm, const char *fn);

// These functions return a struct or enum if there is one by that name
// Id will return the type identifier of the struct
const cnm_struct_t *cnm_get_struct(cnm_t *cnm, const char *name);
cnm_enum_t *cnm_get_enum(cnm_t *cnm, const char *name);
int cnm_struct_get_id(const cnm_struct_t *s);
size_t cnm_struct_get_size(const cnm_struct_t *s);
int cnm_enum_get_id(const cnm_enum_t *e);
size_t cnm_enum_get_size(const cnm_enum_t *e);

// type can be char, uchar, bool, short, ushort, int, uint, long, ulong,
// llong, ullong, float, or double
int cnm_get_pod_id(const char *type);

// Returns the address of a global variable
void *cnm_get_global(cnm_t *cnm, const char *name);

#endif

