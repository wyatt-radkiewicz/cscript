#include <stdio.h>
#include <stdlib.h>

#include "test.h"

char *test_loadfile(const char *path) {
	FILE *fp = fopen(path, "rb");
	fseek(fp, 0, SEEK_END);
	size_t len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	char *str = malloc(len + 1);
	fread(str, 1, len, fp);
	str[len] = '\0';
	fclose(fp);
	return str;
}

int test_lexer(const char *script) {
	lexer_t state = lexer_init(script);
	while (lexer_next(&state)) {
		dbg_tok_print(state.curr);
	}
	return 0;
}

typed_unit_t test_vm(void) {
	unit_t stack[256];
	codeunit_t code[] = {
		// Compute first 10 fibanocci numbers or somethign LMAOOAOOOAOAOAOAO
		{ .op = (opcode_t){
			.ty = opcode_pushimm,
			.op1ty = unit_u16,
			.umm16 = 0,
		}},
		{ .op = (opcode_t){
			.ty = opcode_pushimm,
			.op1ty = unit_u16,
			.umm16 = 1,
		}},
		{ .op = (opcode_t){
			.ty = opcode_pushimm,
			.op1ty = unit_u16,
			.umm16 = 0,
		}},
		// Loop start
		{ .op = (opcode_t){
			.ty = opcode_pushimm,
			.op1ty = unit_u16,
			.umm16 = 9,
		}},
		{ .op = (opcode_t){
			.ty = opcode_cmp,
			.op1ty = unit_i32,
			.op2ty = unit_i32,
			.op1 = 1,
			.op2 = 0,
		}},
		{ .op = (opcode_t){
			.ty = opcode_pop,
			.op1 = 1,
		}},
		{ .op = (opcode_t){
			.ty = opcode_bge,
			.op2ty = unit_i32,
			.op1ty = unit_i8,
			.imm8 = 7,
		}},
		{ .op = (opcode_t){
			.ty = opcode_pushunit,
			.op1ty = unit_u16,
			.umm16 = 2,
		}},
		{ .op = (opcode_t){
			.ty = opcode_moveunit,
			.op1 = 2,
			.op2 = 3,
		}},
		{ .op = (opcode_t){
			.ty = opcode_add,
			.op1ty = unit_i32,
			.op2ty = unit_i32,
			.op1 = 0,
			.op2 = 3,
		}},
		{ .op = (opcode_t){
			.ty = opcode_moveunit,
			.op1 = 0,
			.op2 = 3,
		}},
		{ .op = (opcode_t){
			.ty = opcode_pop,
			.op1 = 2,
		}},
		{ .op = (opcode_t){
			.ty = opcode_inc,
			.op1ty = unit_u16,
			.op2ty = unit_i32,
			.umm16 = 0,
		}},
		{ .op = (opcode_t){
			.ty = opcode_bra,
			.op1ty = unit_i16,
			.imm16 = -11,
		}},
		// return
		{ .op = (opcode_t){
			.ty = opcode_ret,
			.op1ty = unit_i32,
			.op1 = 1,
			.op2 = 3,
		}},
	};
	return interpret(code, stack, arrlen(stack));
}

