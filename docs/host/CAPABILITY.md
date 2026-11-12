# TinyCC Wasm host capability

This file holds host facts only. Put the facts of your domain in the
`probe/CAPABILITY.md` of your language. Paths are relative to the host
kit, or to the root of a generated language. The toolchain was recorded
on 2026-10-08 on AArch64 Darwin. The gate facts below describe gavel-lang S9a.

## Toolchain

- TinyCC 0.9.28rc (AArch64 Darwin) builds `build/langc` with
  `-std=c99 -Wall -Werror` (`Makefile:6`). `PIN` has the exact version.
- `cc -Wall -Wextra -Wswitch-enum -Werror -fsyntax-only` checks the same
  sources (`make check-clang`, `Makefile:7`). Each switch on an enum names
  every case and has no default arm.
- `tcc -run gen/embed.c` turns `domain/domain.lang` into `build/domain.c`
  at build time (`Makefile:20`). The domain is part of the executable.
- The kit uses the C99 `uint64_t` type for `Nat`. Unsigned arithmetic in
  C wraps modulo 2^64, so the evaluator and the IR use explicit carry
  tests to find an overflow.
- `src/wasm.c` writes the Wasm binary format directly: the type,
  function, memory, global, export and code sections, with LEB128
  numbers. It needs no library. The gate validates each module with
  `wasm-validate` and `wasm-objdump -x`, and checks its no-trap forms with
  `build/notraptool`. Node runs the differential rows at S3.

## Values

- `Nat` is an unsigned 64-bit integer. The lexer refuses a literal larger
  than 18446744073709551615 (`test/parse/nat-range.lang`).
- `natAdd` gives `Option Nat`, with `none` on overflow. `natSub` stops at
  0. `natDiv` and `natMod` take a `NonZero Nat` divisor. The kernel has no
  `natMul` word. `docs/host/README.md` gives the no-trap rule.
- There is no text type. The lexer refuses `"` (`test/parse/string.lang`).
- On Wasm, each op is total; a value is one word.

## Overflow tests on Wasm

- `ADD_CARRY` is `a + b < a`, modulo 2^64 (`src/wasm.c:177`).
- `SUB` is `select(a - b, 0, a >= b)`.
- `test/ir/natops.lang` covers the edge value 18446744073709551615:
  `addOr 18446744073709551615 1` is 0 (`natAdd` gives `none`),
  `addOr 18446744073709551615 0` is 18446744073709551615, and
  `pickSecond 18446744073709551615 1` is 1. The S3 gate checks the IR text
  and validates the module. Node and `langc eval` compare each grid case
  in the S3 differential rows.

## Limits

- The checker and the evaluator stop at a depth of 2000
  (`src/front/check.c:14`, `src/front/core.h:14`). The parser stops at a
  nesting of 1000 (`src/front/parser.c:20`). These limits keep the C
  stack bounded. `test/gate.sh` checks a nesting of 1100 parentheses.
- The evaluator has a fuel of 40000000 steps (`src/front/core.h:15`).
  A large loop count can reach `EVAL_FUEL` or the arena limit. The
  differential grid uses large values only for entries with no loop.
- All memory of `langc` comes from one arena with a limit of 1 GiB
  (`src/main.c:12`). A run that reaches it stops with `OOM`.
  `langc eval test/ir/spine.lang foldOnes 4294967296` stops with `OOM`.
- The Wasm memory has a fixed size (`src/wasm.c:374`): minimum = maximum =
  the pages of the largest heap end of an entry (its heap base + its heap
  bound), at least 1 page. The allocator has no check (`src/wasm.c:306`).
  An entry with no static heap bound refuses (`REFUSE_LOWER`,
  `test/ir/spine.lang`).
- The Wasm writer stops at a statement nesting of 2000 (`src/wasm.c:33`).
  The IR printer stops at 4000 (`src/front/lower.c:16`).
