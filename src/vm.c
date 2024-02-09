#include <assert.h>
#include <stdio.h>

#include "vm.h"

// Size assumptions (not good?)
static_assert(sizeof(unit_t) % sizeof(opcode_t) == 0);
static_assert(sizeof(codeunit_t) == sizeof(opcode_t));
static_assert(sizeof((unit_t){0}.i32) / sizeof(opcode_t) == 1);

static inline void store_mem(const typed_unit_t data, void *const addr) {
	switch (data.ty) {
	case unit_i8:
	case unit_u8:
		*(uint8_t *)addr = data.unit.u8;
		break;
	case unit_i16:
	case unit_u16:
		*(uint16_t *)addr = data.unit.u16;
		break;
	case unit_i32:
	case unit_u32:
		*(uint32_t *)addr = data.unit.u32;
		break;
	case unit_i64:
	case unit_u64:
	case unit_ptr:
		*(uint64_t *)addr = data.unit.u64;
		break;
	default:
		assert(false && "unknown unit type");
	}
}
static inline unit_t extract_mem(unitty_t ty, const void *const addr) {
	unit_t unit;
	switch (ty) {
	case unit_i8:
	case unit_u8:
		unit.u8 = *(const uint8_t *)addr;
		break;
	case unit_i16:
	case unit_u16:
		unit.u16 = *(const uint16_t *)addr;
		break;
	case unit_i32:
	case unit_u32:
		unit.u32 = *(const uint32_t *)addr;
		break;
	case unit_i64:
	case unit_u64:
	case unit_ptr:
		unit.u64 = *(const uint64_t *)addr;
		break;
	default:
		assert(false && "unknown unit type");
	}
	return unit;
}
static inline unit_t extend_unit(const unit_t unit, unitty_t from, unitty_t to) {
	switch(to) {
	case unit_i16:
		switch (from) {
		case unit_i8: return (unit_t){ .i16 = unit.i8 };
		default: return unit;
		}
	case unit_i32:
		switch (from) {
		case unit_i8: return (unit_t){ .i32 = unit.i8 };
		case unit_i16: return (unit_t){ .i32 = unit.i16 };
		default: return unit;
		}
	case unit_i64:
		switch (from) {
		case unit_i8: return (unit_t){ .i64 = unit.i8 };
		case unit_i16: return (unit_t){ .i64 = unit.i16 };
		case unit_i32: return (unit_t){ .i64 = unit.i32 };
		default: return unit;
		}
	default: return unit;
	}
}
static inline unit_t extract_imm(
	const opcode_t opcode,
	const codeunit_t **const code
) {
	switch (opcode.op1ty) {
	case unit_i8:
		return (unit_t){ .i64 = opcode.imm8 };
	case unit_u8:
		return (unit_t){ .u64 = opcode.umm8 };
	case unit_i16:
		return (unit_t){ .i64 = opcode.imm16 };
	case unit_u16:
		return (unit_t){ .u64 = opcode.umm16 };
	case unit_i32:
		return (unit_t){ .i64 = ((*code)++)->i32 };
	case unit_u32:
		return (unit_t){ .u64 = ((*code)++)->u32 };
	case unit_i64:
	case unit_u64:
	case unit_ptr:
		// FIXME: Only works with little endian 64bit...
		{
			unit_t unit = (unit_t){ .u64 = ((*code)++)->u32 };
			unit.u64 |= (uint64_t)(((*code)++))->u32 << 32;
			return unit;
		}
	default:
		assert(false && "unknown unit type");
	}
}

typedef struct state {
	const codeunit_t *code;
	unit_t *stack;
	const size_t stacklen;
	const codeunit_t *const code_start;
	const unit_t *const stack_start;
	unit_t cmp_left, cmp_right;
	unitty_t cmp_ty;
} state_t;

