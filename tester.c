/*#include <assert.h>*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bs.h"

static const char *const src =
"enum Shape\n"
"    Circle\n"
"    Square\n"
"        width f32\n"
"        height f32\n";

void print_err(void *user, int line, int col, const char *msg)
{
	printf("[%d:%d]: %s\n", line, col, msg);
}

int main(int argc, char **argv)
{
	bs_byte	code[512];

	struct bs bs;
	struct bs_var testvar;
	int len = 0;

	struct bs_strview src_strview;

	src_strview.str = src;
	src_strview.len = strlen(src);
	bs_init(&bs, src_strview, print_err, NULL, NULL, 0, code, sizeof(code));
	bs.mode = BS_MODE_COMPTIME;

	bs_advance_token(&bs, BS_TRUE);
	bs_parse_typedef(&bs);
	/*bs_parse_type(&bs, testvar.type, &testvar.loc, BS_ARRSIZE(testvar.type), 0);*/
	/*bs_parse_expr(&bs, BS_PREC_FULL, &testvar, 0);*/

	return 0;
}

