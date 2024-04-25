#include "comp_common.h"

static bool comptime_push(comp_state_t *state,
                          uint32_t size,
                          uint32_t align,
                          uint32_t *pos) {
    uint32_t aligninc = state->dsz % align ? (align - (state->dsz % align)) : 0;
    if (state->dsz + aligninc + size > state->res->data_len) {
        *pos = UINT32_MAX;
        comp_error(state,
            "Ran out of data segment room!\n"
            "Note: compile time expressions use data as the stack.");
        return false;
    }
    *pos = state->dsz + aligninc;
    state->dsz += size + aligninc;
    return true;
}

#define comptime_push_template(TYNAME, TYPE) \
static bool comptime_push_##TYNAME(comp_state_t *state, uint32_t *loc, TYPE i) { \
    if (!comptime_push(state, sizeof(TYPE), alignof(TYPE), loc)) return false; \
    *(TYPE *)(state->res->data + *loc) = i; \
    return true; \
}
comptime_push_template(i8, int8_t)
comptime_push_template(b8, uint8_t)
comptime_push_template(u8, uint8_t)
comptime_push_template(c8, uint8_t)
comptime_push_template(i16, int16_t)
comptime_push_template(u16, uint16_t)
comptime_push_template(c16, uint16_t)
comptime_push_template(i32, int32_t)
comptime_push_template(u32, uint32_t)
comptime_push_template(c32, uint32_t)
comptime_push_template(i64, int64_t)
comptime_push_template(u64, uint64_t)
comptime_push_template(f32, float)
comptime_push_template(f64, double)
#undef comptime_push_template
#define comptime_pop_template(TYNAME, TYPE) \
static TYPE comptime_pop_##TYNAME(comp_state_t *state, uint32_t loc) { \
    state->dsz = loc; \
    return *(TYPE *)(state->res->data + state->dsz); \
}
comptime_pop_template(i8, int8_t)
comptime_pop_template(b8, uint8_t)
comptime_pop_template(u8, uint8_t)
comptime_pop_template(c8, uint8_t)
comptime_pop_template(i16, int16_t)
comptime_pop_template(u16, uint16_t)
comptime_pop_template(c16, uint16_t)
comptime_pop_template(i32, int32_t)
comptime_pop_template(u32, uint32_t)
comptime_pop_template(c32, uint32_t)
comptime_pop_template(i64, int64_t)
comptime_pop_template(u64, uint64_t)
comptime_pop_template(f32, float)
comptime_pop_template(f64, double)
#undef comptime_pop_template

static bool comptime_push_literal_type(comp_state_t *state, const ast_t *literal, comp_var_t *var) {
    comp_update_line(state, literal);
    if (literal->type != ast_literal) {
        comp_error(state, "Expected literal!");
        return false;
    }

    *var = (comp_var_t){
        .type.num_lvls = 1,
        .loctype = var_loc_data,
        .lvalue = false,
        .inscope = false,
        .scope_id = UINT32_MAX,
    };

    const lex_token_t *tok = &literal->token;
    if (!comp_get_literal_type(state, literal, &var->type)) return false;
    switch (var->type.lvls[0].type) {
    case type_i32: return comptime_push_i32(state, &var->loc, tok->data.u64);
    case type_u32: return comptime_push_u32(state, &var->loc, tok->data.u64);
    case type_i64: return comptime_push_i64(state, &var->loc, tok->data.u64);
    case type_u64: return comptime_push_u64(state, &var->loc, tok->data.u64);
    case type_f64: return comptime_push_f64(state, &var->loc, tok->data.f64);
    case type_c8: return comptime_push_c8(state, &var->loc, tok->data.chr.c32);
    case type_c16: return comptime_push_c16(state, &var->loc, tok->data.chr.c32);
    case type_c32: return comptime_push_c32(state, &var->loc, tok->data.chr.c32);
    case type_b8: return comptime_push_b8(state, &var->loc, tok->type == tok_true);
    default: comp_error(state, "Unknown literal type!");
    }
    return false;
}

