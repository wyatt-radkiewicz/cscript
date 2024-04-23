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
        typebuf->offs = fn->stack_base - typebuf->offs;
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

static bool comp_expr(comp_state_t *state, const ast_t *node, comp_var_t *ret) {
    comp_scope_t *scope = state->res->scopes + state->scopes_top;

    if (!node) return false;
    switch (node->type) {
    case ast_literal: return comp_push_literal_type(state, node, ret);
    case ast_op_call: {
        if (node->a->type != ast_ident) assert(0 && "Implement function pointers!");
        for (uint32_t i = 0; i < state->num_fns; i++) {
            comp_fn_t *fn = state->res->fns + i;
            if (strview_eq(fn->name, node->a->token.data.str)) {
                int32_t stack = scope->stack_base;
                scope->stack_base = alignid(scope->stack_base, COMP_MAX_ALIGN);
                const ast_t *arg = node->b;
                for (uint32_t j = fn->types_loc; j < fn->types_loc + fn->num_types; j++, arg = arg->next) {
                    if (!arg) {
                        comp_error(state, "Missing function arguments");
                        return false;
                    }
                    comp_update_line(state, arg);
                    //comp_typebuf_t *tybuf = state->res->typebuf + j;

                    if (!comp_expr(state, arg, &(comp_var_t){0})) return false;
                }
                if (arg) {
                    comp_error(state, "Too many arguments passed to function");
                    return false;
                }

                if (fn->is_extern) emit_op_extern_call(&state->code, fn->loc);
                else emit_op_call(&state->code, fn->loc);
                if (!comp_check_code_size(state)) return false;

                // TODO: Handle return
                emit_op_add_stack(&state->code, &scope->stack_base, stack - scope->stack_base);
                return true;
            }
        }
    } return false;
    default: assert(false && "Expression feature not supported! (yet)");
    }
}

static bool comp_stmt_group(comp_state_t *state, const ast_t *node) {
    if (state->scopes_top + 1 >= state->res->scopes_len) {
        comp_error(state, "Number of scopes exceeded allocated amount");
        return false;
    }
    comp_scope_t *scope = state->res->scopes + ++state->scopes_top;
    *scope = state->res->scopes[state->scopes_top-1];
   
    for (const ast_t *curr = node->child; curr; curr = curr->next) {
        switch (curr->type) {
        case ast_stmt_group:
            if (!comp_stmt_group(state, curr)) return false;
            break;
        case ast_stmt_expr: {
            int32_t stack = scope->stack_base;
            if (!comp_expr(state, curr->child, &(comp_var_t){0})) return false;
            emit_op_add_stack(&state->code, &scope->stack_base, stack - scope->stack_base);
            if (!comp_check_code_size(state)) return false;
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
    if (node->type != ast_def_func) return NULL;
    comp_fn_t *sig = comp_new_func_sig(state, node);
    if (!sig || !node->child) {
        comp_error(state, "Invalid function signature.");
        return NULL;
    }
    sig->loc = state->code - state->res->code;
    state->res->scopes->stack_base = sig->stack_base;
    if (!comp_stmt_group(state, node->child)) return NULL;
    emit_op_ret(&state->code);
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

