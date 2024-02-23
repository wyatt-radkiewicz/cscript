char (*pfn)(void);
struct test_struct {
	int a, b;
} test_func(int test_param);
int (*const (*pfn_pfn)(void))(int a);
int foo_func(int a, int b);
int *ptr, i, *const consti;
struct foo {
	unsigned int a;
	unsigned long long b;
};

struct foo using_it_again;
