#include "comp_common.h"
#include "comptime.h"

// TODO: Actually comptime the globals
static void comp_globals(comp_state_t *state) {
    for (const ast_t *node = state->res->ast; node; node = node->next) {
        comp_update_line(state, node);
        comp_var_t var;
        if (node->type != ast_stmt_let) continue;
        if (node->a) {
            comp_type_t type;
            if (!comp_get_type(state, node->a, &type)) return;
            if (!comptime_typed_expr(state, true, &var, node->b, &type)) return;
        } else {
            if (!comptime_expr(state, true, &var, node->b, NULL)) return;
        }

        if (!comp_add_var_to_scope(state, &var, node->token.data.str)) return;
    }
}

static bool comp_get_typelvl(comp_state_t *state, const ast_t **node, comp_type_lvl_t *lvl) {
    *lvl = (comp_type_lvl_t){0};
    comp_update_line(state, *node);

    while (true) {
        switch ((*node)->type) {
        case ast_type_extern: lvl->quals |= qual_extern; break;
        case ast_type_const: lvl->quals |= qual_const; break;
        case ast_type_static: lvl->quals |= qual_static; break;
        default: goto skip_cvrs;
        }
        (*node) = (*node)->child;
        comp_update_line(state, *node);
    }

skip_cvrs:
    switch ((*node)->type) {
    case ast_type_pod: lvl->type = (*node)->token.type - tok_types_start; break;
    case ast_type_ptr: lvl->type = type_ptr; break;
    case ast_type_ref: lvl->type = type_ref; break;
    // TODO: Implement these
    case ast_type_ident:
        for (uint32_t i = 0; i < state->num_structs; i++) {
            if (strview_eq((*node)->token.data.str, state->res->structs[i].name)) {
                lvl->type = type_struct;
                lvl->id = i;
                goto found_ident;
            }
        }
        for (uint32_t i = 0; i < state->num_typedefs; i++) {
            if (strview_eq((*node)->token.data.str,
                state->res->typebuf[state->res->typedefs[i]].name)) {
                lvl->type = type_typedef;
                lvl->id = state->res->typedefs[i];
                goto found_ident;
            }
        }
        comp_error(state, "Use of undeclared identifier");
        return false;
    case ast_type_pfn: break;
    case ast_type_array: {
        lvl->type = type_arr;
        comp_var_t lenvar;
        if (!comptime_expr(state, true, &lenvar, (*node)->b, NULL)) return false;
        if (!comptime_cast(state, &lenvar, &(comp_type_t){
            .lvls[0] = (comp_type_lvl_t){ .type = type_u32 },
            .num_lvls = 1,
        })) {
            comp_error(state, "Array length should be an integer.");
            return false;
        }
        uint32_t len = comptime_pop_u32(state, lenvar.loc);
        if (len >= (uint32_t)1 << 24) {
            comp_error(state, "Array length too long (can not be represented in type)");
            return false;
        }
        lvl->id = len;
        *node = (*node)->a;
        comp_update_line(state, *node);
        return true;
        }
    default: return false; // Shouldn't be reachable
    }
found_ident:

    *node = (*node)->child;
    comp_update_line(state, *node);
    return true;
}

static bool comp_get_type(comp_state_t *state, const ast_t *node, comp_type_t *type) {
    *type = (comp_type_t){0};
    if (!node) return false;
    while (node) {
        comp_update_line(state, node);
        if (type->num_lvls == MAX_TYPE_NESTING) {
            comp_error(state, "Hit max type nesting level!");
            return false;
        }
        if (!comp_get_typelvl(state, &node, type->lvls + type->num_lvls++)) {
            comp_error(state, "Expected type.");
            return false;
        }
    }

    return true;
}

