
let MAX: int = 5 + 2 - 5 * 5;

//fn addtwo(a: int, b: int) -> int {
//	let x: int = a * b;
//
//	if x > MAX {
//		return a + b;
//	} else {
//		return a - b;
//	}
//}

// Base types
// int		(i32)
// uint		(u32)
// float	(f32)
// char		(char)
// &		refrences
// ()		(const function pointer)
// *		refrence counted pointers
// extern *	const pointer in C (only can be cast to refrence)

// Refrences can't be in structs, but pointers and pfn's can.
// Pointers are always nullable pointers to dynamically allocated, refrence
// counted VM objects.
// Extern pointers are pointers to C objects, and are not refrence counted.
// Pointers can only be created by using the new keyword. Which will take an
// init expression which is either a literal, a struct initializer, or an
// array initializer.
// Pointers are also Monads. You either use Rust's
// "if let inside = ptr {  }" syntax
// or use the unwrap keyword like
// "let inside: &int = unwrap ptr;" syntax.
// The unwrap or if let syntax returns a refrence.

// Compound types
// structs
// typedefs

