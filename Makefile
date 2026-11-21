CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra

bee: bee.c rt_inc.h
	$(CC) $(CFLAGS) -o $@ bee.c

# turn the runtime header into a C string literal the compiler embeds
rt_inc.h: bee_rt.h
	sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/^/"/' -e 's/$$/\\n"/' bee_rt.h > $@

test: bee
	./bee -o examples/hello examples/hello.b && ./examples/hello

clean:
	rm -f bee rt_inc.h examples/hello examples/fib examples/strings *.bee.c

.PHONY: test clean
