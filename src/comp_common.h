#ifndef _comp_common_h_
#define _comp_common_h_

#include <stdarg.h>
#include <stdalign.h>
#include <string.h>

#include "compiler.h"

typedef struct comp_state {
    comp_resources_t *res;

    int32_t line, chr;

    uint32_t scope_top, stack_top;

    uint32_t csz, dsz;
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

static bool comp_get_typelvl(comp_state_t *state, const ast_t **node, comp_type_lvl_t *lvl);
static bool comp_get_type(comp_state_t *state, const ast_t *node, comp_type_t *type);

static comp_typebuf_t *comp_alloc_next_type(comp_state_t *state) {
    if (state->num_typebufs == state->res->typebuf_len) {
        comp_error(state, "Ran out of type buffer storage.");
        return NULL;
    }
    return state->res->typebuf + state->num_typebufs++;
}

// Get the base type (aka following typedefs to their actuall definition)
// Edits type in place
static bool comp_get_underlying_typedef(comp_state_t *state, comp_type_t *type) {
    if (type->lvls[type->num_lvls - 1].type != type_typedef) return true;
    
    // Append the typedef info
    const uint32_t oldlvl = type->num_lvls - 1;
    const int quals = type->lvls[oldlvl].quals;
    const comp_type_t *innertype = &state->res->typebuf[type->lvls[oldlvl].id].type;

    if (innertype->num_lvls + type->num_lvls > MAX_TYPE_NESTING) {
        comp_error(state, "Type nesting too far!");
        return false;
    }

    memcpy(type->lvls + oldlvl, innertype->lvls, innertype->num_lvls * sizeof(type->lvls[0]));
    type->num_lvls += innertype->num_lvls;
    type->lvls[oldlvl].quals |= quals;

    return true;
}

static uint32_t comp_get_typealign(comp_state_t *state, const comp_type_lvl_t lvl) {
    switch (lvl.type) {
    case type_b8: case type_c8:
    case type_i8: case type_u8:
        return 1;
    case type_u16: case type_i16: case type_c16:
        return 2;
    case type_u32: case type_i32: case type_f32:
        return 4;
    case type_u64: case type_i64: case type_f64:
        return 8;
    case type_ptr: case type_ref:
        return 8;
    // These are either offsets into the code segment, or external fn ids,
    // so its always a uint32_t
    case type_pfn: return 4;
    case type_u0: return 1;
    // TODO: Do arrays
    case type_arr: return 0;
    case type_struct:
        return state->res->structs[lvl.id].align;
    case type_typedef: return comp_get_typealign(state, state->res->typebuf[lvl.id].type.lvls[0]);
    default:
        comp_error(state, "Unknown type: no alignment");
        return 0;
    }
}

static bool comp_get_typesize(comp_state_t *state, uint32_t *ret, const comp_type_lvl_t lvl) {
    switch (lvl.type) {
    case type_b8: case type_c8:
    case type_i8: case type_u8:
        *ret = 1;
        return true;
    case type_u16: case type_i16: case type_c16:
        *ret = 2;
        return true;
    case type_u32: case type_i32: case type_f32:
        *ret = 4;
        return true;
    case type_u64: case type_i64: case type_f64:
        *ret = 8;
        return true;
    case type_ptr: case type_ref:
        *ret = 8;
        return true;
    // These are either offsets into the code segment, or external fn ids,
    // so its always a uint32_t
    case type_pfn:
        *ret = 4;
        return true;
    case type_u0:
        *ret = 0;
        return true;
    // TODO: Do arrays
    case type_arr:
        *ret = 0;
        return false;
    case type_struct:
        *ret = state->res->structs[lvl.id].size;
        return true;
    case type_typedef: return comp_get_typesize(state, ret, state->res->typebuf[lvl.id].type.lvls[0]);
    default:
        comp_error(state, "Unknown type: no size!");
        return 0;
    }
}

static bool comp_is_pod_type(comp_state_t *state, comp_type_lvl_t type) {
    switch (type.type) {
    case type_arr: case type_struct:
        return false;
    case type_typedef:
        return comp_is_pod_type(state, state->res->typebuf[type.id].type.lvls[0]);
    }
    return true;
}

static bool comp_is_arithmetic_type(comp_state_t *state, comp_type_lvl_t type) {
    switch (type.type) {
    case type_u0: case type_ptr: case type_ref: case type_arr:
    case type_pfn: case type_struct:
        return false;
    case type_typedef:
        return comp_is_arithmetic_type(state, state->res->typebuf[type.id].type.lvls[0]);
    }
    return true;
}

static bool comp_is_truthy_type(comp_state_t *state, comp_type_lvl_t type) {
    switch (type.type) {
    case type_u0: case type_arr:
    case type_struct:
        return false;
    case type_typedef:
        return comp_is_truthy_type(state, state->res->typebuf[type.id].type.lvls[0]);
    }
    return true;
}

static comp_type_lvl_t comp_get_type_promotion_single(comp_state_t *state, comp_type_lvl_t lvl) {
    switch (lvl.type) {
    case type_i8: case type_u8: case type_c8: case type_b8:
    case type_i16: case type_u16: case type_c16:
    case type_i32: lvl.type = type_i32; break;
    case type_u32: lvl.type = type_u32; break;
    case type_i64: lvl.type = type_i64; break;
    case type_u64: lvl.type = type_u64; break;
    case type_typedef:
        return comp_get_type_promotion_single(state,state->res->typebuf[lvl.id].type.lvls[0]);
    }
    return lvl;
}

// Does integer/float promotion
// Only works on arithmetic types
static bool comp_get_type_promotion(comp_state_t *state,
                                    comp_type_t left,
                                    comp_type_t right,
                                    comp_type_t *promoted) {
    if (!comp_get_underlying_typedef(state, &left)) return false;
    if (!comp_get_underlying_typedef(state, &right)) return false;
    const comp_type_lvl_t *llvl = left.lvls, *rlvl = right.lvls;
    if (!comp_is_arithmetic_type(state, *llvl)
        || !comp_is_arithmetic_type(state, *rlvl)) return false;
#define check_type(TYPE) \
    if (llvl->type == TYPE || rlvl->type == TYPE) { \
        *promoted = (comp_type_t){ .lvls[0] = (comp_type_lvl_t){.type = TYPE}, .num_lvls = 1 }; \
        return true; \
    }
    check_type(type_f64)
    check_type(type_f32)
    check_type(type_u64)
    check_type(type_i64)
    check_type(type_u32)
#undef check_type
    *promoted = (comp_type_t){ .lvls[0] = (comp_type_lvl_t){.type = type_i32}, .num_lvls = 1 };
    return true;
}

static bool comp_get_literal_type(comp_state_t *state, const ast_t *literal, comp_type_t *type) {
    comp_update_line(state, literal);
    if (literal->type != ast_literal) {
        comp_error(state, "Expected literal!");
        return false;
    }

    *type = (comp_type_t){.num_lvls = 1};
    comp_type_lvl_t *lvl = type->lvls;
    const lex_token_t *tok = &literal->token;
    switch (tok->type) {
    case tok_literal_u:
        if (tok->data.u64 <= INT32_MAX) lvl->type = type_i32;
        else if (tok->data.u64 <= UINT32_MAX) lvl->type = type_u32;
        else if (tok->data.u64 <= INT64_MAX) lvl->type = type_i64;
        else lvl->type = type_u64;
        break;
    case tok_true:
    case tok_false:
        lvl->type = type_b8; break;
    case tok_literal_f:
        lvl->type = type_f64; break;
    case tok_literal_c:
        switch (tok->data.chr.bits) {
        case 8: lvl->type = type_c8; break;
        case 16: lvl->type = type_c16; break;
        case 32: lvl->type = type_c32; break;
        default: return false;
        }
        break;
    default:
        comp_error(state, "Expected literal!");
        return false;
    }
    return true;
}

static bool comp_types_exacteq(comp_state_t *state,
                               bool check_cvrs,
                               comp_type_t a,
                               comp_type_t b) {
    if (!comp_get_underlying_typedef(state, &a)) return false;
    if (!comp_get_underlying_typedef(state, &b)) return false;
    if (a.num_lvls != b.num_lvls) return false;
    for (uint32_t i = 0; i < a.num_lvls; i++) {
        const comp_type_lvl_t *alvl = a.lvls + i, *blvl = b.lvls + i;
        if (check_cvrs && alvl->quals != blvl->quals) return false;
        if (alvl->id != blvl->id || alvl->type != blvl->type) return false;
    }
    return true;
}

// Will edit var in place to reflect what the var is inside
// of the scopevars
static bool comp_add_var_to_scope(comp_state_t *state,
                                  comp_var_t *var,
                                  strview_t name) {
    if (state->scope_top + 1 == state->res->scopevars_len - 1) {
        comp_error(state, "Ran out of scope storage!");
        return false;
    }
    var->lvalue = true;
    var->inscope = true;
    var->name = name;
    var->scope_id = ++state->scope_top;
    state->res->scopevars[state->scope_top] = *var;
    return true;
}

static comp_var_t *comp_get_scopevar(comp_state_t *state,
                                     strview_t name) {
    for (int32_t i = state->scope_top; i > -1; i--) {
        comp_var_t *var = state->res->scopevars + i;
        if (strview_eq(name, var->name)) {
            return var;
        }
    }
    return NULL;
}

static bool comp_get_expr_type_recurr(comp_state_t *state,
                                      comp_type_t *ret, 
                                      const ast_t *expr) {
    comp_update_line(state, expr);
    switch (expr->type) {
    case ast_literal: return comp_get_literal_type(state, expr, ret);
    case ast_op_call: {
        // TODO: Do calls with function pointers...
        assert(expr->a->token.type == tok_ident);
        for (uint32_t i = 0; i < state->num_fns; i++) {
            comp_fn_t *fn = state->res->fns + i;
            if (strview_eq(expr->a->token.data.str, fn->name)) {
                *ret = state->res->typebuf[fn->types_loc + fn->num_types].type;
                return true;
            }
        }
        } break;
    case ast_op_unary: {
        comp_type_t child;
        if (!comp_get_expr_type_recurr(state, &child, expr->child)) return false;
        if (expr->token.type == tok_not) {
            if (!comp_is_truthy_type(state, child.lvls[0])) return false;
            *ret = (comp_type_t){
                .lvls[0] = (comp_type_lvl_t){.type = type_b8},
                .num_lvls = 1,
            };
            return true;
        }
        if (expr->token.type == tok_band) {
            if (ret->num_lvls + 1 > MAX_TYPE_NESTING) {
                comp_error(state, "Hit max type nesting!");
                return false;
            }
            memmove(ret->lvls + 1, ret->lvls, ret->num_lvls++);
            ret->lvls[0] = (comp_type_lvl_t){.type = type_ref};
            return true;
        }
        if (expr->token.type == tok_star) {
            if (ret->lvls[0].type != type_ptr || ret->num_lvls < 2) {
                comp_error(state, "Can only derefrence pointers!");
                return false;
            }
            memmove(ret->lvls, ret->lvls + 1, --ret->num_lvls);
            return true;
        }
        if (!comp_is_arithmetic_type(state, child.lvls[0])) return false;
        *ret = (comp_type_t){
            .lvls[0] = comp_get_type_promotion_single(state, child.lvls[0]),
            .num_lvls = 1,
        };
    } return true;
    case ast_op_ternary: {
        comp_type_t ont, onf;
        if (!comp_get_expr_type_recurr(state, &ont, expr->a)) return false;
        if (!comp_get_expr_type_recurr(state, &onf, expr->b)) return false;
        if (comp_types_exacteq(state, false, ont, onf)) {
            *ret = ont;
            return true;
        }
        if (!comp_is_arithmetic_type(state, onf.lvls[0])
            || !comp_is_arithmetic_type(state, ont.lvls[0])) {
            comp_error(state, "Ternary true and false paths must have the same type!");
            return false;
        }
        if (!comp_get_type_promotion(state, ont, onf, ret)) return false;
    } return true;
    case ast_op_binary: {
        if (expr->token.type == tok_as) {
            comp_type_t left, right;
            if (!comp_get_expr_type_recurr(state, &left, expr->a)) return false;
            if (!comp_get_type(state, expr->b, &right)) return false;
            if (!comp_is_arithmetic_type(state, left.lvls[0])
                || !comp_is_arithmetic_type(state, right.lvls[0])) {
                comp_error(state, "Cast operations must have arithmetic types!");
                return false;
            }
            *ret = right;
            return true;
        }
        if (expr->token.type == tok_dot || expr->token.type == tok_arrow) {
            comp_type_t left;
            if (!comp_get_expr_type_recurr(state, &left, expr->a)) return false;
            if (!comp_get_underlying_typedef(state, &left)) return false;
            if (expr->token.type == tok_dot) {
                if (left.lvls[0].type != type_struct && left.num_lvls != 2) {
                    comp_error(state, "Left hand side of dot operator must be struct.");
                    return false;
                }
                if (left.lvls[0].type != type_struct
                    || left.lvls[0].type != type_ref
                    || left.lvls[1].type != type_struct) {
                    comp_error(state, "Left hand side of dot operator must be struct!");
                    return false;
                }
            } else {
                if (left.lvls[0].type != type_ptr || left.num_lvls != 2
                    || left.lvls[1].type != type_struct) {
                    comp_error(state, "Left hand side of arrow op must be pointer to struct!");
                    return false;
                }
            }
            comp_update_line(state, expr->b);
            if (expr->b->type != ast_ident) {
                comp_error(state, "Right side of dot operator must be identifier!");
                return false;
            }
            comp_struct_t *strct = state->res->structs + left.lvls[left.num_lvls - 1].id;
            for (uint32_t i = strct->type_loc; i < strct->type_loc + strct->num_members; i++) {
                if (strview_eq(expr->b->token.data.str, state->res->typebuf[i].name)) {
                    *ret = state->res->typebuf[i].type;
                    return true;
                }
            }
            comp_error(state, "No member in such struct");
            return false;
        }
        if (expr->token.type == tok_eq) {
            comp_type_t left, right;
            if (!comp_get_expr_type_recurr(state, &left, expr->a)) return false;
            if (!comp_get_expr_type_recurr(state, &right, expr->b)) return false;
            if (comp_types_exacteq(state, false, left, right)) {
                *ret = left;
                return true;
            }
            if (!comp_is_arithmetic_type(state, left.lvls[0])
                || !comp_is_arithmetic_type(state, right.lvls[0])) {
                comp_error(state, "Equal operator must have the same type!");
                return false;
            }

            *ret = left;
            return true;
        }
        if (expr->token.type == tok_pluseq || expr->token.type == tok_boreq
            || expr->token.type == tok_minuseq || expr->token.type == tok_timeseq
            || expr->token.type == tok_diveq || expr->token.type == tok_modeq
            || expr->token.type == tok_lshifteq || expr->token.type == tok_rshifteq
            || expr->token.type == tok_bandeq || expr->token.type == tok_bxoreq) {
            comp_type_t left, right;
            if (!comp_get_expr_type_recurr(state, &left, expr->a)) return false;
            if (!comp_get_expr_type_recurr(state, &right, expr->b)) return false;
            if (!comp_is_arithmetic_type(state, left.lvls[0])
                || !comp_is_arithmetic_type(state, right.lvls[0])) {
                comp_error(state, "Arithmetic equal operators must have arithmetic types!");
                return false;
            }

            *ret = left;
            return true;
        }
        if (expr->token.type == tok_comma) {
            comp_type_t left;
            if (!comp_get_expr_type_recurr(state, &left, expr->a)) return false;
            *ret = left;
            return true;
        }

        comp_type_t left, right;
        if (!comp_get_expr_type_recurr(state, &left, expr->a)) return false;
        if (!comp_get_expr_type_recurr(state, &right, expr->b)) return false;
        comp_update_line(state, expr);
        if (!comp_get_type_promotion(state, left, right, ret)) return false;
    } return true;
    default: comp_error(state, "Expecting expression");
    }
    return false;
}
static bool comp_get_expr_type(comp_state_t *state,
                               comp_type_t *ret, 
                               const ast_t *expr) {
    *ret = (comp_type_t){0};
    return comp_get_expr_type_recurr(state, ret, expr);
}

#endif

