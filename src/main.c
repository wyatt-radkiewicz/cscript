#include <stdio.h>
#include <stdlib.h>

#include "csi.h"

ast_t _ast[2048];

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

int main(int argc, char **argv) {
	if (argc != 2) {
		printf("Pass in file to test\n");
		return 0;
	}

	// codeunit_t code[2048];
	// uint8_t data[1024];
	// unit_t stack[512];
	{
		const char *src = test_loadfile(argv[1]);
		parser_t parser = parse(src, _ast, arrlen(_ast));
		for (size_t i = 0; i < parser.nerrs; i++) {
			err_print(parser.errs + i);
		}
		dbg_ast_print(_ast, parser.root);
		//compiler_t compiler = compiler_init(src, code, arrlen(code), data, arrlen(data));
		//compile(&compiler);
		//for (size_t i = 0; i < compiler.code - code; i++) {
		//	dbg_opcode_print(&code[i]);
		//}
		free((void *)src);
	}
	//dbg_typed_unit_print(interpret(code, stack, arrlen(stack)));

	return 0;
}

