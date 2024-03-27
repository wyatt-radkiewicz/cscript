
let MAX: const uint;

struct test_s {
	a: int,
	b: uint,
	c: &int,
	d: [int],
	e: &[int],
	//f: &[int; 5],
	//string: str,
	//fnptr: (int, (int) -> int) -> int,
}
//
//typedef test_t = test_s;
//
//fn add_both(a: int, b: int) -> int {
//	return a + b;
//}
//
//fn add_print_generic(a: &void, b: &void) {
//	if (typeof(a) == typeof(int)) {
//		conlog("a + b: " + itos(a as int + b as int));
//	} else if (typeof(a) == typeof(uint)) {
//		conlog("a + b: " + itos(a as uint + b as uint));
//	} else if (typeof(a) == typeof(float)) {
//		conlog("a + b: " + itos(a as float + b as float));
//	}
//}
//fn wobj_init(type: uint, x: float, y: float, ci: int, cf: float) -> wobj_s {
//	let wobj: &wobj_s = &wobj_s{
//		c: &(5 + 5),
//		...
//	};
//}

// Base types
// int		(i32)
// uint		(u32)
// float	(f32)
// str		(const char *)
// &		refrences - refrence counted pointers
// ()		(const function pointer)
// *		analogus to a const pointer in c

// ... syntax
// Will default all values it can of a struct to defaults.
// int, uint, float will be set to 0
// string will be set to ""

// array initialization syntax
// for normal arrays you can do [a, b, c, d, ...] and use ... to initialize
// other things to default values (if needed).
// to initialize arrays programatically, you can use the new keyword like this
// sytnax: 
//	<&>arrinit [varname; arrlen] (init expr)
// where init expr will be called arrlen times with varname set to the index of
// the element in the array, and where init expr evaluates to the object wanting
// to be allocated.

// Compound types
// structs
// typedefs

