/*
 * Utility functions to parse ast nodes
 * Might also fall under the category of functions in a lexer
 */
#ifndef _util_h_
#define _util_h_

/*
 * Consumes indents until it finds a non-indent/whitespace character, and then
 * returns how many idents were consumed.
 */
int
consume_indent(
	const char **src);

#endif