static typed_unit_t _interpret(state_t state);
typed_unit_t interpret(const codeunit_t *code, unit_t *stack, size_t stacklen) {
	return _interpret((state_t){
		.code = code,
		.stacklen = stacklen,
		.code_start = code,
		.stack_start = stack + stacklen - 1,
		.stack = stack + stacklen - 1,
	});
}
static typed_unit_t _interpret(state_t state) {
	while (true) {
		const opcode_t opcode = (state.code++)->op;
		switch (opcode.ty) {
		case opcode_pushimm:
			*(--state.stack) = extract_imm(opcode, &state.code);
			break;
		case opcode_pushunit:
			state.stack--;
			*state.stack = state.stack[extract_imm(opcode, &state.code).u32 + 1];
			break;
		case opcode_pop:
			state.stack += opcode.op1;
			break;
		case opcode_call:
			*(--state.stack) = (unit_t){ .ptr = (uintptr_t)state.code };
			state.code = state.code_start + extract_imm(opcode, &state.code).u64;
			break;
		case opcode_ret:
			{
				const unit_t unit = state.stack[opcode.op1];
				state.code = (const codeunit_t *const)state.stack[opcode.op2].ptr;
				state.stack += opcode.op2 + 1;
				if (state.stack >= state.stack_start - 1) return (typed_unit_t){ .ty = opcode.op1ty, .unit = unit };
				else *(--state.stack) = unit;
			}
			break;
		case opcode_exit:
			return (typed_unit_t){ .ty = opcode.op1ty, .unit = state.stack[opcode.op1] };
			break;
		case opcode_loadunit:
			{
				const unit_t unit = state.stack[extract_imm(opcode, &state.code).u32];
				*(--state.stack) = unit;
			}
			break;
		case opcode_loadmem:
			*(--state.stack) = extract_mem(opcode.op1ty, (void *)extract_imm(opcode, &state.code).ptr);
			break;
		case opcode_moveunit:
			state.stack[opcode.op2] = state.stack[opcode.op1];
			break;
		case opcode_storemem:
			store_mem((typed_unit_t){ .ty = opcode.op2ty, .unit = *state.stack }, (void *)extract_imm(opcode, &state.code).ptr);
			break;

#define BRANCH(condname, cond) \
	case opcode_##condname: \
		switch (state.cmp_ty) { \
		case unit_i32: if (state.cmp_left.i32 cond state.cmp_right.i32) state.code += extract_imm(opcode, &state.code).i32; break; \
		case unit_u32: if (state.cmp_left.u32 cond state.cmp_right.u32) state.code += extract_imm(opcode, &state.code).i32; break; \
		case unit_i64: if (state.cmp_left.i64 cond state.cmp_right.i64) state.code += extract_imm(opcode, &state.code).i32; break; \
		case unit_u64: if (state.cmp_left.u64 cond state.cmp_right.u64) state.code += extract_imm(opcode, &state.code).i32; break; \
		default: assert(false && "vm integer promotion branch error"); \
		} \
		break;
		BRANCH(beq, ==)
		BRANCH(bne, !=)
		BRANCH(bgt, >)
		BRANCH(blt, <)
		BRANCH(bge, >=)
		BRANCH(ble, <=)
		case opcode_bra:
			state.code += extract_imm(opcode, &state.code).i16;
			break;
		case opcode_jmp:
			state.code = state.code_start + extract_imm(opcode, &state.code).u64;
			break;
		case opcode_cmp:
			state.cmp_left = state.stack[opcode.op1];
			state.cmp_right = state.stack[opcode.op2];
			state.cmp_ty = opcode.op1ty;
			break;
		case opcode_tst:
			state.cmp_left = state.stack[opcode.op1];
			state.cmp_right = (unit_t){0};
			state.cmp_ty = opcode.op1ty;
			break;

#define BINARY_INTOP(opname, opsymb) \
		case opcode_##opname: \
			assert(opcode.op1ty == opcode.op2ty); \
			{ \
				unit_t unit; \
				switch (opcode.op1ty) { \
				case unit_i32: unit = (unit_t){ .i32 = state.stack[opcode.op1].i32 opsymb state.stack[opcode.op2].i32 }; break; \
				case unit_u32: unit = (unit_t){ .u32 = state.stack[opcode.op1].u32 opsymb state.stack[opcode.op2].u32 }; break; \
				case unit_i64: unit = (unit_t){ .i64 = state.stack[opcode.op1].i64 opsymb state.stack[opcode.op2].i64 }; break; \
				case unit_u64: unit = (unit_t){ .u64 = state.stack[opcode.op1].u64 opsymb state.stack[opcode.op2].u64 }; break; \
				default: assert(false && "vm integer binary operation error"); \
				} \
				*(--state.stack) = unit; \
			} \
			break;
		BINARY_INTOP(add, +)
		BINARY_INTOP(sub, -)
		BINARY_INTOP(mul, *)
		BINARY_INTOP(div, /)
		BINARY_INTOP(mod, %)
		BINARY_INTOP(shl, <<)
		BINARY_INTOP(shr, >>)

#define UNARY_INTOP(opname, opsymb) \
		case opcode_##opname: \
			{ \
				unit_t unit; \
				switch (opcode.op1ty) { \
				case unit_i32: unit = (unit_t){ .i32 = -state.stack[opcode.op1].i32 }; break; \
				case unit_i64: unit = (unit_t){ .i64 = -state.stack[opcode.op1].i64 }; break; \
				default: assert(false && "vm integer unary operation error"); \
				} \
				*(--state.stack) = unit; \
			} \
			break;
		UNARY_INTOP(and, &)
		UNARY_INTOP(or, |)
		UNARY_INTOP(eor, ^)
		UNARY_INTOP(not, ~)
		case opcode_neg:
			switch (opcode.op1ty) {
			case unit_i32: state.stack[opcode.op1].i32 = -state.stack[opcode.op1].i32; break;
			case unit_i64: state.stack[opcode.op1].i64 = -state.stack[opcode.op1].i64; break;
			default: assert(false && "vm integer negation error");
			}
			break;
		case opcode_inc:
			switch (opcode.op2ty) {
			case unit_i32: state.stack[extract_imm(opcode, &state.code).u64].i32++; break;
			case unit_u32: state.stack[extract_imm(opcode, &state.code).u64].u32++; break;
			case unit_i64: state.stack[extract_imm(opcode, &state.code).u64].i64++; break;
			case unit_u64: state.stack[extract_imm(opcode, &state.code).u64].u64++; break;
			default: assert(false && "vm integer inc error");
			}
			break;
		case opcode_dec:
			switch (opcode.op2ty) {
			case unit_i32: state.stack[extract_imm(opcode, &state.code).u64].i32--; break;
			case unit_u32: state.stack[extract_imm(opcode, &state.code).u64].u32--; break;
			case unit_i64: state.stack[extract_imm(opcode, &state.code).u64].i64--; break;
			case unit_u64: state.stack[extract_imm(opcode, &state.code).u64].u64--; break;
			default: assert(false && "vm integer dec error");
			}
			break;
		case opcode_ext:
			state.stack[opcode.op1] = extend_unit(state.stack[opcode.op1], opcode.op1ty, opcode.op2ty);
			break;
		default:
			assert(false && "unimplemented opcode");
		}
	}

	return (typed_unit_t){ .ty = unit_i32, .unit = *(--state.stack) };
}