// Warning: Stack must be aligned to COMP_MAX_ALIGN before pushing arguments
// onto the stack and calling the function
static comp_fn_t *comp_new_func_sig(comp_state_t *state, const ast_t *fndef) {
    comp_update_line(state, fndef);
    if (!fndef || fndef->type != ast_def_func) {
        comp_error(state, "Expected a function signature.");
        return NULL;
    }
    if (state->num_fns == state->res->fns_len) {
        comp_error(state, "Ran out of function buffer room.");
        return NULL;
    }
    for (uint32_t i = 0; i < state->num_fns; i++) {
        if (strview_eq(fndef->token.data.str, state->res->fns[i].name)) {
            comp_error(state, "Function redefined");
            return NULL;
        }
    }
    comp_fn_t *fn = state->res->fns + state->res->fns_len++;
    *fn = (comp_fn_t){
        .name = fndef->token.data.str,
        .types_loc = state->num_typebufs,
    };
    
    size_t params_size = 0;
    for (const ast_t *param = fndef->a;
        param;
        param = param->next, fn->num_types++) {
        comp_typebuf_t *typebuf = comp_alloc_next_type(state);
        if (!typebuf) return NULL;
        if (!comp_get_type(state, param->child, &typebuf->type)) return NULL;
        typebuf->name = param->token.data.str;
        uint32_t align, size;
        if (!(align = comp_get_typealign(state, typebuf->type, 0))) return NULL;
        if (!comp_get_typesize(state, &size, typebuf->type, 0)) return NULL;
        if (params_size) params_size--;
        params_size = (params_size / align + 1) * align;
        typebuf->offs = params_size;
        params_size += size;
    }
    params_size = (params_size / COMP_MAX_ALIGN + 1) * COMP_MAX_ALIGN;

    for (uint32_t i = fn->types_loc; i < fn->types_loc + fn->num_types; i++) {
        comp_typebuf_t *const typebuf = state->res->typebuf + i;
        typebuf->offs -= params_size;
    }

    comp_typebuf_t *typebuf = comp_alloc_next_type(state);
    if (!typebuf) return NULL;
    if (!comp_get_type(state, fndef->b, &typebuf->type)) return NULL;
    typebuf->name = (strview_t){0};

    return fn;
}

static void comp_extern_fns(comp_state_t *state) {
    for (const ast_t *node = state->res->ast; node; node = node->next) {
        comp_update_line(state, node);
        if (node->type != ast_type_extern) continue;
        if (!node->child || node->child->type != ast_def_func) continue;
        comp_update_line(state, node->child);
        comp_fn_t *sig = comp_new_func_sig(state, node->child);
        if (!sig) {
            comp_error(state, "Couldn't compile extern function.");
            return;
        }
        sig->is_extern = true;
        
        uint32_t pfn_id = UINT32_MAX;
        for (uint32_t i = 0; i < state->res->externfn_len; i++) {
            if (!strview_eq(sig->name, state->res->externfn_name[i])) continue;
            pfn_id = i;
            break;
        }
        if (pfn_id == UINT32_MAX) {
            comp_error(state, "Extern function doesn't have pointer associated with it!");
            return;
        }

        sig->loc = pfn_id;
    }
}

static comp_struct_t *comp_new_struct(comp_state_t *state, const ast_t *node) {
    comp_update_line(state, node);
    if (!node || node->type != ast_def_struct) {
        comp_error(state, "Expected struct definition.");
        return NULL;
    }
    if (state->num_structs == state->res->structs_len) {
        comp_error(state, "Out of memory to store struct definitions.");
        return NULL;
    }
    comp_struct_t *strct = state->res->structs + state->num_structs++;
    *strct = (comp_struct_t){0};
    strct->name = node->token.data.str;

    for (const ast_t *node_mbr = node->child;
        node_mbr;
        node_mbr = node_mbr->next, strct->num_members++) {
        comp_update_line(state, node_mbr);
        comp_typebuf_t *typebuf = comp_alloc_next_type(state);
        if (!typebuf) return NULL;
        typebuf->name = node_mbr->token.data.str;
        if (!comp_get_type(state, node_mbr->child, &typebuf->type)) return NULL;

        uint32_t align;
        if (!(align = comp_get_typealign(state, typebuf->type, 0))) {
            comp_error(state, "Type doesn't have an alignment!");
            return 0;
        }
        if (align > strct->align) strct->align = align;
        
        uint32_t size;
        if (!comp_get_typesize(state, &size, typebuf->type, 0)) {
            return NULL;
        }

        if (strct->size) strct->size--;
        strct->size += (strct->size / strct->align + 1) * strct->align;
        typebuf->offs = strct->size;
        strct->size += size;
    }

    // Make the struct align with its largest member
    if (strct->size) strct->size--;
    strct->size += (strct->size / strct->align + 1) * strct->align;

    return strct;
}

static void comp_structs(comp_state_t *state) {
    for (const ast_t *node = state->res->ast; node; node = node->next) {
        comp_update_line(state, node);
        if (node->type != ast_def_struct) continue;
        comp_struct_t *strct = comp_new_struct(state, node);
        if (!strct) {
            comp_error(state, "Invalid struct definition");
            return;
        }

        for (uint32_t i = 0; i < state->num_structs - 1; i++) {
            if (strview_eq(state->res->structs[i].name, strct->name)) {
                comp_error(state, "Struct redefinition");
                return;
            }
        }
    }
}

void compile(comp_resources_t *res) {
    comp_state_t state = {
        .res = res,
        .scope_top = -1,
        .stack_top = 0,
    };
    if (res->scopes_len < 1) {
        comp_error(&state, "Need more scope buffer room to start compiling.");
        return;
    }
    res->scopes[0] = (comp_scope_t){
        .scope_base = state.scope_top,
        .stack_base = state.stack_top,
    };

    comp_structs(&state);
    comp_globals(&state);
    comp_extern_fns(&state);
}

