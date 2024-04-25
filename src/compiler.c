#include "comp_common.h"
#include "comptime.h"

static bool comp_check_code_size(comp_state_t *state) {
    if (state->code - state->res->code < state->res->code_len) return true;
    comp_error(state, "Ran out of code segment space!");
    return false;
}

static bool comp_global(comp_state_t *state, const ast_t *node) {
    comp_update_line(state, node);
    comp_var_t var;
    if (node->type != ast_stmt_let) return true;
    if (node->a) {
        comp_type_t type;
        if (!comp_get_type(state, node->a, &type)) return false;
        if (!comptime_typed_expr(state, true, &var, node->b, &type)) return false;
    } else {
        if (!comptime_expr(state, true, &var, node->b, NULL)) return false;
    }

    if (!comp_add_var_to_scope(state, &var, node->token.data.str)) return false;
    return true;
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
        uint32_t loc = state->dsz;
        if (!comptime_expr(state, true, &lenvar, (*node)->b, NULL)) return false;
        if (!comptime_cast(state, &lenvar, &(comp_type_t){
            .lvls[0] = (comp_type_lvl_t){ .type = type_u32 },
            .num_lvls = 1,
        }, loc)) {
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
    comp_fn_t *fn = state->res->fns + state->num_fns++;
    *fn = (comp_fn_t){
        .name = fndef->token.data.str,
        .types_loc = state->num_typebufs,
    };
    
    //size_t params_size = 0;
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
        fn->stack_base = alignu(fn->stack_base, align);
        typebuf->offs = fn->stack_base;
        fn->stack_base += size;
    }

    for (uint32_t i = fn->types_loc; i < fn->types_loc + fn->num_types; i++) {
        comp_typebuf_t *const typebuf = state->res->typebuf + i;
        uint32_t size;
        if (!comp_get_typesize(state, &size, typebuf->type, 0)) return NULL;
        typebuf->offs = fn->stack_base - (typebuf->offs + size);
    }
    fn->stack_base = -fn->stack_base; // Put it in the negatives

    comp_typebuf_t *typebuf = comp_alloc_next_type(state);
    if (!typebuf) return NULL;
    if (!comp_get_type(state, fndef->b, &typebuf->type)) return NULL;
    typebuf->name = (strview_t){0};

    return fn;
}

static bool comp_extern_fn(comp_state_t *state, const ast_t *node) {
    comp_update_line(state, node);
    if (node->type != ast_type_extern) return true;
    if (!node->child || node->child->type != ast_def_func) return true;
    comp_update_line(state, node->child);
    comp_fn_t *sig = comp_new_func_sig(state, node->child);
    if (!sig) {
        comp_error(state, "Couldn't compile extern function.");
        return false;
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
        return false;
    }

    sig->loc = pfn_id;
    return true;
}

static comp_struct_t *comp_struct(comp_state_t *state, const ast_t *node) {
    comp_update_line(state, node);
    if (!node || node->type != ast_def_struct) return NULL;
    if (state->num_structs == state->res->structs_len) {
        comp_error(state, "Out of memory to store struct definitions.");
        return NULL;
    }
    comp_struct_t *strct = state->res->structs + state->num_structs;
    *strct = (comp_struct_t){.type_loc = state->num_typebufs};
    strct->name = node->token.data.str;

    for (uint32_t i = 0; i < state->num_structs; i++) {
        if (strview_eq(state->res->structs[i].name, strct->name)) {
            if (state->res->structs[i].defined) {
                comp_error(state, "Struct redefinition");
                return NULL;
            } else {
                strct = state->res->structs + i;
                goto only_defin;
            }
        }
    }
    state->num_structs++;
    if (!node->child) {
        strct->type_loc = UINT32_MAX;
        strct->num_members = 0;
        return strct;
    }

only_defin:
    strct->defined = true;
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

        strct->size = alignu(strct->size, align);
        typebuf->offs = strct->size;
        strct->size += size;
    }

    // Make the struct align with its largest member
    strct->size = alignu(strct->size, strct->align);

    return strct;
}

static bool comp_push_literal_type(comp_state_t *state, const ast_t *literal, comp_var_t *var) {
    comp_update_line(state, literal);
    if (literal->type != ast_literal) {
        comp_error(state, "Expected literal!");
        return false;
    }
    comp_scope_t *scope = state->res->scopes + state->scopes_top;

    *var = (comp_var_t){
        .type.num_lvls = 1,
        .loctype = var_loc_stack,
        .lvalue = false,
        .inscope = false,
        .scope_id = UINT32_MAX,
    };

    const lex_token_t *tok = &literal->token;
    if (!comp_get_literal_type(state, literal, &var->type)) return false;
    switch (var->type.lvls[0].type) {
    case type_i32: emit_op_imm_i32(&state->code, &scope->stack_base, tok->data.u64); break;
    case type_u32: emit_op_imm_u32(&state->code, &scope->stack_base, tok->data.u64); break;
    case type_i64: emit_op_imm_i64(&state->code, &scope->stack_base, tok->data.u64); break;
    case type_u64: emit_op_imm_u64(&state->code, &scope->stack_base, tok->data.u64); break;
    case type_f64: emit_op_imm_f64(&state->code, &scope->stack_base, tok->data.f64); break;
    case type_c8: emit_op_imm_i8(&state->code, &scope->stack_base, tok->data.chr.c32); break;
    case type_c16: emit_op_imm_i16(&state->code, &scope->stack_base, tok->data.chr.c32); break;
    case type_c32: emit_op_imm_i32(&state->code, &scope->stack_base, tok->data.chr.c32); break;
    case type_b8: emit_op_imm_u8(&state->code, &scope->stack_base, tok->type == tok_true); break;
    default: comp_error(state, "Unknown literal type!"); return false;
    }
    var->loc = scope->stack_base;
    return comp_check_code_size(state);
}

