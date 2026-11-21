CC     ?= cc
CFLAGS ?= -O2 -Wall -Wextra
PREFIX ?= /usr/local   # Termux sets $PREFIX itself, so `make install` just works there

SRC := $(wildcard src/*.c)
OBJ := $(SRC:src/%.c=build/%.o)

bee: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

build/%.o: src/%.c src/bee.h | build
	$(CC) $(CFLAGS) -Isrc -Ibuild -c -o $@ $<

# main.c embeds the runtime, so it also depends on the generated header
build/main.o: build/rt_inc.h

# turn the runtime header into a C string literal the compiler embeds
build/rt_inc.h: src/runtime/bee_rt.h | build
	sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/^/"/' -e 's/$$/\\n"/' $< > $@

build:
	mkdir -p build

test: bee
	./bee -o examples/hello examples/hello.b && ./examples/hello
	tests/run.sh

install: bee
	mkdir -p $(PREFIX)/bin
	cp bee $(PREFIX)/bin/bee

clean:
	rm -rf bee build examples/hello examples/fib examples/strings *.bee.c

.PHONY: test install clean
