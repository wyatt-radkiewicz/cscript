#ifndef _compiler_h_
#define _compiler_h_

#include "common.h"
#include "vm.h"
#include "ast.h"

#define MAX_TYPE_NESTING 12
#define COMP_MAX_ALIGN 8

typedef enum comp_type_inf {
    type_i8,
    type_i16,
    type_i32,
    type_i64,
    type_u8,
    type_u16,
    type_u32,
    type_u64,
    type_f32,
    type_f64,
    type_c8,
    type_c16,
    type_c32,
    type_b8,
    type_u0,
    type_ptr,
    type_ref,
    type_arr,
    type_pfn,
    type_struct,
    type_typedef,
} comp_type_inf_t;

typedef enum comp_quals {
    qual_extern = 1 << 0,
    qual_const  = 1 << 1,
    qual_static = 1 << 2,
} comp_quals_t;

typedef struct comp_type_lvl {
    uint32_t quals  : 3;
    uint32_t type   : 5;
    uint32_t id     : (sizeof(uint16_t)*8)-5;
} comp_type_lvl_t;

typedef struct comp_type {
    comp_type_lvl_t lvls[MAX_TYPE_NESTING];
    uint32_t num_lvls;
} comp_type_t;

typedef struct comp_struct {
    strview_t name;

    bool defined;
    uint32_t type_loc, num_members;
    uint32_t align, size;
} comp_struct_t;

typedef struct comp_fn {
    strview_t name;

    bool is_extern;
    uint32_t loc;
    uint32_t types_loc, num_types;
} comp_fn_t;

typedef struct comp_typebuf {
    comp_type_t type;

    int32_t offs;
    strview_t name;
} comp_typebuf_t;

typedef struct comp_pfn {
    uint32_t types_loc, num_types;
} comp_pfn_t;

typedef enum comp_var_loc {
    var_loc_stack,
    var_loc_data,
    var_loc_ram,
} comp_var_loc_t;

typedef struct comp_var {
    comp_var_loc_t loctype;
    comp_type_t type;
    union {
        uint32_t loc;
        void *ptr;
    };

    strview_t name;
    bool lvalue, inscope;
    uint32_t scope_id;
} comp_var_t;

typedef struct comp_scope {
    // These bases are the tops of the stack and scope at the start of this scope
    uint32_t scope_base, stack_base;
} comp_scope_t;

// Resources needed by the compiler to compile the code
typedef struct comp_resources {
    uint8_t *code;
    size_t code_len;

    uint8_t *data;
    size_t data_len;

    // This should already by created by the time compilation starts
    const ast_t *ast;

    // These should also already be filled out by the time compilation starts
    const strview_t *externfn_name;
    const vm_extern_fn_t *externfn_ptr;
    size_t externfn_len;

    strview_t *internfn_name;
    uint32_t *internfn_loc;
    size_t infernfn_len;

    comp_typebuf_t *typebuf;
    size_t typebuf_len;

    error_t *error;
    size_t error_len, num_errors;

    comp_struct_t *structs;
    size_t structs_len;

    uint32_t *typedefs;
    size_t typedef_len;

    comp_pfn_t *pfns;
    size_t pfns_len;

    comp_fn_t *fns;
    size_t fns_len;

    comp_var_t *scopevars;
    size_t scopevars_len;

    comp_scope_t *scopes;
    size_t scopes_len;
} comp_resources_t;

void compile(comp_resources_t *res);

#endif