// Will copy the variable to the top of the stack.
// If the variable is already on the top of the stack, it does nothing
static bool comp_get_by_value(comp_state_t *state,
                              comp_var_t *copy,
                              const comp_var_t *var) {
    comp_scope_t *scope = state->res->scopes + state->scopes_top;
    if (var->loctype == var_loc_stack
        && var->loc == scope->stack_base
        && !var->lvalue) return true;
    uint32_t varsize;
    if (!comp_get_typesize(state, &varsize, var->type, 0)) return false;
    switch (var->loctype) {
    case var_loc_stack:
        emit_op_load_stack(&state->code, &scope->stack_base, var->loc - scope->stack_base, varsize);
        *copy = *var;
        copy->loc = scope->stack_base;
        copy->lvalue = false;
        copy->inscope = false;
        copy->scope_id = UINT32_MAX;
        break;
    case var_loc_data:
        emit_op_load_data(&state->code, &scope->stack_base, var->loc, varsize);
        *copy = *var;
        copy->loc = scope->stack_base;
        copy->loctype = var_loc_stack;
        copy->lvalue = false;
        copy->inscope = false;
        copy->scope_id = UINT32_MAX;
        break;
    case var_loc_ram:
        emit_op_imm_ptr(&state->code, &scope->stack_base, var->ptr);
        emit_op_load_data(&state->code, &scope->stack_base, var->loc, varsize);
        *copy = *var;
        copy->loc = scope->stack_base;
        copy->loctype = var_loc_stack;
        copy->lvalue = false;
        copy->inscope = false;
        copy->scope_id = UINT32_MAX;
        break;
    }
    if (!comp_check_code_size(state)) return false;
    return true;
}

