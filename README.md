# Bee

A successor to the B language. Bee reads **traditional B and a modern, C-inspired syntax in the
same source file** (no mode switch), and translates it to C, so it runs anywhere a C compiler
does (built for Termux on arm64 and arm32 first). Traditional B is never removed or changed;
the modern syntax is an extension.

    prog.b  ->  bee  ->  prog.c  ->  cc/clang  ->  native binary

## Quick start

Termux (or any machine with git, make and a C compiler), one paste:

    pkg install -y git clang make    # Termux; use your package manager elsewhere
    git clone https://github.com/nivalox/Bee.git && cd Bee && make && make test

Already in the folder you want it in? Clone straight into the current directory
(it must be empty):

    git clone https://github.com/nivalox/Bee.git . && make

Then compile and run a program:

    ./bee examples/hello.b      # writes ./examples/hello
    ./examples/hello

`make install` copies `bee` to `$PREFIX/bin` (Termux sets `$PREFIX` for you, so `bee`
then works from anywhere; elsewhere it defaults to `/usr/local`, or run
`make install PREFIX=$HOME/.local`). Update later with `git pull && make`.

Options: `-o FILE` output name, `-C` write only the C, `-k` keep the generated C,
`-m` modern assignment only (see Assignment), `-s` strict B (no backslash escapes). Set `CC` to pick a compiler.

## How it works

- **Memory is one array of machine words.** A B pointer is a word index into it, so
  `v[1]` is the next word and `a[b] == b[a]` both hold, as on the original machines.
- **The word is `int64_t`, on every target** (arm32 included), so programs behave the same
  everywhere. Characters pack 8 per word, first char in the low byte. Original B packed
  2 to 4; Bee raises that on purpose.
- **Floats are doubles stored bit-for-bit in a word**, so they pass through memory, calls and
  returns like any other word. Only arithmetic and printf know the difference.
- **All functions take 8 words**, so function values and indirect calls just work.
- **The runtime is plain C** (`bee_rt.h`) and is embedded in every generated file.
- Two passes: the first learns top-level names, types and function signatures so forward
  references work; type errors are reported in the second, when everything is known.

## Source layout

    src/
      bee.h          shared types and declarations
      util.c         buffers, string building, errors and warnings
      lexer.c        source -> tokens (comments, numbers, escapes, historical operators)
      symbols.c      global and local symbol tables, builtin names, implicit externs
      types.c        the value types and the assignment-compatibility rules
      parser.c       parser state, token helpers, unsupported-syntax messages
      expr.c         expressions   (typed: B -> C), calls, printf checks
      stmt.c         statements    (B -> C)
      decl.c         declarations: top-level functions/globals, int/char/float, char arrays
      main.c         driver: options, the two passes, calling the C compiler
      runtime/
        bee_rt.h     the C runtime (printf, floats, strings...) embedded in every output
    examples/        sample B programs
    tests/           regression tests, run by tests/run.sh
    build/           object files and the generated rt_inc.h (git-ignored)

To add a builtin: write `BEE_FN(name)` in `src/runtime/bee_rt.h` and add its name to
`builtins[]` in `src/symbols.c`. Lexer changes live in `src/lexer.c`; a new statement goes in
`src/stmt.c`; an operator in `binprec`/`binop`/`arith` in `src/expr.c`; a type rule in
`src/types.c`.

## Two syntaxes, one language

Both of these are valid, and they can be mixed freely, even inside one function:

    /* traditional B */                  // modern
    main() {                              main() {
        extrn a, b, c;                        int age = 16;
        auto i;                               hello = "Gilbert";
        a = 10;  c =+ 5;                      age += 1;
        printf("%d*n", a);                    printf("%s %d\n", hello, age);
    }                                     }

    add(a, b) { return a + b; }           int add(int a, int b) { return a + b; }

### The type rule (this is what keeps both styles safe)

- **Traditional names are untyped words and are never checked**, exactly as in B: `auto`,
  `extrn`, plain parameters, and globals like `a 10;`. Anything may be stored in them.
- **Modern names have a stable type, checked at compile time:** anything declared with
  `int`, `char`, `float`, `char x[]`, or created by inference. A variable never silently
  changes type.

| type   | holds | notes |
|--------|-------|-------|
| word   | anything (traditional B) | unchecked |
| int    | 64-bit integer | |
| char   | one character | `'B'`; a constant with several characters is an error here |
| float  | 64-bit double | `3.14`, `.5`, `2e3`, `1.5e-3` |
| string | address of packed characters | string literals and `char x[]`; `s[i]` is a byte |

Assignment rules (`=`, `+=`..., initialisers, arguments, `return`):

- into a **word**: anything.
- into **int/char**: int, char or word. A float or a string is an error (no implicit narrowing).
- into **float**: float or int/char (converted). A string or an untyped word is an error.
- into **string**: a string or a word. Anything else is an error.
- arithmetic mixing int and float produces a float; `+ - * / < <= > >= == !=` work on floats,
  `% & | << >>` do not. `==`/`!=` on strings compare addresses; no other operator is allowed on them.
