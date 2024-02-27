#define INNER(x) (x + "inner")
#define MAC(x, y) INNER(x) % y
int a = MAC("s", func(4, 5));