// Will push the variable as the new type onto the stack
static bool comp_cast(comp_state_t *state,
                      comp_var_t *var,
                      const comp_type_t *type,
                      uint32_t topbefore) {
    comp_type_t typeto = *type, typefrom = var->type;
    if (!comp_get_by_value(state, var, var)) return false;
    if (!comp_get_underlying_typedef(state, &typeto)) return false;
    if (!comp_get_underlying_typedef(state, &typefrom)) return false;
    if (comp_types_exacteq(state, false, 0, typeto, typefrom)) {
        if (typeto.num_lvls > 1 && !comp_types_exacteq(state, true, 1, typeto, typefrom)) return true;
        if (~typeto.lvls[0].quals & qual_const && typefrom.lvls[0].quals & qual_const) {
            comp_error(state, "Can't cast const type to non-const");
            return false;
        }
        var->type.lvls[0].quals = typeto.lvls[0].quals;
    }

    const comp_type_lvl_t *tolvl = typeto.lvls;
    comp_type_lvl_t *fromlvl = typefrom.lvls;
    comp_scope_t *scope = state->res->scopes + state->scopes_top;

    assert(var->loctype == var_loc_stack);
    if (tolvl->type == type_arr) return false;
    if (fromlvl->type == type_struct
        && tolvl->type == type_struct
        && fromlvl->id != tolvl->id) {
        comp_error(state, "Can not cast a struct to a struct of another type!");
        return false;
    }
    if ((fromlvl->type == type_ref || fromlvl->type == type_ptr)
        && fromlvl->type != tolvl->type) {
        comp_error(state, "Can't cast refrences or pointers!");
        return false;
    }

    var->type = typeto;
    topbefore = alignid(topbefore - 1, comp_get_typealign(state, typeto, 0));
    switch (fromlvl->type) {
    case type_b8: case type_u8: case type_c8: case type_i8: {
        const bool u = fromlvl->type == type_u8;
        switch (tolvl->type) {
        case type_i8: case type_c8: case type_u8: return true;
        case type_b8:
            if (fromlvl->type == type_b8) return true;
            emit_op_extend(&state->code, &scope->stack_base, 0, u);
            emit_op_extend(&state->code, &scope->stack_base, 1, u);
            emit_op_imm_i32(&state->code, &scope->stack_base, 0);
            emit_op_push_ne(&state->code, &scope->stack_base, false, false);
            emit_op_store_stack(&state->code, topbefore - scope->stack_base, 1);
            emit_op_add_stack(&state->code, &scope->stack_base, topbefore - scope->stack_base);
            if (!comp_check_code_size(state)) return false;
            return true;
        case type_i16: case type_u16:
            emit_op_extend(&state->code, &scope->stack_base, 0, u);
            if (!comp_check_code_size(state)) return false;
            return true;
        case type_i32: case type_u32: case type_c32:
            emit_op_extend(&state->code, &scope->stack_base, 0, u);
            emit_op_extend(&state->code, &scope->stack_base, 1, u);
            if (!comp_check_code_size(state)) return false;
            return true;
        case type_i64: case type_u64:
            emit_op_extend(&state->code, &scope->stack_base, 0, u);
            emit_op_extend(&state->code, &scope->stack_base, 1, u);
            emit_op_extend(&state->code, &scope->stack_base, 2, u);
            if (!comp_check_code_size(state)) return false;
            return true;
        case type_f32:
            emit_op_extend(&state->code, &scope->stack_base, 0, u);
            emit_op_extend(&state->code, &scope->stack_base, 1, u);
            if (u) emit_op_u2f(&state->code, false);
            else emit_op_i2f(&state->code, false);
            if (!comp_check_code_size(state)) return false;
            return true;
        case type_f64:
            emit_op_extend(&state->code, &scope->stack_base, 0, u);
            emit_op_extend(&state->code, &scope->stack_base, 1, u);
            emit_op_extend(&state->code, &scope->stack_base, 2, u);
            if (u) emit_op_u2f(&state->code, true);
            else emit_op_i2f(&state->code, true);
            if (!comp_check_code_size(state)) return false;
            return true;
        }
    } return false;
    case type_u16: case type_i16: {
        const bool u = fromlvl->type == type_u16;
        switch (tolvl->type) {
        case type_i8: case type_c8: case type_u8:
            emit_op_reduce(&state->code, &scope->stack_base, 1);
            emit_op_store_stack(&state->code, topbefore - scope->stack_base, 1);
            emit_op_add_stack(&state->code, &scope->stack_base, topbefore - scope->stack_base);
            if (!comp_check_code_size(state)) return false;
            return true;
        case type_b8:
            emit_op_extend(&state->code, &scope->stack_base, 1, u);
            emit_op_imm_i32(&state->code, &scope->stack_base, 0);
            emit_op_push_ne(&state->code, &scope->stack_base, false, false);
            emit_op_store_stack(&state->code, topbefore - scope->stack_base, 1);
            emit_op_add_stack(&state->code, &scope->stack_base, topbefore - scope->stack_base);
            if (!comp_check_code_size(state)) return false;
            return true;
        case type_i16: case type_u16: case type_c16: return true;
        case type_i32: case type_u32: case type_c32:
            emit_op_extend(&state->code, &scope->stack_base, 1, u);
            if (!comp_check_code_size(state)) return false;
            return true;
        case type_i64: case type_u64:
            emit_op_extend(&state->code, &scope->stack_base, 1, u);
            emit_op_extend(&state->code, &scope->stack_base, 2, u);
            if (!comp_check_code_size(state)) return false;
            return true;
        case type_f32:
            emit_op_extend(&state->code, &scope->stack_base, 1, u);
            if (u) emit_op_u2f(&state->code, false);
            else emit_op_i2f(&state->code, false);
            if (!comp_check_code_size(state)) return false;
            return true;
        case type_f64:
            emit_op_extend(&state->code, &scope->stack_base, 1, u);
            emit_op_extend(&state->code, &scope->stack_base, 2, u);
            if (u) emit_op_u2f(&state->code, true);
            else emit_op_i2f(&state->code, true);
            if (!comp_check_code_size(state)) return false;
            return true;
        }
    } return false;
    case type_i32: case type_u32: {
        const bool u = fromlvl->type == type_u32;
        switch (tolvl->type) {
        case type_i8: case type_c8: case type_u8:
            emit_op_reduce(&state->code, &scope->stack_base, 2);
            emit_op_reduce(&state->code, &scope->stack_base, 1);
            emit_op_store_stack(&state->code, topbefore - scope->stack_base, 1);
            emit_op_add_stack(&state->code, &scope->stack_base, topbefore - scope->stack_base);
            if (!comp_check_code_size(state)) return false;
            return true;
        case type_b8:
            emit_op_imm_i32(&state->code, &scope->stack_base, 0);
            emit_op_push_ne(&state->code, &scope->stack_base, false, false);
            emit_op_store_stack(&state->code, topbefore - scope->stack_base, 1);
            emit_op_add_stack(&state->code, &scope->stack_base, topbefore - scope->stack_base);
            if (!comp_check_code_size(state)) return false;
            return true;
        case type_i16: case type_u16: case type_c16:
            emit_op_reduce(&state->code, &scope->stack_base, 2);
            emit_op_store_stack(&state->code, topbefore - scope->stack_base, 2);
            emit_op_add_stack(&state->code, &scope->stack_base, topbefore - scope->stack_base);
            if (!comp_check_code_size(state)) return false;
            return true;
        case type_i32: case type_u32: case type_c32: return true;
        case type_i64: case type_u64:
            emit_op_extend(&state->code, &scope->stack_base, 2, false);
            if (!comp_check_code_size(state)) return false;
            return true;
        case type_f32:
            if (u) emit_op_u2f(&state->code, false);
            else emit_op_i2f(&state->code, false);
            if (!comp_check_code_size(state)) return false;
            return true;
        case type_f64:
            emit_op_extend(&state->code, &scope->stack_base, 2, false);
            if (u) emit_op_u2f(&state->code, true);
            else emit_op_i2f(&state->code, true);
            if (!comp_check_code_size(state)) return false;
            return true;
        }
    } return false;
    case type_i64: case type_u64: {
        const bool u = fromlvl->type == type_u64;
        switch (tolvl->type) {
        case type_i8: case type_c8: case type_u8:
            emit_op_reduce(&state->code, &scope->stack_base, 3);
            emit_op_reduce(&state->code, &scope->stack_base, 2);
            emit_op_reduce(&state->code, &scope->stack_base, 1);
            emit_op_store_stack(&state->code, topbefore - scope->stack_base, 1);
            emit_op_add_stack(&state->code, &scope->stack_base, topbefore - scope->stack_base);
            if (!comp_check_code_size(state)) return false;
            return true;
        case type_b8:
            emit_op_reduce(&state->code, &scope->stack_base, 3);
            emit_op_imm_i32(&state->code, &scope->stack_base, 0);
            emit_op_push_ne(&state->code, &scope->stack_base, false, false);
            emit_op_store_stack(&state->code, topbefore - scope->stack_base, 1);
            emit_op_add_stack(&state->code, &scope->stack_base, topbefore - scope->stack_base);
            if (!comp_check_code_size(state)) return false;
            return true;
        case type_i16: case type_u16: case type_c16:
            emit_op_reduce(&state->code, &scope->stack_base, 3);
            emit_op_reduce(&state->code, &scope->stack_base, 2);
            emit_op_store_stack(&state->code, topbefore - scope->stack_base, 2);
            emit_op_add_stack(&state->code, &scope->stack_base, topbefore - scope->stack_base);
            if (!comp_check_code_size(state)) return false;
            return true;
        case type_i32: case type_u32: case type_c32:
            emit_op_reduce(&state->code, &scope->stack_base, 3);
            emit_op_store_stack(&state->code, topbefore - scope->stack_base, 4);
            emit_op_add_stack(&state->code, &scope->stack_base, topbefore - scope->stack_base);
            if (!comp_check_code_size(state)) return false;
            return true;
        case type_i64: case type_u64: return true;
        case type_f32:
            emit_op_reduce(&state->code, &scope->stack_base, 3);
            if (u) emit_op_u2f(&state->code, false);
            else emit_op_i2f(&state->code, false);
            emit_op_store_stack(&state->code, topbefore - scope->stack_base, 4);
            emit_op_add_stack(&state->code, &scope->stack_base, topbefore - scope->stack_base);
            if (!comp_check_code_size(state)) return false;
            return true;
        case type_f64:
            if (u) emit_op_u2f(&state->code, true);
            else emit_op_i2f(&state->code, true);
            if (!comp_check_code_size(state)) return false;
            return true;
        }
    } return false;
    case type_f32:
        switch (tolvl->type) {
        case type_i8: case type_c8: case type_u8:
            if (tolvl->type == type_u8) emit_op_f2u(&state->code, false);
            else emit_op_f2i(&state->code, false);
            emit_op_reduce(&state->code, &scope->stack_base, 2);
            emit_op_reduce(&state->code, &scope->stack_base, 1);
            emit_op_store_stack(&state->code, topbefore - scope->stack_base, 1);
            emit_op_add_stack(&state->code, &scope->stack_base, topbefore - scope->stack_base);
            if (!comp_check_code_size(state)) return false;
            return true;
        case type_b8:
            emit_op_f2u(&state->code, false);
            emit_op_imm_i32(&state->code, &scope->stack_base, 0);
            emit_op_push_ne(&state->code, &scope->stack_base, false, false);
            emit_op_store_stack(&state->code, topbefore - scope->stack_base, 1);
            emit_op_add_stack(&state->code, &scope->stack_base, topbefore - scope->stack_base);
            if (!comp_check_code_size(state)) return false;
            return true;
        case type_i16: case type_u16: case type_c16:
            if (tolvl->type == type_u16) emit_op_f2u(&state->code, false);
            else emit_op_f2i(&state->code, false);
            emit_op_reduce(&state->code, &scope->stack_base, 2);
            emit_op_store_stack(&state->code, topbefore - scope->stack_base, 2);
            emit_op_add_stack(&state->code, &scope->stack_base, topbefore - scope->stack_base);
            if (!comp_check_code_size(state)) return false;
            return true;
        case type_i32: case type_u32: case type_c32:
            if (tolvl->type == type_u32) emit_op_f2u(&state->code, false);
            else emit_op_f2i(&state->code, false);
            if (!comp_check_code_size(state)) return false;
            return true;
        case type_i64: case type_u64:
            if (tolvl->type == type_u64) emit_op_f2u(&state->code, false);
            else emit_op_f2i(&state->code, false);
            emit_op_extend(&state->code, &scope->stack_base, 2, tolvl->type == type_u64);
            if (!comp_check_code_size(state)) return false;
            return true;
        case type_f32: return true;
        case type_f64:
            emit_op_f2d(&state->code, &scope->stack_base);
            if (!comp_check_code_size(state)) return false;
            return true;
        }
    return false;
    case type_f64:
        switch (tolvl->type) {
        case type_i8: case type_c8: case type_u8:
            if (tolvl->type == type_u8) emit_op_f2u(&state->code, true);
            else emit_op_f2i(&state->code, true);
            emit_op_reduce(&state->code, &scope->stack_base, 3);
            emit_op_reduce(&state->code, &scope->stack_base, 2);
            emit_op_reduce(&state->code, &scope->stack_base, 1);
            emit_op_store_stack(&state->code, topbefore - scope->stack_base, 1);
            emit_op_add_stack(&state->code, &scope->stack_base, topbefore - scope->stack_base);
            if (!comp_check_code_size(state)) return false;
            return true;
        case type_b8:
            emit_op_f2u(&state->code, false);
            emit_op_reduce(&state->code, &scope->stack_base, 3);
            emit_op_imm_i32(&state->code, &scope->stack_base, 0);
            emit_op_push_ne(&state->code, &scope->stack_base, false, false);
            emit_op_store_stack(&state->code, topbefore - scope->stack_base, 1);
            emit_op_add_stack(&state->code, &scope->stack_base, topbefore - scope->stack_base);
            if (!comp_check_code_size(state)) return false;
            return true;
        case type_i16: case type_u16: case type_c16:
            if (tolvl->type == type_u16) emit_op_f2u(&state->code, false);
            else emit_op_f2i(&state->code, false);
            emit_op_reduce(&state->code, &scope->stack_base, 3);
            emit_op_reduce(&state->code, &scope->stack_base, 2);
            emit_op_store_stack(&state->code, topbefore - scope->stack_base, 2);
            emit_op_add_stack(&state->code, &scope->stack_base, topbefore - scope->stack_base);
            if (!comp_check_code_size(state)) return false;
            return true;
        case type_i32: case type_u32: case type_c32:
            if (tolvl->type == type_u32) emit_op_f2u(&state->code, false);
            else emit_op_f2i(&state->code, false);
            emit_op_reduce(&state->code, &scope->stack_base, 3);
            emit_op_store_stack(&state->code, topbefore - scope->stack_base, 4);
            emit_op_add_stack(&state->code, &scope->stack_base, topbefore - scope->stack_base);
            if (!comp_check_code_size(state)) return false;
            return true;
        case type_i64: case type_u64:
            if (tolvl->type == type_u64) emit_op_f2u(&state->code, true);
            else emit_op_f2i(&state->code, true);
            if (!comp_check_code_size(state)) return false;
            return true;
        case type_f32:
            emit_op_d2f(&state->code, &scope->stack_base);
            emit_op_store_stack(&state->code, topbefore - scope->stack_base, 4);
            emit_op_add_stack(&state->code, &scope->stack_base, topbefore - scope->stack_base);
            if (!comp_check_code_size(state)) return false;
            return true;
        case type_f64: return true;
        }
    return false;
    default: return false;
    }

    return true;
}

