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

    comp_type_lvl_t *lvl = var->type.lvls;
    const lex_token_t *tok = &literal->token;
    switch (tok->type) {
    case tok_literal_u:
        if (tok->data.u64 <= INT32_MAX) {
            lvl->type = type_i32;
            if (!comptime_push(state, 4, 4, &var->loc)) return false;
            int32_t x = tok->data.u64;
            memcpy(state->res->data + var->loc, &x, sizeof(x));
        } else if (tok->data.u64 <= UINT32_MAX) {
            lvl->type = type_u32;
            if (!comptime_push(state, 4, 4, &var->loc)) return false;
            uint32_t x = tok->data.u64;
            memcpy(state->res->data + var->loc, &x, sizeof(x));
        } else if (tok->data.u64 <= INT64_MAX) {
            lvl->type = type_i64;
            if (!comptime_push(state, 8, 8, &var->loc)) return false;
            int64_t x = tok->data.u64;
            memcpy(state->res->data + var->loc, &x, sizeof(x));
        } else {
            lvl->type = type_u64;
            if (!comptime_push(state, 8, 8, &var->loc)) return false;
            uint64_t x = tok->data.u64;
            memcpy(state->res->data + var->loc, &x, sizeof(x));
        }
        break;
    case tok_true:
    case tok_false:
        lvl->type = type_b8;
        if (!comptime_push(state, 1, 1, &var->loc)) return false;
        uint8_t x = tok->type == tok_true ? 1 : 0;
        memcpy(state->res->data + var->loc, &x, sizeof(x));
        break;
    case tok_literal_f:
        lvl->type = type_f64;
        if (!comptime_push(state, 8, 8, &var->loc)) return false;
        memcpy(state->res->data + var->loc, &tok->data.f64, sizeof(tok->data.f64));
        break;
    case tok_literal_c:
        switch (tok->data.chr.bits) {
        case 8: {
            lvl->type = type_c8;
            if (!comptime_push(state, 1, 1, &var->loc)) return false;
            int8_t x = tok->data.chr.c32;
            memcpy(state->res->data + var->loc, &x, sizeof(x));
        } break;
        case 16: {
            lvl->type = type_c16;
            if (!comptime_push(state, 2, 2, &var->loc)) return false;
            int16_t x = tok->data.chr.c32;
            memcpy(state->res->data + var->loc, &x, sizeof(x));
        } break;
        case 32: {
            lvl->type = type_c32;
            if (!comptime_push(state, 4, 4, &var->loc)) return false;
            memcpy(state->res->data + var->loc, &tok->data.chr.c32, sizeof(tok->data.chr.c32));
        } break;
        }
        break;
    default:
        comp_error(state, "Expected literal!");
        return false;
    }
    return true;
}

// Will cast to a type of size bigger than the previous if needed
// Will in place edit var to reflect the new type
// ONLY works on latest push object into the data segment!!!
static bool comptime_cast(comp_state_t *state,
                          comp_var_t *var,
                          const comp_type_t *type) {
    comp_type_t typeto = *type, typefrom = var->type;
    if (!comp_get_underlying_typedef(state, &typeto)) return false;
    if (!comp_get_underlying_typedef(state, &typefrom)) return false;

    const comp_type_lvl_t *tolvl = typeto.lvls;
    comp_type_lvl_t *fromlvl = typefrom.lvls;

    // TODO: Arrays
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

    var->type = typeto;
#define conv_to(TYID, TYTO) \
case TYID: \
    if (!comptime_push(state, sizeof(TYTO), alignof(TYTO), &var->loc)) return false; \
    *(TYTO *)(state->res->data + var->loc) = (TYTO)x; \
    return true;
#define from_type(TYID, TYFROM) \
case TYID: { \
    const TYFROM x = *(TYFROM *)(state->res->data + var->loc); \
    state->dsz = var->loc; \
    switch (tolvl->type) { \
    conv_to(type_i8, int8_t) \
    case type_b8: case type_c8: conv_to(type_u8, uint8_t) \
    conv_to(type_i16, int16_t) \
    case type_c16: conv_to(type_u16, uint16_t) \
    conv_to(type_i32, int32_t) \
    case type_c32: conv_to(type_u32, uint32_t) \
    conv_to(type_i64, int64_t) \
    conv_to(type_u64, uint64_t) \
    conv_to(type_f32, float) \
    conv_to(type_f64, double) \
    default: return false; \
    } \
}
    switch (fromlvl->type) {
    from_type(type_i8, int8_t)
    case type_b8: case type_c8: from_type(type_u8, uint8_t)
    from_type(type_i16, int16_t)
    case type_c16: from_type(type_u16, uint16_t)
    from_type(type_i32, int32_t)
    case type_c32: from_type(type_u32, uint32_t)
    from_type(type_i64, int64_t)
    from_type(type_u64, uint64_t)
    from_type(type_f32, float)
    from_type(type_f64, double)
    default: return false;
    }
#undef from_type
#undef conv_to

    return true;
}

// Expects both sides to be of the same type
static bool comptime_op_binary(comp_state_t *state,
                               comp_var_t *ret,
                               comp_var_t *left, comp_var_t *right,
                               const ast_t *leftast, const ast_t *rightast) {
    return true;
}

// Compile time expressions use the data segment as a make shift stack.
static bool comptime_expr(comp_state_t *state,
                          bool errnocomp,
                          comp_var_t *ret,
                          const ast_t *expr) {
    comp_update_line(state, expr);
    switch (expr->type) {
    case ast_literal:
        
        return true;
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

        // The rest from here on out is arithmetic operations
        comp_type_t tyleft, tyright;
        if (!comp_get_expr_type(state, &tyleft, expr->a)
            || !comp_get_expr_type(state, &tyright, expr->b)) return false;
        if (!comp_is_arithmetic_type(state, tyleft.lvls[0])
            || !comp_is_arithmetic_type(state, tyright.lvls[0])) {
            comp_error(state, "Arithmetic operations must occur on arithmetic types!");
            return false;
        }

        comp_var_t left, right;
        if (!comptime_expr(state, errnocomp, &left, expr->a)
            || !comptime_expr(state, errnocomp, &right, expr->b)) return false;
        if (!comptime_op_binary(state, ret, &left, &right, expr->a, expr->b)) return false;

        comp_error(state, "Can not manipulate state in a compile time expression!");
        return false;
    }
    default:
        comp_error(state, "Expecting expression");
        return false;
    }
}

