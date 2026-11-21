# Bee

A B compiler that works by translating B to C, so it runs anywhere a C compiler does
(built for Termux on arm64 and arm32 first).

    prog.b  ->  bee  ->  prog.c  ->  cc/clang  ->  native binary

## Build and run (Termux)

    pkg install clang make
    make
    ./bee examples/hello.b      # writes ./examples/hello
    ./examples/hello

Options: `-o FILE` output name, `-C` write only the C, `-k` keep the generated C,
`-m` modern assignment (see below). Set `CC` to pick a compiler.

## How it works

- **Memory is one array of machine words.** A B pointer is a word index into it, so
  `v[1]` is the next word and `a[b] == b[a]` both hold, as on the original machines.
- **The word is `intptr_t`.** 64-bit on arm64, 32-bit on arm32. Characters pack
  `sizeof(word)` per word, first char in the low byte (8 on arm64, 4 on arm32).
- **All functions take 8 words**, so function values and indirect calls just work.
- **The runtime is plain C** (`bee_rt.h`) and is embedded in every generated file.
- Two passes: the first learns top-level names so forward references work.

## Supported

auto/extrn, vectors, `if/else`, `while`, `switch/case/default`, `break`, `goto`/labels,
`return`, all B operators, `?:`, `++/--`, function values.
Builtins: `putchar getchar char lchar printf printn putnumb exit`.
Strings end in `*e`, escapes use `*` (`*n *t *0 *e *( *) ** *' *"`).

Extensions: `&&`, `||`, `continue`, `+=` style assignment, `0x` hex numbers.

## Quirks (faithful to B)

- `x=-1` reads as `x =- 1` (subtract), like real B. Put a space (`x = -1`) or pass `-m`.
- `putchar('ab')` prints every char packed in the word, not just one.
- Names resolve leniently: `extrn` is parsed but not required.

## TODO

- Real `alloc`/`free`, file I/O (`open read write close`), `nargs`
- `-W` warnings, better error recovery (currently stops at the first error)
- Write the compiler in B and have Bee compile itself