static bool comp_typed_expr(comp_state_t *state,
                            comp_var_t *ret,
                            const ast_t *expr,
                            const comp_type_t *deftype);
static bool comp_expr(comp_state_t *state,
                      comp_var_t *ret,
                      const ast_t *node,
                      const comp_type_t *arrty) {
    comp_scope_t *scope = state->res->scopes + state->scopes_top;

    if (!node) return false;
    switch (node->type) {
    case ast_literal: return comp_push_literal_type(state, node, ret);
    case ast_ident: {
        comp_var_t *scopevar;
        if (!(scopevar = comp_get_scopevar(state, node->token.data.str))) return false;
        *ret = *scopevar;
    } return true;
    case ast_op_binary:
        if (node->token.type == tok_arrow) {
            assert(0 && "unimplemented");
        }
        if (node->token.type == tok_dot) {
            assert(0 && "unimplemented");
        }
        if (node->token.type == tok_comma) {
            assert(0 && "unimplemented");
        }
        if (node->token.type == tok_as) {
            assert(0 && "unimplemented");
            //comp_type_t totype;
            //const uint32_t loc = state->dsz;
            //if (!comp_get_type(state, expr->b, &totype)) return false;
            //if (!comptime_expr(state, true, ret, expr->a, NULL)) return false;
            //if (!comptime_cast(state, ret, &totype, loc)) return false;
            //return true;
        }

        // The rest from here on out is arithmetic operations
        comp_type_t tyleft, tyright;
        if (!comp_get_expr_type(state, &tyleft, node->a)
            || !comp_get_expr_type(state, &tyright, node->b)) return false;
        comp_type_t commonty;
        if (!comp_get_type_promotion(state, tyleft, tyright, &commonty)) return false;

        comp_var_t left, right;
        int32_t loc = scope->stack_base;
        if (!comp_expr(state, &left, node->a, NULL)) return false;
        if (!comp_cast(state, &left, &commonty, loc)) return false;
        loc = scope->stack_base;
        if (!comp_expr(state, &right, node->b, NULL)) return false;
        if (!comp_cast(state, &right, &commonty, loc)) return false;
        assert(comp_types_exacteq(state, false, 0, left.type, right.type));

        // CONT HERE:
#define do_op(CASE, OPNAME) \
case CASE: \
    emit_op_##OPNAME(&state->code, \
                    &scope->stack_base, \
                    comp_is_64bits(state, commonty.lvls[0]), \
                    comp_is_unsigned(state, commonty.lvls[0]), \
                    comp_is_floating(state, commonty.lvls[0])); \
break;
        switch (node->token.type) {
        do_op(tok_plus, add)
        do_op(tok_minus, sub)
        do_op(tok_slash, div)
        do_op(tok_star, mul)
        default: assert(0 && "unimplemented");
        }
#undef do_op
        if (!comp_check_code_size(state)) return false;
        *ret = (comp_var_t){
            .type = commonty,
            .loc = scope->stack_base,
            .lvalue = false,
            .inscope = false,
            .scope_id = UINT32_MAX,
            .loctype = var_loc_stack,
        };

        return true;
    case ast_op_call: {
        if (node->a->type != ast_ident) assert(0 && "Implement function pointers!");
        for (uint32_t i = 0; i < state->num_fns; i++) {
            comp_fn_t *fn = state->res->fns + i;
            if (strview_eq(fn->name, node->a->token.data.str)) {
                int32_t stack = scope->stack_base;
                emit_op_sub_stack(&state->code, &scope->stack_base, scope->stack_base - alignid(scope->stack_base, COMP_MAX_ALIGN));
                const ast_t *arg = node->b;
                for (uint32_t j = fn->types_loc; j < fn->types_loc + fn->num_types; j++, arg = arg->next) {
                    if (!arg) {
                        comp_error(state, "Missing function arguments");
                        return false;
                    }
                    comp_update_line(state, arg);
                    comp_typebuf_t *const tybuf = state->res->typebuf + j;
                    comp_var_t argvar;
                    if (!comp_typed_expr(state, &argvar, arg, &tybuf->type)) return false;
                    if (!comp_get_by_value(state, &(comp_var_t){0}, &argvar)) return false;
                }
                if (arg) {
                    comp_error(state, "Too many arguments passed to function");
                    return false;
                }

                if (fn->is_extern) emit_op_extern_call(&state->code, fn->loc);
                else emit_op_call(&state->code, fn->loc);
                if (!comp_check_code_size(state)) return false;

                // TODO: Handle return
                comp_type_t rettype = state->res->typebuf[fn->types_loc+fn->num_types].type;
                uint32_t size, align = comp_get_typealign(state, rettype, 0);
                if (!comp_get_typesize(state, &size, rettype, 0)) return false;
                int32_t vartarget = alignid(stack - size, align);

                // The return variable was added ontop of the stack.
                scope->stack_base = alignid(scope->stack_base, align);
                scope->stack_base -= size;

                // Push the variable up the stack and pop off the arguments
                emit_op_store_stack(&state->code, vartarget - scope->stack_base, size);
                emit_op_add_stack(&state->code, &scope->stack_base, vartarget - scope->stack_base);
                *ret = (comp_var_t){
                    .type = rettype,
                    .loc = scope->stack_base,
                    .lvalue = false,
                    .inscope = false,
                    .scope_id = UINT32_MAX,
                };
                return true;
            }
        }
    } return false;
    default: assert(false && "Expression feature not supported! (yet)");
    }
}

