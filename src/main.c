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
		//printf("\n\n%s\n\n", srcbuf);
	}
	{
		struct parser_result res = parse(srcbuf);
		for (size_t i = 0; i < res.nerrs; i++) {
			err_print(res.errs + i);
		}
		dbg_ast_print(res.root, 0);
	}

	arena_deinit();

	return 0;
}

