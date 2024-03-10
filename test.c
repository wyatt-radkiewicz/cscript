#define INNER(x) (x + 1)
//#define TEST(x) (x * 2)
//#define TEST2 / 69
#define _concat(x, y) x + y
#define concat(x, y) INNER(x) + y
int a = concat(test, pre);