// Compile an expression that expects a certain type
static bool comp_typed_expr(comp_state_t *state,
                            comp_var_t *ret,
                            const ast_t *expr,
                            const comp_type_t *deftype) {
    comp_update_line(state, expr);
    if (!expr) {
        uint32_t size;
        if (!comp_get_typesize(state, &size, *deftype, 0)) {
            comp_error(state, "Type does not have a valid size");
            return false;
        }
        assert(false && "TODO: Undefined typed vars!");
    } else {
        //const int32_t stack = state->dsz;
        if (deftype && deftype->lvls[0].type == type_arr) {
            comp_type_t innerty = *deftype;
            assert(comp_remove_type_lvls(state, &innerty, 1));
            if (!comp_expr(state, ret, expr, &innerty)) {
                comp_error(state, "Expected compile time expression!");
                return false;
            }
        } else if (!comp_expr(state, ret, expr, NULL)) {
            comp_error(state, "Expected compile time expression!");
            return false;
        }
        //if (deftype && !comptime_cast(state, ret, deftype, loc)) return false;
    }

    return true;
}

static bool comp_stmt_group(comp_state_t *state,
                            const ast_t *node,
                            bool *did_return);
static bool comp_stmt_let(comp_state_t *state, const ast_t *node) {
    comp_var_t var;
    if (node->a) {
        comp_type_t type;
        if (!comp_get_type(state, node->a, &type)) return false;
        if (!comp_typed_expr(state, &var, node->b, &type)) return false;
    } else {
       if (!comp_expr(state, &var,  node->b, NULL)) return false;
    }
    if (!comp_add_var_to_scope(state, &var, node->token.data.str)) return false;
    return true;
}

