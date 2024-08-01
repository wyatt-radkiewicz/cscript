
typedef bool (*test_pfn_t)(void);
typedef struct test_s {
    test_pfn_t pfn;
    const char *name;
} test_t;

