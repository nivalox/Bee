# Bee

A B compiler that works by translating B to C, so it runs anywhere a C compiler does
(built for Termux on arm64 and arm32 first).

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
`-m` modern assignment (see below), `-s` strict B (no backslash escapes). Set `CC` to pick a compiler.

## How it works

- **Memory is one array of machine words.** A B pointer is a word index into it, so
  `v[1]` is the next word and `a[b] == b[a]` both hold, as on the original machines.
- **The word is `intptr_t`.** 64-bit on arm64, 32-bit on arm32. Characters pack
  `sizeof(word)` per word, first char in the low byte (8 on arm64, 4 on arm32).
- **All functions take 8 words**, so function values and indirect calls just work.
- **The runtime is plain C** (`bee_rt.h`) and is embedded in every generated file.
- Two passes: the first learns top-level names so forward references work.

## Source layout

    src/
      bee.h          shared types and declarations
      util.c         buffers, string building, error reporting
      lexer.c        source -> tokens (escapes live here)
      symbols.c      global and local symbol tables, builtin names
      parser.c       parser state, token helpers, char/string literal emission
      expr.c         expressions   (B -> C)
      stmt.c         statements    (B -> C)
      decl.c         top level: functions, globals, vectors
      main.c         driver: options, the two passes, calling the C compiler
      runtime/
        bee_rt.h     the C runtime (printf, putchar, ...) embedded in every output
    examples/        sample B programs
    tests/           regression tests: NAME.b + NAME.exp, run by tests/run.sh
    build/           object files and the generated rt_inc.h (git-ignored)

To add a builtin: write `BEE_FN(name)` in `src/runtime/bee_rt.h` and add its name to
`builtins[]` in `src/symbols.c`. To change the lexer, edit `src/lexer.c` only; to add
a statement, `src/stmt.c`; to add an operator, `binprec`/`binop` in `src/expr.c`.

## Supported

auto/extrn, vectors, `if/else`, `while`, `switch/case/default`, `break`, `goto`/labels,
`return`, all B operators, `?:`, `++/--`, function values.
Builtins: `putchar getchar char lchar printf printn putnumb exit`.
Strings end in `*e`, escapes use `*` (`*n *t *0 *e *( *) ** *' *"`).
printf: `%d %o %x %c %s %%`, with `-` / `0` flags and a field width (`%4d`, `%-6s`, `%04x`).

Extensions: `&&`, `||`, `continue`, `+=` style assignment, `0x` hex numbers, and C-style
backslash escapes (`\n \t \r \0 \e \\ \' \"`) next to B's `*` ones. An unknown `\x` keeps its
backslash. Use `-s` to turn backslash escapes off.

Tests: `make test` (or `tests/run.sh`) compiles each `tests/*.b` and diffs it against `.exp`.

## Quirks (faithful to B)

- `x=-1` reads as `x =- 1` (subtract), like real B. Put a space (`x = -1`) or pass `-m`.
- `putchar('ab')` prints every char packed in the word, not just one.
- Names resolve leniently: `extrn` is parsed but not required.

## TODO

- Real `alloc`/`free`, file I/O (`open read write close`), `nargs`
- `-W` warnings, better error recovery (currently stops at the first error)
- Write the compiler in B and have Bee compile itself