// Will cast to a type of size bigger than the previous if needed
// Will in place edit var to reflect the new type
// ONLY works on latest push object into the data segment!!!
// newloc is used as where the last variable was on the stack (negating the
// alignment of the variable its casting from)
static bool comptime_cast(comp_state_t *state,
                          comp_var_t *var,
                          const comp_type_t *type,
                          uint32_t newloc) {
    comp_type_t typeto = *type, typefrom = var->type;
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

    assert(var->loctype == var_loc_data);
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

#define from_type(TYID, TYFROM) \
case type_##TYID: { \
    const TYFROM x = comptime_pop_##TYID(state, var->loc); \
    state->dsz = newloc; \
    switch (tolvl->type) { \
    case type_i8: \
        if (!comptime_push_i8(state, &var->loc, x)) return false; \
        else return true; \
    case type_b8: case type_c8: case type_u8: \
        if (!comptime_push_u8(state, &var->loc, x)) return false; \
        else return true; \
    case type_i16: \
        if (!comptime_push_i16(state, &var->loc, x)) return false; \
        else return true; \
    case type_c16: case type_u16: \
        if (!comptime_push_u16(state, &var->loc, x)) return false; \
        else return true; \
    case type_i32: \
        if (!comptime_push_i32(state, &var->loc, x)) return false; \
        else return true; \
    case type_c32: case type_u32: \
        if (!comptime_push_u32(state, &var->loc, x)) return false; \
        else return true; \
    case type_i64: \
        if (!comptime_push_i64(state, &var->loc, x)) return false; \
        else return true; \
    case type_u64: \
        if (!comptime_push_u64(state, &var->loc, x)) return false; \
        else return true; \
    case type_f32: \
        if (!comptime_push_f32(state, &var->loc, x)) return false; \
        else return true; \
    case type_f64: \
        if (!comptime_push_f64(state, &var->loc, x)) return false; \
        else return true; \
    default: return false; \
    } \
}
    var->type = typeto;
    switch (fromlvl->type) {
    from_type(i8, int8_t)
    case type_b8: case type_c8: from_type(u8, uint8_t)
    from_type(i16, int16_t)
    case type_c16: from_type(u16, uint16_t)
    from_type(i32, int32_t)
    case type_c32: from_type(u32, uint32_t)
    from_type(i64, int64_t)
    from_type(u64, uint64_t)
    from_type(f32, float)
    from_type(f64, double)
    default: return false;
    }
#undef from_type
#undef conv_to

    return true;
}

#define binary_op_type(OP, TYPE, NAME) \
    case type_##NAME: { \
        TYPE bx = comptime_pop_##NAME(state, b->loc); \
        TYPE ax = comptime_pop_##NAME(state, a->loc); \
        comptime_push_##NAME(state, &ret->loc, ax OP bx); \
        } return true;
#define binary_float_ops(OP) \
        binary_op_type(OP, float, f32) \
        binary_op_type(OP, double, f64)
#define binary_op_template(OP, TOK) \
    case TOK: { \
        switch (a->type.lvls[0].type) { \
        binary_op_type(OP, int8_t, i8) \
        binary_op_type(OP, uint8_t, u8) \
        binary_op_type(OP, uint8_t, b8) \
        binary_op_type(OP, uint8_t, c8) \
        binary_op_type(OP, int16_t, i16) \
        binary_op_type(OP, uint16_t, u16) \
        binary_op_type(OP, uint16_t, c16) \
        binary_op_type(OP, int32_t, i32) \
        binary_op_type(OP, uint32_t, u32) \
        binary_op_type(OP, uint32_t, c32) \
        binary_op_type(OP, int64_t, i64) \
        binary_op_type(OP, uint64_t, u64) \
        binary_float_ops(OP) \
        default: break; \
        } \
    }