static bool comp_stmt_for(comp_state_t *state,
                          const ast_t *node,
                          bool *did_return) {
    comp_update_line(state, node);

    if (node->b->type != ast_op_binary
        || node->b->token.type != tok_dotdot) {
        comp_error(state, "Only range loops supported currently. List loops coming soon.");
        return false;
    }

    comp_type_t loopty;
    if (node->a->type == ast_var) {
        if (!comp_get_type(state, node->a, &loopty)) return NULL;
    } else {
        comp_type_t start, end;
        if (!comp_get_expr_type(state, &start, node->b->a)) return NULL;
        if (!comp_get_expr_type(state, &end, node->b->b)) return NULL;
        if (!comp_get_type_promotion(state, start, end, &loopty)) return NULL;
    }

    comp_scope_t *scope = state->res->scopes + ++state->scopes_top;
    *scope = state->res->scopes[state->scopes_top-1];

    comp_var_t loopvar;
    if (!comp_typed_expr(state, &loopvar, node->b->a, &loopty)) return NULL;
    if (!comp_add_var_to_scope(state, &loopvar, node->a->token.data.str)) return NULL;

    int32_t checkstack = scope->stack_base;
    const int32_t checkstart = state->code - state->res->code;
    if (!comp_get_by_value(state, &(comp_var_t){0}, &loopvar)) return NULL;
    comp_var_t end;
    if (!comp_typed_expr(state, &end, node->b->b, &loopty)) return NULL;
    if (!comp_get_by_value(state, &(comp_var_t){0}, &end)) return NULL;
    emit_op_push_ge(&state->code,
                    &scope->stack_base,
                    comp_is_64bits(state, loopty.lvls[0]),
                    comp_is_floating(state, loopty.lvls[0]),
                    comp_is_unsigned(state, loopty.lvls[0]));
    uint8_t *branchloc = state->code;
    emit_op_bne(&state->code, &scope->stack_base, 0, false); // Gets replaced
    uint32_t branchstack = checkstack - scope->stack_base;
    emit_op_add_stack(&state->code, &scope->stack_base, branchstack);
    if (!comp_check_code_size(state)) return false;
    if (!comp_stmt_group(state, node->child, did_return)) return false;

    int32_t addstack = scope->stack_base;
    comp_type_t addty = (comp_type_t){
        .lvls[0] = comp_get_type_promotion_single(state, loopty.lvls[0]),
        .num_lvls = 1,
    };
    comp_var_t addloopvar;
    if (!comp_get_by_value(state, &addloopvar, &loopvar)) return false;
    if (!comp_cast(state, &addloopvar, &addty, addstack)) return false;
    switch (addty.lvls[0].type) {
    case type_i32: emit_op_imm_i32(&state->code, &scope->stack_base, 1); break;
    case type_u32: emit_op_imm_u32(&state->code, &scope->stack_base, 1); break;
    case type_i64: emit_op_imm_i64(&state->code, &scope->stack_base, 1); break;
    case type_u64: emit_op_imm_u64(&state->code, &scope->stack_base, 1); break;
    case type_f32: emit_op_imm_f32(&state->code, &scope->stack_base, 1.0f); break;
    case type_f64: emit_op_imm_f64(&state->code, &scope->stack_base, 1.0); break;
    default: assert(0 && "cant happen nono for loop type");
    }
    emit_op_add(&state->code,
                &scope->stack_base,
                comp_is_64bits(state, addty.lvls[0]),
                comp_is_unsigned(state, addty.lvls[0]),
                comp_is_floating(state, addty.lvls[0]));
    uint32_t loopvarsize;
    if (!comp_get_typesize(state, &loopvarsize, loopty, 0)) return false;
    int32_t caststack = scope->stack_base;
    if (!comp_cast(state, &addloopvar, &loopty, caststack)) return false;
    emit_op_store_stack(&state->code, loopvar.loc - scope->stack_base, loopvarsize);
    emit_op_add_stack(&state->code, &scope->stack_base, addstack - scope->stack_base);

    emit_op_jump(&state->code, checkstart - (int32_t)(state->code - state->res->code));
    emit_op_add_stack(&state->code, &scope->stack_base, branchstack);
    emit_op_bne(&branchloc, &(int32_t){0}, state->code - branchloc, false); // Replace it

    emit_op_add_stack(&state->code, &scope->stack_base, state->res->scopes[state->scopes_top-1].stack_base - scope->stack_base);
    if (!comp_check_code_size(state)) return false;
    state->scopes_top--;

    return true;
}

