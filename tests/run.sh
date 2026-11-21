#!/bin/sh
# Regression tests. From the bee directory, after `make`:  tests/run.sh
#   NAME.b + NAME.exp            compile, run, output must equal NAME.exp
#   e_*.b + NAME.err [NAME.line] must be rejected; the message must contain NAME.err,
#                                and be reported on line NAME.line if that file exists
#   w_*.b + NAME.warn            must compile, and warn with the text in NAME.warn
cd "$(dirname "$0")/.." || exit 1
tmp=$(mktemp -d); trap 'rm -rf "$tmp"' EXIT
fail=0
run() { # name [flags]
    name=$1; flags=$2
    if ! ./bee $flags -o "$tmp/$name" "tests/$name.b" 2> "$tmp/$name.msg"; then
        echo "FAIL $name (compile)"; cat "$tmp/$name.msg"; fail=1; return
    fi
    "$tmp/$name" > "$tmp/$name.out"
    if diff -u "tests/$name.exp" "$tmp/$name.out" > "$tmp/$name.diff"; then echo "ok   $name${flags:+ ($flags)}"
    else echo "FAIL $name"; cat "$tmp/$name.diff"; fail=1; fi
}
fails() {
    name=$1
    if ./bee -o "$tmp/$name" "tests/$name.b" 2> "$tmp/$name.msg"; then echo "FAIL $name (should not compile)"; fail=1; return; fi
    if ! grep -qF -- "$(cat "tests/$name.err")" "$tmp/$name.msg"; then echo "FAIL $name (wrong message)"; cat "$tmp/$name.msg"; fail=1; return; fi
    if [ -f "tests/$name.line" ] && ! grep -q ":$(cat "tests/$name.line"): error:" "$tmp/$name.msg"; then
        echo "FAIL $name (wrong line, wanted $(cat "tests/$name.line"))"; cat "$tmp/$name.msg"; fail=1; return
    fi
    echo "ok   $name (rejected)"
}
warns() {
    name=$1
    if ! ./bee -o "$tmp/$name" "tests/$name.b" 2> "$tmp/$name.msg"; then echo "FAIL $name (should compile)"; cat "$tmp/$name.msg"; fail=1; return; fi
    if grep -qF -- "$(cat "tests/$name.warn")" "$tmp/$name.msg"; then echo "ok   $name (warns)"
    else echo "FAIL $name (no warning)"; cat "$tmp/$name.msg"; fail=1; fi
}

# everything in one self-checking program (start here if something breaks)
run all_features
# traditional B
run traditional; run histops; run stress
# escapes, printf, strings, chars
run escapes; run intfmt; run width; run strict -s; run longchar; run chararray; run strings_chars
# modern syntax
run modern_decl; run inferred; run compound; run functions; run floats; run comments
# both styles in one file
run mixed; run dual
# diagnostics
fails toolong; fails chararray_err; fails charsize_err; fails charaddr_err
for f in tests/e_*.b; do fails "$(basename "$f" .b)"; done
for f in tests/w_*.b; do warns "$(basename "$f" .b)"; done
[ $fail = 0 ] && echo "all tests passed"
exit $fail
