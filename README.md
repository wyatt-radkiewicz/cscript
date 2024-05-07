# CScript

A semi-freestanding, C11, single-header modern scripting language with just enough bells and whistles to make coding in it a breeze, while also making it safe and fast to run.
* Note: Freestanding in this context meaning no malloc, printf, etc, but still uses sprintf, etc (A list of all semi-freestanding functions used will be included)

CScript is a scripting language meant to be embedded into other apps. By default cscript comes with not many features:
- No standard library
- No sweep and mark garbage collection
- Tagged Unions (enums)
- Structs
- Typedefs
- Functions
- Generics
- Refrences

CScript also has no mark and sweep garbage collector. All allocations are explicit through the use of refrence counted pointers. Memory saftey is also enforced meaning the only vector that can be used to attack a cscript program is the foreign functions exposed to the script itself.

Demo CScript file:
```
ext print_str(s &[] char)

fn copyarr<T>(from &[] T, to &[] mut T)
	for i = 0..min(len(from), len(to))
		to[i] = from[i]

type StrBuf
	str *[] mut char
	len usize

fn strbuf_new(capacity usize) StrBuf
	StrBuf{
		str = alloc(StrBuf.str, capacity)
		...
	}
fn strbuf_cat(buf &mut StrBuf, s &[] mut char)
	if len(s) + buf.len > len(buf.str)
		mut newbuf = alloc(StrBuf.str, len(buf.str) * 2)
		copyarr(buf.str, newbuf)
		buf.str = newbuf
	copyarr(s, &mut buf.str[buf.len..])
	buf.len = buf.len + len(s)


-- FNV-1a hash algorithm for strings
fn calc_hash<StrBuf>(buf &StrBuf) Hash
	const OFFSET_BIAS = Hash{2166136261}
	const PRIME = Hash{16777619}

	mut hash = OFFSET_BIAS
	mut i = 0
	while i < (buf.len / 4 * 4)
		hash = hash * PRIME
		hash = hash ^ {
			mut x = buf.str[i++]
			x = x | buf.str[i++] << 8
			x = x | buf.str[i++] << 16
			x = x | buf.str[i++] << 24
		}
	if i == buf.len
		return hash
	hash = hash * PRIME
	hash ^ {
		mut x = 0
		mut shift = 0
		while i < buf.len
			x = x | buf.str[i++] << shift
			shift = shift + 8
	}

type Hash u32
fn calc_hash<T>(x &T) Hash

type HashElem<K, V>
	used bool
	psl u32
	key K
	val V
type HashMap<K, V>
	ents *[] HashElem<K, V>
	nused usize

fn hashmap_new<K, V>(capacity usize) HashMap<K, V>
	HashMap<K, V>{
		ents = alloc(HashMap.ents, capacity)
		nused = 0
	}
-- Returns true if there was nothing in the map
-- Returns false if it only set a pre-existing key
-- Doesn't work fully yet
fn hashmap_set<K, V>(map &mut HashMap<K, V>, key &K, val &V) bool
	mut elem = HashElem{
		key = key
		val = val
		...
	}
	mut i = calc_hash(key) % len(map.ents)
	while map.ents[i].used
		mut ent = &mut map.ents[i]
		if elem.psl > ent
			const tmp = elem
			elem = ent
			ent = tmp
		elem.psl = elem.psl + 1
		i = (i + 1) % len(map.ents)
	map.ents[i] = elem
	true
			
-- Array creation:
-- const arr = initarr(type, len, default (optional))
```

Current CScript syntax:
```
# Top level syntax
file		-> file def | def
def			-> struct | enum | function | let
struct		-> 'struct' '{' UP memberlist DN '}' NEWLINE
memberlist	-> [memberlist NEWLINE INDENT] ident type
enum		-> 'enum' '{' UP variantlist DN '}' NEWLINE
variantlist	-> [variantlist NEWLINE INDENT] variant
variant		-> ident [UP NEWLINE memberlist DN NEWLINE]
function	-> [fn | ext] ident [< templatelist >] '(' paramlist ')' [type] NEWLINE [UP stmtlist DN]
templatelist-> templatelist , ident | ident
paramlist	-> paramlist , ident type | ident type

# Statement syntax
stmtlist	-> stmtlist stmt | stmt
stmt		-> if | let | while | for | return | break | continue | match | exprstmt

# Expression syntax
expr		-> postfix = expr | condexpr
condexpr	-> 'if' expr 'then' expr ['else' expr] | logicor
logicor		-> logicor || logicand | logicand
logicand	-> logicand && equality | equality
equality	-> equality eqop relational | relational
eqop		-> == | !=
relational	-> relational relop bitor | bitor
relop		-> > | >= | < | <=
bitor		-> bitor '|' bitxor | bitxor
bitxor		-> bitxor ^ bitand | bitand
bitand		-> bitand & bitshift | bitshift
bitshift	-> bitshift shiftop additive | additive
shiftop		-> << | >>
additive	-> additive addop multiplicative | multiplicative
addop		-> + | -
multiplicative -> multiplicative mulop prefix | prefix
mulop		-> * | / | %
prefix		-> prefixop prefix | builtin
prefixop	-> + | - | ! | ~ | & | *
builtin		-> builtinfn '(' expr ')' | postfix
builtinfn	-> len | type | sizeof | alignof
postfix		-> grouping postfixop | grouping
postfixop	-> '->' | . | '(' arglist ')' | '[' expr ']'
grouping	-> '(' expr ')' | '{' UP stmtlist DN '}' | factor
factor		-> literal | initexpr
literal		-> ident | number | string | character
initexpr	-> ident '{' UP initlist DN '}'
arglist		-> arglist ',' expr | expr
initlist	-> [initlist NEWLINE INDENT] initmember
initmember	-> ident = expr | expr

# Type syntax
type		-> typelvl type | typesentinal
typelvl		-> const | mut | '&' | '&[]' | '*' | '*[]' | '[' postfix ']'
typesentinal-> podtype | ident
podtype		-> 'i8' | 'i16' | 'i32' | 'i64'
			 | 'u8' | 'u16' | 'u32' | 'u64'
			 | 'char' | 'bool'

# Token syntax
# TODO
```


