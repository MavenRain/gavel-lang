#!/bin/sh
# The kit gate. `make check` runs it from the kit directory after the build.
set -eu
fail=0
count=0
# kit LIST FILE prints --kit when FILE is a line of LIST. A row whose text uses a
# kit name or a Nat or word literal is a kit row (orbit-lang S3); the others run
# in guest mode.
kit() {
  if [ -f "$1" ] && rg -q -x -F "$2" "$1"; then echo --kit; fi
}
for prog in examples/*.lang; do
  out=$(build/langc $(kit examples/kit.txt "${prog#examples/}") check "$prog" 2>&1) || true
  count=$((count + 1))
  if [ "$out" != "ok" ]; then
    echo "FAIL $prog: $out"
    fail=$((fail + 1))
  fi
done
echo "examples: $count checked, $fail failed"

tmp=${TMPDIR:-/tmp}/langc-gate.$$
mkdir -p "$tmp"
awk 'BEGIN { printf "def deep : Nat := " } { } END {
  for (i = 0; i < 1100; i++) printf "("; printf "1"; for (i = 0; i < 1100; i++) printf ")"; print "" }' </dev/null >"$tmp/deep.lang"

# refusals DIR LABEL: each line of DIR/expect.txt is `FILE CODE`. `langc check`
# must exit 1 and print `langc: CODE: ...` on stderr.
refusals() {
  checked=0
  while read -r file code; do
    path="$1/$file"
    [ "$file" = "deep.lang" ] && path="$tmp/deep.lang"
    status=0
    build/langc $(kit "$1/kit.txt" "$file") check "$path" 2>"$tmp/err" >/dev/null || status=$?
    first=$(head -n 1 "$tmp/err")
    checked=$((checked + 1))
    case "$status:$first" in
      "1:langc: $code: "*) ;;
      *) echo "FAIL $file: want exit 1 and $code, got exit $status: $first"; fail=$((fail + 1)) ;;
    esac
  done <"$1/expect.txt"
  echo "$2 refusals: $checked checked"
}
refusals test/parse parse
refusals test/check check

# A REFUSE_KIT row refuses a kit item in guest mode only: `langc --kit check`
# must print ok.
kit_only=0
while read -r file code; do
  [ "$code" = "REFUSE_KIT" ] || continue
  status=0
  out=$(build/langc --kit check "test/check/$file" 2>&1) || status=$?
  kit_only=$((kit_only + 1))
  if [ "$status:$out" != "0:ok" ]; then
    echo "FAIL $file: want ok under --kit, got exit $status: $out"
    fail=$((fail + 1))
  fi
done <test/check/expect.txt
echo "kit-only refusals: $kit_only checked"

# A refl refusal shows both normal forms.
build/langc $(kit test/check/kit.txt bad-refl.lang) check test/check/bad-refl.lang 2>"$tmp/err" >/dev/null || true
if ! rg -q -F 'the path at i1 is some 1201, but the type needs some 1200' "$tmp/err"; then
  echo "FAIL bad-refl.lang: the message does not show both normal forms"
  fail=$((fail + 1))
fi

# A REFUSE_CLOCK refusal names the log height form (S3, O3; S7).
time_hint=0
build/langc $(kit test/check/kit.txt time-block.lang) check test/check/time-block.lang 2>"$tmp/err" >/dev/null || true
time_hint=$((time_hint + 1))
if ! rg -q -F 'time is the log height: use a `Height` (a position in the log)' "$tmp/err"; then
  echo "FAIL time-block.lang: the message does not name the log height form"
  fail=$((fail + 1))
fi
echo "time hint: $time_hint checked"

# Each line of test/eval/expect.txt is `FILE NAME [ARGS] => OUTPUT`. `langc eval`
# must exit 0 and print OUTPUT.
evals=0
while IFS= read -r line; do
  want=${line#* => }
  set -- ${line%% => *}
  file=$1
  name=$2
  shift 2
  status=0
  got=$(build/langc $(kit examples/kit.txt "$file.lang") eval "examples/$file.lang" "$name" "$@" 2>"$tmp/err") || status=$?
  first=$(head -n 1 "$tmp/err")
  evals=$((evals + 1))
  case "$status|$got|$first" in
    "0|$want|"*) ;;
    *) echo "FAIL eval $file $name: want $want, got exit $status: $got $first"; fail=$((fail + 1)) ;;
  esac
done <test/eval/expect.txt
echo "evals: $evals checked"

# eval_refused STATUS CODE NAME [ARGS]: `langc --kit eval examples/entries.lang
# NAME ARGS` must exit STATUS and print `langc: CODE: NAME: ...` on stderr.
eval_refused() {
  want_status=$1
  code=$2
  shift 2
  status=0
  build/langc --kit eval examples/entries.lang "$@" 2>"$tmp/err" >/dev/null || status=$?
  case "$status:$(head -n 1 "$tmp/err")" in
    "$want_status:langc: $code: $1: "*) ;;
    *) echo "FAIL eval $*: want exit $want_status and $code, got exit $status"; fail=$((fail + 1)) ;;
  esac
}
eval_refused 2 EVAL_ARGS addPrice 1
eval_refused 2 EVAL_ARGS addPrice x 1
eval_refused 2 EVAL_ARGS addPrice 18446744073709551616 1
eval_refused 2 EVAL_ARGS pick 2 4 9
eval_refused 1 EVAL_ENTRY nope

# The target (S3). The Wasm writer is in the build: `langc build
# examples/functions.lang` must exit 0 and write a module that wasm-validate
# accepts.
builds=0
status=0
build/langc $(kit examples/kit.txt functions.lang) build examples/functions.lang -o "$tmp/functions.wasm" 2>"$tmp/err" || status=$?
builds=$((builds + 1))
if [ "$status" -ne 0 ] || ! wasm-validate "$tmp/functions.wasm" 2>>"$tmp/err"; then
  echo "FAIL build functions: want exit 0 and a valid module, got exit $status: $(head -n 1 "$tmp/err")"
  fail=$((fail + 1))
fi
echo "builds: $builds checked"

# The IR (S3): `langc ir examples/functions.lang` must exit 0 and give one
# func line for each line of `langc abi`.
irs=0
status=0
build/langc $(kit examples/kit.txt functions.lang) ir examples/functions.lang >"$tmp/ir.txt" 2>"$tmp/ir.err" || status=$?
irs=$((irs + 1))
funcs=$(rg -c '^func ' "$tmp/ir.txt" || echo 0)
entries=$(build/langc $(kit examples/kit.txt functions.lang) abi examples/functions.lang | rg -c '.' || echo 0)
[ "$status" -eq 0 ] && [ "$funcs" -gt 0 ] && [ "$funcs" -eq "$entries" ] || { echo "FAIL ir functions: exit $status, $funcs funcs for $entries entries: $(head -n 1 "$tmp/ir.err")"; fail=$((fail + 1)); }
echo "ir: $irs lowered"

# The IR refusals (S3): `langc --kit check` must accept each file, and
# `langc --kit ir` must exit 1 and print `langc: ` and the given text first.
# closure.lang gives a function value at run time (REFUSE_RUNTIME_CLOSURE).
# kmap.lang uses a KMap operation at run time (a refuse() arm of lower.c).
# tree.lang folds a tree that exists only at run time (REFUSE_RUNTIME_TREE).
# spine.lang walks lists that exist only at run time, so its heap use has no
# static bound (S3).
irrefusals=0
ir_refused() {
  status=0
  build/langc --kit check "test/ir/$1" > /dev/null 2>&1 || status=$?
  [ "$status" -eq 0 ] || { echo "FAIL $1: check exit $status"; fail=$((fail + 1)); }
  status=0
  first=$(build/langc --kit ir "test/ir/$1" 2>&1 > /dev/null) || status=$?
  case "$status:$first" in
    "1:langc: $2"*) ;;
    *) echo "FAIL $1: want exit 1 and $2, got exit $status: $first"; fail=$((fail + 1)) ;;
  esac
  irrefusals=$((irrefusals + 1))
}
ir_refused closure.lang "REFUSE_RUNTIME_CLOSURE: bad: "
ir_refused kmap.lang "REFUSE_LOWER: got: the Wasm host does not lower an elim, KMap"
ir_refused tree.lang "REFUSE_RUNTIME_TREE: tips: "
ir_refused spine.lang "REFUSE_LOWER: trapHead: heap use has no static bound"
ir_refused hop.lang "REFUSE_LOWER: hopDepth: a recursive fold over a block of families"
echo "ir refusals: $irrefusals checked"

# The IR lowerings (S3): `langc
# --kit check` must accept each file and `langc --kit ir` must exit 0. The
# IR text of natops.lang and of block.lang (S3) must be
# test/ir/natops.ir and test/ir/block.ir (golden files: a change of the
# lowering changes the text).
irlowered=0
ir_lowered() {
  status=0
  build/langc --kit check "test/ir/$1" > /dev/null 2>&1 || status=$?
  [ "$status" -eq 0 ] || { echo "FAIL $1: check exit $status"; fail=$((fail + 1)); }
  status=0
  build/langc --kit ir "test/ir/$1" >"$tmp/$1.ir" 2>"$tmp/ir.err" || status=$?
  [ "$status" -eq 0 ] || { echo "FAIL $1: want ir exit 0, got exit $status: $(head -n 1 "$tmp/ir.err")"; fail=$((fail + 1)); }
  irlowered=$((irlowered + 1))
}
ir_lowered natops.lang
cmp -s "$tmp/natops.lang.ir" test/ir/natops.ir || { echo "FAIL natops.lang: the IR text is not test/ir/natops.ir"; fail=$((fail + 1)); }
ir_lowered block.lang
cmp -s "$tmp/block.lang.ir" test/ir/block.ir || { echo "FAIL block.lang: the IR text is not test/ir/block.ir"; fail=$((fail + 1)); }
echo "ir lowerings: $irlowered lowered"

# The writer (S3): `langc --kit build
# test/ir/natops.lang` must exit 0 and write a module that wasm-validate
# accepts. In `wasm-objdump -x`, memory 0 must have 1 page (minimum =
# maximum = the pages of the largest heap bound, at least 1). The module of
# examples/functions.lang (the builds rows) has 0 heap words: its memory 0
# must also have 1 page. The Wasm runs and the differential lines run node:
# they come back at S8 (Q-S3-2 (b)).

status=0
build/langc --kit build test/ir/natops.lang -o "$tmp/natops.wasm" 2>"$tmp/err" || status=$?
if [ "$status" -ne 0 ] || ! wasm-validate "$tmp/natops.wasm" 2>>"$tmp/err"; then
  echo "FAIL build natops: want exit 0 and a valid module, got exit $status: $(head -n 1 "$tmp/err")"
  fail=$((fail + 1))
fi
# The module of test/ir/block.lang (S3): the same build
# check. The notrap row below uses it.
status=0
build/langc --kit build test/ir/block.lang -o "$tmp/block.wasm" 2>"$tmp/err" || status=$?
if [ "$status" -ne 0 ] || ! wasm-validate "$tmp/block.wasm" 2>>"$tmp/err"; then
  echo "FAIL build block: want exit 0 and a valid module, got exit $status: $(head -n 1 "$tmp/err")"
  fail=$((fail + 1))
fi
# The module of examples/state-no-history.lang (Q-S3-3 (b)): build lowers a
# state family as a plain family. The same build check; the notrap row
# below uses it. Its differential row comes at S8 (Q-S3-2 (b)).
status=0
build/langc $(kit examples/kit.txt state-no-history.lang) build examples/state-no-history.lang -o "$tmp/state-no-history.wasm" 2>"$tmp/err" || status=$?
if [ "$status" -ne 0 ] || ! wasm-validate "$tmp/state-no-history.wasm" 2>>"$tmp/err"; then
  echo "FAIL build state-no-history: want exit 0 and a valid module, got exit $status: $(head -n 1 "$tmp/err")"
  fail=$((fail + 1))
fi
for mod in natops functions; do
  wasm-objdump -x "$tmp/$mod.wasm" 2>/dev/null | rg -q -x -F ' - memory[0] pages: initial=1 max=1' ||
    { echo "FAIL $mod.wasm: memory 0 is not initial=1 max=1"; fail=$((fail + 1)); }
done
# build/notraptool (test/notraptool.c, S3) must accept the
# modules of natops.lang, block.lang, functions.lang and
# state-no-history.lang: exit 0 and no output. It fails a module with unreachable, memory.grow, an opcode that
# is not in src/wasmop.h, an i64.div_u or i64.rem_u with no divisor guard,
# or a memory with no maximum or a maximum that is not the minimum.
notrapmods=0
for mod in natops block functions state-no-history; do
  notrapmods=$((notrapmods + 1))
  status=0
  got=$(build/notraptool "$tmp/$mod.wasm" 2>&1) || status=$?
  [ "$status" -eq 0 ] && [ -z "$got" ] ||
    { echo "FAIL notrap $mod.wasm: want exit 0 and no output, got exit $status: $got"; fail=$((fail + 1)); }
done
echo "notrap modules: $notrapmods checked"
# The bad rows (S3): each line of test/notrap/bad/expect.txt is
# `NAME MESSAGE`. wat2wasm makes $tmp/NAME.wasm from test/notrap/bad/NAME.wat.
# build/notraptool must exit 1 and print the one line
# `notraptool: $tmp/NAME.wasm: MESSAGE`.
notrapbad=0
while read -r name msg; do
  notrapbad=$((notrapbad + 1))
  status=0
  got=$(wat2wasm "test/notrap/bad/$name.wat" -o "$tmp/$name.wasm" 2>&1 && build/notraptool "$tmp/$name.wasm" 2>&1) || status=$?
  [ "$status" -eq 1 ] && [ "$got" = "notraptool: $tmp/$name.wasm: $msg" ] ||
    { echo "FAIL notrap bad $name: want exit 1 and $msg, got exit $status: $got"; fail=$((fail + 1)); }
done <test/notrap/bad/expect.txt
echo "notrap bad rows: $notrapbad checked"
# The kit evals of test/ir/spine.lang at 4. Each line is `NAME VALUE`.
spineevals=0
while read -r name want; do
  got=$(build/langc --kit eval test/ir/spine.lang "$name" 4 2>&1) || got="exit $?: $got"
  spineevals=$((spineevals + 1))
  [ "$got" = "$want" ] || { echo "FAIL spine $name 4: want $want, got $got"; fail=$((fail + 1)); }
done <<'EOF'
foldOnes 8
mapUp 20
bindUp 50
filterUp 3
trapHead 1
trapSum 3
filterTrap 7
bindTrap 1
sizeSum 20
sizeTrap 4
EOF
echo "spine evals: $spineevals checked"

# The Wasm target (S3, OI-20): each module builds and is valid. Then the differential: on
# the grid of test/grid.awk, node and `langc eval` give the same result for
# each entry. R0 (docs/PLAN.md section 2, OI-4): no target traps, so a
# `trap` line on either side is a FAIL. The stderr of eval is not merged
# into stdout. A file in test/ir/ takes --kit.
wasms=0
cases=0
mismatches=0
difkit() {
  case "$1" in
    test/ir/*) echo --kit ;;
    *) kit examples/kit.txt "$(basename "$1")" ;;
  esac
}
for f in examples/*.lang test/ir/natops.lang test/ir/block.lang test/ir/proof-trap.lang; do
  w="$tmp/$(basename "$f" .lang).wasm"
  wasms=$((wasms + 1))
  if ! build/langc $(difkit "$f") build "$f" -o "$w" 2>"$tmp/w.err" || ! wasm-validate "$w" 2>>"$tmp/w.err" ||
    ! build/notraptool "$w" 2>>"$tmp/w.err" ||
    ! wasm-objdump -x "$w" >/dev/null 2>>"$tmp/w.err"; then
    echo "FAIL wasm $f: $(head -n 1 "$tmp/w.err")"
    fail=$((fail + 1))
    continue
  fi
  build/langc $(difkit "$f") ir "$f" | awk -f test/grid.awk >"$tmp/cases.txt"
  : >"$tmp/want.txt"
  while read -r call; do
    status=0
    want=$(build/langc $(difkit "$f") eval "$f" $call 2>/dev/null) || status=$?
    [ "$status" -eq 0 ] || want="eval-exit-$status"
    echo "$want" >>"$tmp/want.txt"
  done <"$tmp/cases.txt"
  if ! node test/run-wasm.mjs "$w" - <"$tmp/cases.txt" >"$tmp/got.txt" 2>"$tmp/w.err"; then
    echo "FAIL node $f: $(head -n 1 "$tmp/w.err")"
    fail=$((fail + 1))
    continue
  fi
  cases=$((cases + $(rg -c '.' "$tmp/cases.txt" || echo 0)))
  paste -d '|' "$tmp/cases.txt" "$tmp/want.txt" "$tmp/got.txt" | awk -F '|' '$2 != $3' >"$tmp/bad.txt"
  bad=$(rg -c '.' "$tmp/bad.txt" || echo 0)
  mismatches=$((mismatches + bad))
  [ "$bad" -eq 0 ] || { echo "FAIL differential $f: $bad mismatches, first $(head -n 1 "$tmp/bad.txt")"; fail=$((fail + 1)); }
  traps=$(cat "$tmp/want.txt" "$tmp/got.txt" | rg -c -x -F trap || echo 0)
  [ "$traps" -eq 0 ] || { echo "FAIL trap $f: $traps trap lines (R0)"; fail=$((fail + 1)); }
done
echo "wasm: $wasms modules valid"
echo "differential: $cases cases, $mismatches mismatches"
# A Flag argument larger than 1 (langc eval refuses it with EVAL_ARGS). R0
# (docs/PLAN.md section 2, OI-4): the entry sets a Flag argument of 2 or
# more to 1 (flag_norm in src/wasm.c), so the module gives 4 and does not
# trap.
got=$(node test/run-wasm.mjs "$tmp/entries.wasm" pick 2 4 9 2>&1) || true
[ "$got" = "4" ] || { echo "FAIL wasm pick 2 4 9: want 4, got $got"; fail=$((fail + 1)); }

status=0
build/langc 2>/dev/null || status=$?
[ "$status" -eq 2 ] || { echo "FAIL usage: want exit 2, got $status"; fail=$((fail + 1)); }
status=0
build/langc --kit 2>/dev/null || status=$?
[ "$status" -eq 2 ] || { echo "FAIL usage --kit: want exit 2, got $status"; fail=$((fail + 1)); }
status=0
build/langc check "$tmp/missing.lang" 2>/dev/null || status=$?
[ "$status" -eq 2 ] || { echo "FAIL missing file: want exit 2, got $status"; fail=$((fail + 1)); }
rm -rf "$tmp"

if rg -n '\x{2013}|\x{2014}' . >/dev/null; then
  echo "FAIL: an em-dash or en-dash is in the kit"
  fail=$((fail + 1))
fi
echo "gate: $fail failures"
[ "$fail" -eq 0 ]
