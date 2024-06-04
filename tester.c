/*#include <assert.h>*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bs.h"

static const char *const src =
"type Point\n"
"    x i32\n"
"    y i32\n";

void print_err(void *user, int line, int col, const char *msg)
{
	printf("[%d:%d]: %s\n", line, col, msg);
}

int main(int argc, char **argv)
{
	bs_byte	code[512];

	struct bs bs;
	int len = 0;

	struct bs_strview src_strview;

	src_strview.str = src;
	src_strview.len = strlen(src);
	bs_init(&bs, src_strview, print_err, NULL, NULL, 0, code, sizeof(code));

	bs_advance_token(&bs);
	bs_parse_typedef(&bs);

	/*bs_parse_type(&bs, bs.typebuf, &len,
			BS_ARRSIZE(bs.typebuf) - bs.typebuflen, 0);
	bs.typebuflen += len;*/

	return 0;
}

