#!/bin/sh
# Regression tests: compile each tests/NAME.b, run it, diff against tests/NAME.exp.
# Usage: tests/run.sh   (from the bee directory, after `make`)
cd "$(dirname "$0")/.." || exit 1
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
fail=0
run() { # name flags
    name=$1; flags=$2
    if ! ./bee $flags -o "$tmp/$name" "tests/$name.b"; then echo "FAIL $name (compile)"; fail=1; return; fi
    "$tmp/$name" > "$tmp/$name.out"
    if diff -u "tests/$name.exp" "$tmp/$name.out" > "$tmp/$name.diff"; then echo "ok   $name${flags:+ ($flags)}"
    else echo "FAIL $name"; cat "$tmp/$name.diff"; fail=1; fi
}
run escapes; run intfmt; run width; run strict -s; run stress
exit $fail