static bool comp_stmt_group(comp_state_t *state,
                            const ast_t *node,
                            bool *did_return) {
    if (state->scopes_top + 1 >= state->res->scopes_len) {
        comp_error(state, "Number of scopes exceeded allocated amount");
        return false;
    }
    comp_scope_t *scope = state->res->scopes + ++state->scopes_top;
    *scope = state->res->scopes[state->scopes_top-1];
   
    for (const ast_t *curr = node->child; curr; curr = curr->next) {
        switch (curr->type) {
        case ast_stmt_let:
            if (!comp_stmt_let(state, curr)) return false;
            break;
        case ast_stmt_group:
            if (!comp_stmt_group(state, curr, did_return)) return false;
            break;
        case ast_stmt_for:
            if (!comp_stmt_for(state, curr, did_return)) return false;
            break;
        case ast_stmt_expr: {
            int32_t stack = scope->stack_base;
            if (!comp_expr(state, &(comp_var_t){0}, curr->child, NULL)) return false;
            emit_op_add_stack(&state->code, &scope->stack_base, stack - scope->stack_base);
            if (!comp_check_code_size(state)) return false;
        } break;
        case ast_stmt_return: {
            *did_return |= true;
            comp_type_t *rettype = &state->res->typebuf[state->currfn->types_loc+state->currfn->num_types].type;
            bool voidret = rettype->lvls[0].type == type_u0;
            if (!voidret && !comp_typed_expr(state, &(comp_var_t){0}, curr->child, rettype)) {
                comp_error(state, "Return statement doesn't return same type as function!");
                return false;
            }
            if (voidret != !curr->child) {
                comp_error(state, "Return not matching function!");
                return false;
            }

            comp_scope_t *fnscope = state->res->scopes;
            uint32_t retsize, retalign = comp_get_typealign(state, *rettype, 0);
            if (!comp_get_typesize(state, &retsize, *rettype, 0)) return false;
            int32_t vartarget = alignid(fnscope->stack_base - retsize, retalign);
            emit_op_store_stack(&state->code, vartarget - scope->stack_base, retsize);
            emit_op_add_stack(&state->code, &scope->stack_base, vartarget - scope->stack_base);
            state->scopes_top--;
            emit_op_ret(&state->code);
            if (!comp_check_code_size(state)) return false;

            return true;
        } break;
        default: assert(0 && "comp_stmt_group: expected statement!");
        }
    }

    emit_op_add_stack(&state->code, &scope->stack_base, state->res->scopes[state->scopes_top-1].stack_base - scope->stack_base);
    if (!comp_check_code_size(state)) return false;
    state->scopes_top--;

    return true;
}