- floats divide like IEEE: `x / 0.0` is inf or nan, not a runtime error. Integer division by
  zero is still a runtime error.

## Modern declarations

    int age = 16;                // zeroed if you leave off "= value"
    char letter = 'B';
    float pi = 3.14159;
    char greeting[] = "Hello";   // see Char arrays
    int a = 1, b = 2, c;         // several at once

Allowed at top level (the initialiser must be a constant) and inside functions (any
expression). `int`, `char` and `float` start a declaration only when a name follows, so
`char(s, i)` (the builtin) and old programs that use these words as names keep working.
Declaring a name twice in one scope is an error; an inner block may shadow an outer name.
Arrays of int/float are not supported (use B's `auto v[N]` word vectors).

### Inferred declarations

    hello = "Gilbert";   age = 67;   number = 3.14;   ch = 'x';

An assignment statement to a name that does not exist yet **creates it**, typed from the
value (`hello` string, `age` int, `number` float, `ch` char). If the value is untyped (for
example the result of a traditional function) the new variable is an untyped word. Inside a
function the new variable is local and lives until the end of its block; at top level it is a
global (the value must be a constant). `x = x + 1` on a new `x` is an error, since `x` does
not exist yet on the right-hand side. Only a plain `name = value;` statement creates
variables; `x += 1` or a use inside an expression of a name that does not exist is an
"undefined variable" error.

### `extrn` without a definition

Real B needs `extrn name;` to match a definition elsewhere. Bee relaxes that: if nothing
defines the name, `extrn name;` gives you a zeroed global word, so this works as written:

    main() { extrn oldValue;  oldValue = 10;  ... }

## Functions

    add(a, b) { return a + b; }                       // traditional
    int add(int a, int b) { return a + b; }           // modern
    float half(int n) { return n / 2.0; }
    int len(char s[]) { ... s[n] ... }                // string parameter: s[i] is a byte

The return type and parameter types are optional and can be mixed (`f(int a, b)`). A function
written with any type is **checked at every call**: argument count, argument types (an int
passed to a float parameter is converted) and the `return` value. Traditional functions are
never checked, as in B. Functions can be called before they are defined; there are no
prototypes (and `int f(int a);` is an error that says so). Return types are `int`, `char`,
`float`. At most 8 parameters.

## Char arrays and strings

    char var[] = "hello, world!";    /* size taken from the text (plus the *e terminator) */
    char buf[64];                    /* 64 bytes, zeroed */
    char line[32] = "hi";            /* 32 bytes, starts with "hi" */
    message = "Hello, world!";       /* an inferred string variable */

`var[i]` is the i-th character: read it, assign to it, use `+=`, `++` (it holds 0-255).
`var` itself is the address of the string, so it works with `printf("%s", var)`, `char(var, i)`
and `lchar(var, i)`. Strings end in `*e` (value 4), not C's NUL.

A **char array has fixed storage and cannot be reassigned** (`var = "x"` is an error); a
string **variable** such as `message` can point at another string. Bee does not have C
pointers: a string value is a word index into Bee's memory, nothing more. A local array is
zeroed and filled in each time execution reaches its declaration. `&var[i]` is not allowed.
A `char s[]` parameter keeps byte indexing inside the function; with a plain parameter use
`char(s, i)` / `lchar(s, i)`. String literals are writable (`message[0] = 'J'`).

## Strings, characters and escapes

Traditional escapes use `*` (`*n *t *r *0 *e *( *) ** *' *"`). Modern backslash escapes work
too (`\n \t \r \0 \e \\ \' \"`); an unknown `\x` keeps its backslash, and `-s` turns
backslash escapes off for strict B. In a string, a literal star is `**`.
A character constant is one word: up to 8 characters (`'hello, w'`), longer is an error.

## printf

`%d %o %x %c %s %f %e %g %%`, flags `-` and `0`, a field width, and a precision for
`%f %e %g %s` (`%4d %-6s %04x %8.3f %.2f`). `%f` takes a float; give it `3.0`, not `3`.

When the format is a string literal, Bee checks it at compile time: a float given to `%d`, an
int to `%f` or `%s`, a string to `%d` are errors; a wrong argument count or an unknown
conversion is a warning (as in B, extra or missing arguments are not fatal).

## Assignment

Modern: `x = 10; x += 1; x -= 1; x *= 2; x /= 2;` plus `%= <<= >>= &= |=`.

Traditional B puts the `=` first: `=+ =- =* =/ =% =<< =>> =& =|`, and six relational ones,
`=== =!= =< =<= => =>=` (`x =< y` means `x = x < y`). All of them work, in the same file as
the modern forms. See Historical notes.

**The one place the two disagree is `x=-1`**: with no space B reads `=-` (subtract), so it means
`x = x - 1`. Bee keeps that historical meaning and prints a warning. Write `x = -1` (with
spaces) to assign a negative number. Pass `-m` to read the modern way only.

## Comments

    /* traditional block comment */
    // modern single-line comment

Neither starts inside a string. `//` is never a B operator, so existing programs are unaffected.

## Diagnostics

Every error is `file:LINE: error: explanation`; warnings use `warning:`. Examples:

    prog.b:3: error: undefined variable 'nope' (declare it with int/auto, or create it by assigning to it: nope = ...;)
    prog.b:4: error: 'x' is already declared in this scope (first declared on line 2)
    prog.b:5: error: incompatible types: 'age' is an int but the value is a string (a string is not a number)
    prog.b:6: error: function 'add' takes 2 argument(s) but 3 given
    prog.b:7: error: malformed parameter list in 'f': expected ',' or ')' but found 'b'
    prog.b:8: error: unsupported syntax: 'for' loops are not part of Bee yet; use while
    prog.b:9: error: printf: '%d' expects an integer but argument 1 is a float (use %f)

Covered: undefined variables/functions, duplicate declarations (variables, parameters,
globals, functions), incompatible assignments/arguments/returns, invalid declarations
(`int v[3]`, `char c = 'ab'`, `char s[]`, non-constant global initialisers), malformed
functions (missing name, missing body, prototypes, bad parameter lists), unsupported syntax
(`for`, `do`, `void`, `double`, `long`, `struct`, `static`, `const`, `sizeof`, `.`), and printf
mismatches. Compilation stops at the first error.

## Where B and the modern syntax collide

Decisions made so old programs keep their meaning:

| conflict | resolution |
|----------|-----------|
| `x=-1` (B: `x =- 1`) | historical meaning kept, warning printed; `-m` for modern-only |
| `int`, `char`, `float` as names | a declaration only when a name follows; `char(s, i)` still calls the builtin |
| `name value;` at top level is a B global | `int x = 5;` and `x = 5;` are the modern forms; a *global named* `int` or `float` followed by a name is now read as a declaration |
| a bare assignment to an unknown name | was "undefined name"; now creates a variable. Reading an unknown name is still an error |
| `extrn name;` with no definition | now a zeroed global instead of an error |
| `//` | was not valid B; now a comment |
| `\` in strings | was an ordinary character; now an escape if it forms `\n \t ...`. Use `-s` for the old rule |

## Historical notes

Checked against Thompson's *Users' Reference to B* (1972):

- B has **16 assignment operators** (`=` plus `=op` for every binary operator), including the
  relational ones; Bee implements all of them. (The 1972 manual notes the original interpreter
  never got `===  =!= =< =<= => =>=`.)
- B is **untyped**: "there is no check to insure that there are no type mismatches", and no
  conversions. That is why traditional names stay unchecked in Bee; only modern names are typed.
- B has no floating point. Floats are a Bee extension.
- In B, the first use of an undeclared name is an *internal* (static) variable. Bee's inferred
  declarations look similar but are ordinary automatic (per-call) variables.
- B's `extrn` must name something defined elsewhere (Bee relaxes this), character constants hold
  one or two characters (Bee: eight), and `printf` knew `%d %o %s %c` only.
- Comments are `/* */` only, names are at most 8 characters and keywords are lower case
  and reserved.

## Supported

auto/extrn, vectors, `if/else`, `while`, `switch/case/default`, `break`, `goto`/labels,
`return`, all B operators, `?:`, `++/--`, function values. Extensions: `&&`, `||`, `continue`,
`0x` hex numbers, floats, the modern syntax above.
Builtins: `putchar getchar char lchar printf printn putnumb exit`.

**Not supported (yet):** C pointers (`char *p`), arrays of int/float, `for`, `do`, structs,
`void`, prototypes, `sizeof`, `double`/`long`/`unsigned`, string comparison or concatenation
operators, a modern `float`/`int` cast.

## Tests

`make test` (or `tests/run.sh`) builds the examples and runs the whole suite.

**`tests/all_features.b` is one self-checking program covering every feature** (traditional
operators and historical `=op` forms, control flow, goto/switch, vectors, function values,
typed and untyped functions, declarations, inference, compound assignment, floats, char arrays,
every escape, printf, mixed syntax, comments). It prints a line per section, a `FAIL` line for
any broken check, and a summary ending in `ALL PASSED`. Run it alone with:

    ./bee -o /tmp/all tests/all_features.b && /tmp/all

The rest of the suite narrows a failure down. Each `tests/NAME.b` is one of:

- **`NAME.exp`**: compile, run, and the output must match exactly. Covers traditional B
  (`traditional`, `histops`, `stress`), escapes/printf/strings (`escapes`, `intfmt`, `width`,
  `strict`, `longchar`, `chararray`, `strings_chars`), modern syntax (`modern_decl`,
  `inferred`, `compound`, `functions`, `floats`, `comments`) and both styles together (`mixed`).
- **`e_*.b` + `.err`** (+ optional `.line`): must be rejected with that message on that line.
- **`w_*.b` + `.warn`**: must compile and print that warning.

The suite also passes with the compiler built under AddressSanitizer and UBSan.

## TODO

- Real `alloc`/`free`, file I/O (`open read write close`), `nargs`
- Several errors per run instead of stopping at the first
- `for` loops, int/float arrays, a pointer-like type
- Write the compiler in B and have Bee compile itself
