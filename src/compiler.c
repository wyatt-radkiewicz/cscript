#include <stdarg.h>
#include <string.h>

#include "compiler.h"

typedef struct comp_state {
    comp_resources_t *res;

    int32_t line, chr;

    uint32_t scope_top, stack_top;

    uint32_t cptr, dptr;
    uint32_t num_ifns, num_typebufs,
             num_structs, num_typedefs,
             num_pfns, num_fns;
    size_t intbuf_len;
} comp_state_t;

static bool comp_error(comp_state_t *state, const char *msg, ...) {
    if (state->res->num_errors == state->res->error_len) return false;
    if (state->res->num_errors + 1 == state->res->error_len) {
        state->res->error[state->res->num_errors++] = (error_t){
            .category = error_category_compiler,
            .line = state->line,
            .chr = state->chr,
            .msg = "Too many errors. Aborting...",
        };
        return false;
    }

    error_t *error = state->res->error + state->res->num_errors++;
    *error = (error_t){
        .category = error_category_compiler,
        .line = state->line,
        .chr = state->chr,
    };

    va_list args;
    va_start(args, msg);
    if (vsnprintf(error->msg, arrsz(error->msg), msg, args) >= arrsz(error->msg)) {
        strcpy(error->msg, "Message too big for message buffer!");
    }
    va_end(args);

    return true;
}
static void comp_update_line(comp_state_t *state, const ast_t *node) {
    if (!node) return;
    state->line = node->token.line;
    state->chr = node->token.chr;
}

static comp_typebuf_t *comp_alloc_next_type(comp_state_t *state) {
    if (state->num_typebufs == state->res->typebuf_len) {
        comp_error(state, "Ran out of type buffer storage.");
        return NULL;
    }
    return state->res->typebuf + state->num_typebufs++;
}

//static comp_var_t comp_comptime_expr(comp_state_t *state, );

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
        break;
    case ast_type_pfn: break;
    case ast_type_array: break;
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
    
    for (const ast_t *param = fndef->a;
        param;
        param = param->next, fn->num_types++) {
        comp_typebuf_t *typebuf = comp_alloc_next_type(state);
        if (!typebuf) return NULL;
        if (!comp_get_type(state, param->child, &typebuf->type)) return NULL;
        typebuf->name = param->token.data.str;
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
    }

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
    comp_extern_fns(&state);
}

