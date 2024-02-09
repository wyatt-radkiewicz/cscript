#ifndef _test_h_
#define _test_h_

#include "csi.h"

char *test_loadfile(const char *path);
int test_lexer(const char *script);
typed_unit_t test_vm(void);

#endif