// Expects both sides to be of the same type
static bool comptime_op_binary(comp_state_t *state,
                               lex_token_type_t op,
                               comp_var_t *ret,
                               comp_var_t *a, comp_var_t *b) {
    *ret = *a;
    switch (op) {
    binary_op_template(+, tok_plus)
    binary_op_template(-, tok_minus)
    binary_op_template(*, tok_star)
    binary_op_template(/, tok_slash)
#undef binary_float_ops
#define binary_float_ops(OP)
    binary_op_template(<<, tok_lshift)
    binary_op_template(<<, tok_rshift)
    binary_op_template(|, tok_bor)
    binary_op_template(^, tok_bxor)
    binary_op_template(&, tok_band)
    default: break;
    }
    return false;
}
#undef binary_float_ops
#undef binary_op_template
#undef binary_op_type

static bool comptime_typed_expr(comp_state_t *state,
                                bool errnocomp,
                                comp_var_t *ret,
                                const ast_t *expr,
                                const comp_type_t *deftype);
static bool comptime_expr_init_struct(comp_state_t *state,
                                      bool errnocomp,
                                      comp_var_t *ret,
                                      const ast_t *expr,
                                      comp_struct_t *strct) {
    comp_update_line(state, expr);
    *ret = (comp_var_t){
        .loc = state->dsz,
        .loctype = var_loc_data,
        .scope_id = UINT32_MAX,
        .type = (comp_type_t){
            .lvls[0] = (comp_type_lvl_t){
                .type = type_struct,
                .id = strct - state->res->structs,
            },
            .num_lvls = 1,
        },
    };

    {
        uint32_t aligndsz = alignu(state->dsz, strct->align);
        if (aligndsz > state->res->data_len) {
            comp_error(state, "Ran out of data segment space!");
            return false;
        }
        state->dsz = aligndsz;
    }
    uint32_t currsz = 0;
    for (uint32_t i = strct->type_loc; i < strct->type_loc + strct->num_members; i++) {
        comp_typebuf_t *tybuf = state->res->typebuf + i;
        
        const ast_t *init = expr->child;
        for (; init; init = init->next) {
            comp_update_line(state, init);
            if (strview_eq(init->token.data.str, tybuf->name)) break;
        }

        if (init) {
            comp_var_t tmp;
            if (!comptime_typed_expr(state, errnocomp, &tmp, init->child, &tybuf->type)) return false;
            currsz += state->dsz - tmp.loc;
        }

        uint32_t needsz = i+1 == strct->type_loc + strct->num_members
                          ? strct->size
                          : state->res->typebuf[i+1].offs;
        if (state->dsz + (needsz - currsz) > state->res->data_len) {
            comp_error(state, "Ran out of data segement space!");
            return false;
        }
        memset(state->res->data + state->dsz, 0, needsz - currsz);
        state->dsz += needsz - currsz;
        currsz = needsz;
    }

    return true;
}
// Compile time expressions use the data segment as a make shift stack.
static bool comptime_expr(comp_state_t *state,
                          bool errnocomp,
                          comp_var_t *ret,
                          const ast_t *expr,
                          const comp_type_t *arrtype) {
    comp_update_line(state, expr);
    switch (expr->type) {
    case ast_init_struct: {
        for (uint32_t t = 0; t < state->num_typedefs; t++) {
            comp_typebuf_t *typebuf = state->res->typebuf + state->res->typedefs[t];
            if (strview_eq(expr->token.data.str, typebuf->name) && typebuf->type.lvls[0].type == type_struct) {
                return comptime_expr_init_struct(state, errnocomp, ret, expr, state->res->structs + typebuf->type.lvls[0].id);
            }
        }
        for (uint32_t i = 0; i < state->num_structs; i++) {
            comp_struct_t *strct = state->res->structs + i;
            if (strview_eq(expr->token.data.str, strct->name)) {
                return comptime_expr_init_struct(state, errnocomp, ret, expr, strct);
            }
        }
        comp_error(state, "Unknown struct!");
        return false;
    }
    case ast_init_array: {
        assert(((expr = expr->child) || !arrtype) && "Array init must have initializers!");
        comp_type_t innerty;
        if (arrtype) {
            innerty = *arrtype;
            if (!comp_remove_type_lvls(state, &innerty, 1)) return false;
        }
        else if (!comp_get_expr_type(state, &innerty, expr)) return false;
        *ret = (comp_var_t){
            .loc = state->dsz,
            .inscope = false,
            .lvalue = false,
            .scope_id = UINT32_MAX,
            .loctype = var_loc_data,
            .type = (comp_type_t){
                .lvls[0].type = type_arr,
                .num_lvls = 1,
            },
        };
        if (ret->type.num_lvls + innerty.num_lvls > MAX_TYPE_NESTING) {
            comp_error(state, "Hit max type nesting");
            return false;
        }
        memcpy(ret->type.lvls + 1, innerty.lvls, sizeof(innerty.lvls[0]) * innerty.num_lvls);
        ret->type.num_lvls += innerty.num_lvls;
        for (; expr; expr = expr->next) {
            comp_update_line(state, expr);
            if (!comptime_typed_expr(state, errnocomp, &(comp_var_t){0}, expr, &innerty)) return false;
            ret->type.lvls[0].id++;
        }
        if (arrtype) {
            uint32_t innersize, diff;
            if (!comp_get_typesize(state, &innersize, *arrtype, 1)) return false;
            diff = arrtype->lvls[0].id - ret->type.lvls[0].id;
            if (state->dsz + innersize * diff > state->res->data_len) {
                comp_error(state, "Array runs out of data segment storage!");
                return false;
            }
            uint32_t nbytes = innersize * diff;
            memset(state->res->data + state->dsz, 0, nbytes);
            state->dsz += nbytes;
        }
    } return true;
    case ast_ident: {
        for (uint32_t i = 0; i < state->res->scopes[state->scopes_top].scope_base; i++) {
            comp_var_t *var = state->res->scopevars + i;
            if (strview_eq(var->name, expr->token.data.str)) {
                if (~var->type.lvls[0].quals & qual_const) {
                    comp_error(state, "Can not use non-const variable in comptime expression.");
                    return false;
                }

                uint32_t aligndsz = alignu(state->dsz, comp_get_typealign(state, var->type, 0));
                if (!aligndsz) return false;
                if (aligndsz > state->res->data_len) {
                    comp_error(state, "Ran out of data segment space.");
                    return false;
                }
                state->dsz = aligndsz;
                uint32_t size;
                if (!comp_get_typesize(state, &size, var->type, 0)) return false;
                if (state->dsz + size > state->res->data_len) {
                    comp_error(state, "Ran out of data segment space");
                    return false;
                }

                // Copy the data over since never can you edit a const variable anyway
                *ret = (comp_var_t){
                    .loc = state->dsz,
                    .inscope = false,
                    .lvalue = false,
                    .scope_id = UINT32_MAX,
                    .loctype = var_loc_data,
                    .type = var->type,
                };
                memcpy(state->res->data + state->dsz, state->res->data + var->loc, size);
                state->dsz += size;
                return true;
            }
        }
        comp_error(state, "Identifier not found");
    } return false;
    case ast_literal:
        if (!comptime_push_literal_type(state, expr, ret)) return false;
        return true;
    case ast_op_unary:
        if (expr->token.type == tok_minus) {
            comp_type_t innerty;
            if (!comp_get_expr_type(state, &innerty, expr->child)) return false;
            if (!comp_is_arithmetic_type(state, innerty.lvls[0])) {
                comp_error(state, "Expected arithmetic type for unary '-'");
                return false;
            }
            innerty.lvls[0] = comp_get_type_promotion_single(state, innerty.lvls[0]);

            const uint32_t loc = state->dsz;
            if (!comptime_expr(state, errnocomp, ret, expr->child, NULL)) return false;
            if (!comptime_cast(state, ret, &innerty, loc)) return false;
            switch (ret->type.lvls[0].type) {
            case type_i32: *(int32_t *)(state->res->data + ret->loc) *= -1; break;
            case type_u32: {
                uint32_t x = comptime_pop_u32(state, ret->loc);
                
                if (x <= (uint32_t)INT32_MAX+1) {
                    innerty.lvls[0].type = type_i32;
                    ret->type.lvls[0].type = type_i32;
                    if (!comptime_push_i32(state, &ret->loc, x == (uint32_t)INT32_MAX+1 ? INT32_MIN : -(int32_t)x)) {
                        return false;
                    }
                } else {
                    innerty.lvls[0].type = type_i64;
                    ret->type.lvls[0].type = type_i64;
                    if (!comptime_push_i64(state, &ret->loc, -(int64_t)x)) {
                        return false;
                    }
                }
            } return true;
            case type_i64: *(int64_t *)(state->res->data + ret->loc) *= -1; break;
            case type_u64: {
                uint64_t x = comptime_pop_u64(state, ret->loc);
                innerty.lvls[0].type = type_i64;
                ret->type.lvls[0].type = type_i64;
                if (!comptime_push_i64(state, &ret->loc, -(int64_t)x)) return false;
            } return true;
            case type_f32: *(float *)(state->res->data + ret->loc) *= -1.0f; break;
            case type_f64: *(double *)(state->res->data + ret->loc) *= -1.0f; break;
            default: assert(false && "Expected arithemtic type comptime '-'");
            }
            return true;
        }
        assert(false && "TODO: unary operators for compile time expressions!");
        break;
    case ast_op_call:
        if (errnocomp) comp_error(state, "Can not call functions in a compile time expression!");
        return false;
    case ast_op_binary: {
        if (expr->token.type == tok_arrow) {
            if (errnocomp) comp_error(state, "Can not use pointers in compile time expressions!");
            return false;
        }
        if (expr->token.type == tok_dot) {
            assert(false && "TODO: dot operator in compile time expressions!");
        }
        if (expr->token.type == tok_comma) {
            assert(false && "TODO: comma operator in compile time expressions!");
        }
        if (expr->token.type == tok_as) {
            comp_type_t totype;
            const uint32_t loc = state->dsz;
            if (!comp_get_type(state, expr->b, &totype)) return false;
            if (!comptime_expr(state, true, ret, expr->a, NULL)) return false;
            if (!comptime_cast(state, ret, &totype, loc)) return false;
            return true;
        }

        // The rest from here on out is arithmetic operations
        comp_type_t tyleft, tyright;
        if (!comp_get_expr_type(state, &tyleft, expr->a)
            || !comp_get_expr_type(state, &tyright, expr->b)) return false;
        comp_type_t commonty;
        if (!comp_get_type_promotion(state, tyleft, tyright, &commonty)) {
            if (errnocomp) comp_error(state, "Can not manipulate state in a compile time expression!");
            //comp_error(state, "Arithmetic operations must occur on arithmetic types!");
            return false;
        }

        comp_var_t left, right;
        uint32_t loc = state->dsz;
        if (!comptime_expr(state, errnocomp, &left, expr->a, NULL)) return false;
        if (!comptime_cast(state, &left, &commonty, loc)) return false;
        loc = state->dsz;
        if (!comptime_expr(state, errnocomp, &right, expr->b, NULL)) return false;
        if (!comptime_cast(state, &right, &commonty, loc)) return false;
        assert(comp_types_exacteq(state, false, 0, left.type, right.type));
        if (!comptime_op_binary(state, expr->token.type, ret, &left, &right)) return false;
        return true;
    }
    default:
        comp_error(state, "Expecting expression");
        return false;
    }
}

// Compile an expression that expects a certain type
static bool comptime_typed_expr(comp_state_t *state,
                                bool errnocomp,
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
        if (state->dsz + size > state->res->data_len) {
            comp_error(state, "Ran out of storage in data segment!");
            return false;
        }

        assert(false && "TODO: Undefined typed vars!");
    } else {
        const uint32_t loc = state->dsz;
        if (deftype && deftype->lvls[0].type == type_arr) {
            //assert(comp_remove_type_lvls(state, &innerty, 1));
            if (!comptime_expr(state, true, ret, expr, deftype)) {
                comp_error(state, "Expected compile time expression!");
                return false;
            }
        } else if (!comptime_expr(state, true, ret, expr, NULL)) {
            comp_error(state, "Expected compile time expression!");
            return false;
        }
        if (deftype && !comptime_cast(state, ret, deftype, loc)) return false;
    }

    return true;
}