static const char *unitty_to_str(const unitty_t ty) {
	switch (ty) {
#define enumdef(e) case e: return #e;
unitty_def
#undef enumdef
	default: return "ERR";
	}
}

void dbg_typed_unit_print(const typed_unit_t unit) {
	printf("%s: ", unitty_to_str(unit.ty));
	switch (unit.ty) {
	case unit_i8: printf("%d", unit.unit.i8); break;
	case unit_i16: printf("%d", unit.unit.i16); break;
	case unit_i32: printf("%d", unit.unit.i32); break;
	case unit_i64: printf("%lld", unit.unit.i64); break;
	case unit_u8: printf("%u", unit.unit.i8); break;
	case unit_u16: printf("%u", unit.unit.i16); break;
	case unit_u32: printf("%u", unit.unit.i32); break;
	case unit_u64: printf("%llu", unit.unit.i64); break;
	case unit_ptr: printf("%p", (void *)unit.unit.ptr); break;
	default: printf("unknown"); break;
	}
	printf("\n");
}

static const char *opcode_to_str(const opcodety_t ty) {
	switch (ty) {
#define enumdef(e) case e: return #e;
opcodety_def
#undef enumdef
	default: return "ERR";
	}
}

void dbg_opcode_print(const codeunit_t *unit) {
	printf("opcode: %s\n", opcode_to_str(unit->op.ty));
}

