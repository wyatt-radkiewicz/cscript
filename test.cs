
let const MAX: uint = 5;

struct test {
	a: int,
	b: uint,
	c: &int,
	d: [int; 5],
	e: &[int],
	f: &[int; 5],
	string: str,
	fnptr: (int, (int) -> int) -> int,
}

typedef test_t: struct test;

fn add_both(a: int, b: int) -> int {
	a + b
}

fn add_print_generic(a: &void, b: &void) {
	if (typeof(a) == typeof(int)) {
		conlog("a + b: " + itos(a as int + b as int));
	} else if (typeof(a) == typeof(uint)) {
		conlog("a + b: " + itos(a as uint + b as uint));
	} else if (typeof(a) == typeof(float)) {
		conlog("a + b: " + itos(a as float + b as float));
	}
}

// Base types
// int		(i32)
// uint		(u32)
// float	(f32)
// ()		(function pointer)
// str		(const char *)
// &		refrences - refrence counted pointers
// *		nullable refrences

// Compound types
// structs
// typedefs

