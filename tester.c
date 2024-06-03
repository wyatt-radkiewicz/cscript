/*#include <assert.h>*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bs.h"

static const char *const src =
"Bar<i32, bool>";

void print_err(void *user, int line, int col, const char *msg)
{
	printf("[%d:%d]: %s\n", line, col, msg);
}

int main(int argc, char **argv)
{
	bs_byte	code[512];

	struct bs bs;
	bs_type_t *typeiter;
	int len_left;

	struct bs_strview src_strview;

	src_strview.str = src;
	src_strview.len = strlen(src);
	bs_init(&bs, src_strview, print_err, NULL, NULL, 0, code, sizeof(code));
	bs.defs[0].type = BS_USERDEF_STRUCT;
	bs.defs[0].name.str = "Foo";
	bs.defs[0].name.len = 3;
	bs.defs[0].def.s.templates = bs.strbuf;
	bs.defs[0].def.s.ntemplates = 2;
	bs.strbuf[0].str = "A";
	bs.strbuf[0].len = 1;
	bs.strbuf[1].str = "B";
	bs.strbuf[1].len = 1;
	bs.defs[1].type = BS_USERDEF_TYPEDEF;
	bs.defs[1].name.str = "Bar";
	bs.defs[1].name.len = 3;
	bs.defs[1].def.t.templates = bs.strbuf + 2;
	bs.defs[1].def.t.ntemplates = 2;
	bs.defs[1].def.t.type = bs.typebuf;
	bs.strbuf[2].str = "C";
	bs.strbuf[2].len = 1;
	bs.strbuf[3].str = "D";
	bs.strbuf[3].len = 1;
	bs.typebuf[0] = BS_TYPE_REF;
	bs.typebuf[1] = BS_TYPE_USER;
	bs.typebuf[2] = 0;
	bs.typebuf[3] = BS_TYPE_TEMPLATE;
	bs.typebuf[4] = 3;
	bs.typebuf[5] = BS_TYPE_TEMPLATE;
	bs.typebuf[6] = 2;
	bs.typebuflen = 7;
	bs.ndefs = 2;

	typeiter = bs.typebuf + bs.typebuflen;
	len_left = BS_ARRSIZE(bs.typebuf) - bs.typebuflen;
	bs_parse_type(&bs, &typeiter, &len_left);
	bs.typebuflen = BS_ARRSIZE(bs.typebuf) - len_left;

	return 0;
}

