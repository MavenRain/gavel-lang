# gavel-lang status

The build implements slice S4 of `docs/PLAN.md`: the Cubical TT kernel of
ratify-lang a46d5df (a port of permit-lang 3d47120), R0 in the kernel and
the Wasm writer, ported from ratify-lang 71f2c22, without the affine layer
(Q-S0-14 (a), Q-S2-1 (a), Q-S3-1 (a)), and the auction rule (Q-S4-1 (a) to
Q-S4-7 (a)). It does not complete M0. The next slice is S5: `Party`,
`Committee`, `Certificate`, `Params`, `checkCert`.

## Implemented

- The front end in `src/front/`: lexer, parser, elaboration and type
  check, evaluation.
- Pi and Sigma types, and the universes up to the largest level. A
  universe above it gives `TYPE_UNIVERSE`.
- The interval, path types and path application `@`. The end check of a
  path gives `PATH_END` (or `PATH_UIP` when the ends are paths).
- Transport, hcomp, Glue and `ua`. The face check of glue gives
  `GLUE_BOUNDARY`.
- Families from `domain/domain.lang`, with indices (former F15), path
  constructors and `fold`.
- The kernel prelude in `domain/domain.lang` (`domain/README.md` lists
  the names). The gavel domain words start at S4.
- The auction rule in `domain/domain.lang` (S4): `Amount` (compare
  `amLe`, select `amSelect`, max `amMax`, second max `amMin`), `Bids`,
  `Rule`, `Outcome` and `outcome`, and the laws A1, A2 and A4 as `Path`
  terms. In a program, any other read of an `Amount` gives
  `REFUSE_AMOUNT`, and a closed bid list with more than 1024 bids gives
  `REFUSE_BOUND`.
- The state and history part of the kernel (`state`, `REFUSE_STATE`,
  `REFUSE_HISTORY_WRITE`), byte equal to a46d5df (Q-S2-4 (a)).
- R0 (`docs/PLAN.md` section 2): the Nat operations are total, and a
  partial kit word gives `REFUSE_PARTIAL`.
- The IR lowering (`src/front/lower.c`, `src/ir.h`) and the Wasm writer
  `src/wasm.c`. The writer emits no trap form; `test/notraptool.c` checks
  each built module. The entry sets a Flag argument above 1 to 1.
- The gate: 32 examples, 17 parse refusals, 163 check refusals, 10
  kit-only refusals, 1 time hint, 83 evals, 1 build, 1 ir lowered, 5 ir
  refusals, 2 ir lowerings, 4 notrap modules, 7 notrap bad rows, 10 spine
  evals, 35 Wasm modules valid, 998 differential cases with 0 mismatches;
  the review, domain and path regressions. Each differential module
  passes `build/notraptool`.

## Remaining M0 work

1. S5 to S8c: the governance and crypto layers of `docs/PLAN.md` section
   12 (`Party`, `Committee`, `Certificate`, `SealedBid`, `Log`, `Machine`,
   the crypto prelude). Each slice needs the slice before it; S5 needs
   the S4 commit first.
2. S9 to S22: the targets T1 to T5, A1 to A7 and the close.

## Known limits

- The affine syntax (`-o`, `@0`, `@1`, `unpack`, `Budget`) is not in the
  language (Q-S0-14 (a), Q-S2-3 (a)). Four gate rows give a parse or
  check refusal for it (`LEX_CHAR`, `NAME_UNKNOWN`, `TYPE_MISMATCH`), not
  an affine code.
- `REFUSE_CELL` holds the CM and CC shape check inline in the kernel.
  The R1 slice renames it or adds `REFUSE_FORMER`.
- `Amount` is a unary family in the kernel. T1 lowers it to a
  fixed-width word (`docs/PLAN.md` OI-21).
- An open bid list passes the check at any length. The T1 entry bounds it
  (OI-22).
- The kernel signature slot is `DaoOutcome`, not `Outcome` (OI-23).

## Internal boundaries

`src/front/lexer.c` and `src/front/parser.c` read text into terms
(`src/front/ast.h`). `src/front/check.c` elaborates terms to core terms
(`src/front/core.h`) and gives the refusal codes. `src/front/eval.c`
evaluates core terms. `src/front/front.c` loads the domain, then the
program. `src/front/lower.c` lowers a checked definition to the IR
(`src/ir.h`), and `src/wasm.c` writes a Wasm module from the IR.
`src/main.c` holds the commands.

`gen/embed.c` copies `domain/domain.lang` into `build/domain.c` at build
time, so the domain is part of `build/langc`.

`test/gate.sh` runs the examples and the rows in `test/parse/`,
`test/check/` and `test/eval/` against their `expect.txt` files, and the
IR, Wasm and differential rows. `build/notraptool` checks each built Wasm
module for a trap form.
`test/review.sh` runs the review regressions. `build/fronttool` runs the
domain regressions and `build/pathtool` the path regressions.
`make check` also compiles all sources with clang and `-Werror`.
