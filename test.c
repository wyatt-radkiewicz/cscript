#define INNER(x) (x + 1)
#define TEST(x) (INNER(x) * 2)
//#define TEST2 / 69
//#define _concat(x, y)  + y##_post
//#define concat(x, y) concat(INNER(x), y)
int a = TEST(test);