- A loop over a list spine has a fuel of 2^63 steps
  (`src/front/lower.c:913`). A spine walk whose allocations exceed the
  static heap limit refuses during lowering. In a host-form entry, the
  walk over a list with a bound (`Bids`: 1024, `src/front/check.c:4283`)
  has the fuel bound + 1 (`src/front/lower.c:1400`). A list above the
  bound gives the status 1, never a short walk.
- `langc eval` takes a kit `Amount` argument from 0 to 16
  (`EVAL_AMOUNT_MAX`, `src/front/check.c:5264`): `leOf 17 0` gives
  `EVAL_ARGS` (exit 2). The evaluator keeps `Amount` unary, and the inner
  elim of `amLe` and `amSelect` computes each `ih` before the step, so one
  compare costs about 2^min(x, y) arena units. From about 21, a compare
  stops with `OOM`. In Wasm an Amount is one `i64` word, with no such
  limit.
- The static heap bound of a loop is the heap of its body times its fuel.
  `bdArg` and `bdSecond` walk the bids inside a walk step, so the bound of
  an outcome entry grows with the square of the list bound: `hlOut`
  (`test/ir/hostlist.lang`) has 6322202 heap words (about 50.6 MB, 772
  pages of 64 KiB). Each instance of the module has these pages. S9a has
  no heap reset for each loop step.
- `langc eval` runs an outcome row only up to about 48 bids: 64 bids give
  `OOM` in the 1 GiB arena. The gate rows with 64 and 1024 bids are golden
  rows: node only, and the want text comes from a Python oracle.
- A host-form entry has a bounded list parameter or a family result.
  S9a takes only these forms, and another definition is not an entry:
  the result family is a family of the domain with no recursive field,
  and each field is a word family or a scalar (`Outcome`); a family that
  the program declares is not a result family, so each old module keeps
  its bytes; each list parameter has the form `nil | cons (head : Amount)
  (tail : F)` with a bound, and it comes after each scalar parameter.
- In a module with a host-form entry, an entry named `memory` refuses:
  `REFUSE_LOWER: memory: the name memory clashes with the memory export of
  the host form` (`src/front/lower.c:1936`, `test/ir/memclash.lang`,
  Q-S9-39 (a)). A module with no host-form entry has no memory export, so
  `def memory` stays an entry (`test/ir/memnohost.lang`). The allocator
  `alloc` has no export.
- Source files are at most 1 MiB (`src/front/front.h:7`).

## Measured numbers

- At S9a, `make check` validates 48 Wasm modules with `wasm-validate`
  and `wasm-objdump`, runs `build/notraptool` on each differential module,
  checks 7 bad modules, and compares 2747 evaluator and Wasm results with
  0 mismatches. The T1 rows of `outcomeT1` give 18 differential and 5
  golden results with 0 mismatches. One gate run takes about 1050 s of
  CPU time.
- `langc --kit build examples/entries.lang -o OUT` writes a module of
  193 bytes for three entries.

## HOST-LIMIT reasons

The IR is first-order (`src/ir.h`). It has words, heap blocks, branches,
counted loops and bounded loops. It has no calls, no function values and
no stack of its own. Two programs need more than that. The lowering
refuses them with a HOST-LIMIT code. These are not defects.

- `REFUSE_RUNTIME_CLOSURE` (`src/front/lower.c:583`): a function value
  must exist at run time, for example as the result of a `fold`. The IR
  has no closures. `test/ir/closure.lang` is the gate case.
- `REFUSE_RUNTIME_TREE` (`src/front/lower.c:1203`): a `fold` walks a value
  of a family with two or more recursive fields, and the value exists
  only at run time. A loop walks one spine. A tree needs recursion or an
  explicit stack. `test/ir/tree.lang` is the gate case.

S9a lowers an `elim` on a run-time value of a family with no recursive
field (a switch) or with one spine (a walk). A tree stays a refusal.
