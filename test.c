enum enum_test {
	A,
	B,
	C = 10,
	D = 1 << 1,
};
enum enum_test *const b;
typedef struct tok {
	enum enum_test e;
} tok_t, tok_a;

extern tok_t token;

char (*pfn)(void);
struct test_struct {
	int a, b;
} test_func(int test_param);
int (*const (*pfn_pfn)(void))(union pfn_pfn_param {
	int foo, bar;
} param1);
int foo_func(int a, int b);
int *ptr, i, *const consti;
struct foo {
	unsigned int a;
	unsigned long long b;
};

struct foo using_it_again;
