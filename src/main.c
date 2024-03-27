#include <stdio.h>
#include <stdlib.h>

#include "lexer.h"
#include "parser.h"

// Testing function
char *load_file(const char *file)
{
	long len;
	FILE *fp;
	char *str;

	fp = fopen(file, "r");
	if (!fp) return NULL;
	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	str = malloc(len + 1);
	if (fread(str, 1, len, fp) != len)
	{
		free(str);
		fclose(fp);
		return NULL;
	}
	str[len] = '\0';
	fclose(fp);

	return str;
}

static struct ast_node ast_buffer[512];

int main(int argc, char **argv)
{
	char *src = load_file("test.cs");
	//char *head = src;
	//struct token token;
	//int i;

	//token_iter_init(&token);
	//while (token_iter_next((const char **)&head, &token))
	//{
	//	printf("consumed %d: \"", token.type);
	//	for (i = 0; i < token.strlen; i++)
	//	{
	//		putchar(token.str[i]);
	//	}
	//	printf("\"\n");
	//}

	ast_construct(src, ast_buffer, 512, NULL, 0);

	free(src);

	return 0;
}

