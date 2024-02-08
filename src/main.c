#include <stdio.h>
#include <stdlib.h>

#include "csi.h"

int test_parser(int argc, char **argv) {
	if (argc != 2) {
		printf("give a file to parse\n");
		return -1;
	}

	FILE *fp = fopen(argv[1], "rb");
	fseek(fp, 0, SEEK_END);
	size_t len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	char *script = malloc(len + 1);
	fread(script, 1, len, fp);
	script[len] = '\0';
	fclose(fp);

	parse_state_t state = parse_state_init(script);
	while (true) {
		tok_t tok = parse_state_nexttok(&state);
		dbg_tok_print(tok);
		if (tok.ty == tok_error || tok.ty == tok_eof) break;
	}

	free(script);
	return 0;
}

typed_unit_t test_vm(void) {
	unit_t stack[256];
	const union {
		opcode_t op;
		uint32_t i;
	} code[] = {
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
			.umm16 = 10,
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
	return interpret((const opcode_t *)code, stack, arrlen(stack));
}

int main(int argc, char **argv) {
	if (test_parser(argc, argv) != 0) return -1;
	dbg_typed_unit_print(test_vm());

	return 0;
}

