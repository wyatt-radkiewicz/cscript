#include <stdio.h>
#include <stdlib.h>

#include "csi.h"

char *test_loadfile(const char *path) {
	FILE *fp = fopen(path, "rb");
	fseek(fp, 0, SEEK_END);
	size_t len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	char *str = arena_alloc(len + 1);
	fread(str, 1, len, fp);
	str[len] = '\0';
	fclose(fp);
	return str;
}

void conlog_s1d(const char *s, int x1) {
	printf("%s%d\n", s, x1);
}

int main(int argc, char **argv) {
	if (argc != 2) {
		printf("Pass in file to test\n");
		return 0;
	}

	arena_init("__BASE");

	char *srcbuf = NULL;
	{
		char *src = test_loadfile(argv[1]);
		srcbuf = arena_alloc(4096);
		preprocess(src, srcbuf, 4096);
	}
	codeunit_t *code = arena_alloc(sizeof(*code) * 1024);
	uint8_t *data = arena_alloc(1024);
	{
		struct parser_result res = parse(srcbuf);
		for (size_t i = 0; i < res.nerrs; i++) {
			err_print(res.errs + i);
		}
		dbg_ast_print(res.root, 0);
		if (!res.nerrs) compile(res.root, code, 1024, data, 1024);
	}
	unit_t *stack = arena_alloc(sizeof(*stack) * 128);
	dbg_typed_unit_print(interpret(code, stack, 128));

	arena_deinit();

	return 0;
}