static comp_fn_t *comp_fn(comp_state_t *state, const ast_t *node) {
    if (node->type == ast_type_static && node->child->type != ast_def_func) return NULL;
    if (node->type != ast_def_func) return NULL;
    bool is_static = false;
    if (node->type == ast_type_static) {
        is_static = true;
        node = node->child;
    }

    comp_fn_t *sig = comp_new_func_sig(state, node);
    if (!sig || !node->child) {
        comp_error(state, "Invalid function signature.");
        return NULL;
    }
    comp_update_line(state, node);
    state->currfn = sig;
    sig->loc = state->code - state->res->code;
    state->res->scopes->stack_base = sig->stack_base;

    // Add it to the internal functions list
    if (!is_static) {
        if (state->num_ifns == state->res->internfn_len) {
            comp_error(state, "Ran out of internal functions to name!\nTry setting functions that don't need to be named as static!");
            return NULL;
        }
        state->res->internfn_loc[state->num_ifns] = sig->loc;
        state->res->internfn_name[state->num_ifns] = sig->name;
        state->num_ifns++;
    }

    for (uint32_t i = sig->types_loc; i < sig->types_loc+sig->num_types; i++) {
        comp_typebuf_t *tybuf = state->res->typebuf + i;
        comp_var_t var = (comp_var_t){
            .loctype = var_loc_stack,
            .loc = state->res->scopes->stack_base + tybuf->offs,
            .type = tybuf->type,
        };
        if (!comp_add_var_to_scope(state, &var, tybuf->name)) return NULL;
    }
    
    bool did_return = false;
    if (!comp_stmt_group(state, node->child, &did_return)) return NULL;
    if (!did_return) emit_op_ret(&state->code);
    if (state->res->typebuf[sig->types_loc+sig->num_types].type.lvls[0].type != type_u0
        && !did_return) {
        comp_error(state, "Function that must return did not return");
        return NULL;
    }
    if (!comp_check_code_size(state)) return NULL;
    return sig;
}

void compile(comp_resources_t *res) {
    comp_state_t state = {
        .res = res,
        .scopes_top = 0,
        .code = res->code,
    };
    if (res->scopes_len < 1) {
        comp_error(&state, "Need more scope buffer room to start compiling.");
        return;
    }
    res->scopes[0] = (comp_scope_t){
        .scope_base = 0,
        .stack_base = 0,
    };

    for (const ast_t *node = res->ast; node; node = node->next) {
        comp_struct(&state, node);
        comp_global(&state, node);
        comp_extern_fn(&state, node);
        comp_fn(&state, node);
    }
}

