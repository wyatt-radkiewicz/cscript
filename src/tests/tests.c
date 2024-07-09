#include "test_common.h"

#include "test_lexer.h"
#include "test_type.h"

//
// Actual tests
//
#define add_test(NAME) { .fn = test_##NAME, .name = #NAME }
static const test_t tests[] = {
	add_test(lexer1),
	add_test(lexer2),
	add_test(lexer3),
	add_test(type1),
	add_test(type2),
	add_test(type3),
	add_test(type4),
	add_test(type5),
	add_test(type6),
	add_test(type7),
	add_test(type8),
	add_test(type9),
	add_test(type10),
	add_test(type11),
	add_test(type12),
	add_test(type13),
	add_test(type14),
	add_test(type15),
	add_test(type16),
	add_test(type17),
	add_test(type18),
	add_test(type19),
	add_test(type20),
	add_test(type21),
	add_test(type22),
	add_test(typedef1),
	add_test(typedef2),
	add_test(typedef3),
	add_test(typedef4),
	add_test(typeeq1),
	add_test(typeeq2),
	add_test(typeneq1),
	add_test(typeneq2),
	add_test(typeneq3),
	add_test(typeneq4),
};

//
// Main test runner
//
int main(const int argc, char **const argv) {
	int npassed = 0;

	for (int i = 0; i < arrlen(tests); i++) {
		curr_test = tests[i].name;
		const bool res = tests[i].fn();

		printf("test %16s: ", curr_test);
		if (res) {
			npassed++;
			printf("[PASS]\n");
		} else {
			printf("[\033[0;31mFAIL\033[0m]\n");
		}
	}

	printf("passed %d/%d\n", npassed, (int)arrlen(tests));

	return 0;
}

