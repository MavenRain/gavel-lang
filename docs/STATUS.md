# gavel-lang status

The build implements slice S8 of `docs/PLAN.md`: the Cubical TT kernel of
ratify-lang a46d5df (a port of permit-lang 3d47120), R0 in the kernel and
the Wasm writer, ported from ratify-lang 71f2c22, without the affine layer
(Q-S0-14 (a), Q-S2-1 (a), Q-S3-1 (a)), the auction rule (Q-S4-1 (a) to
Q-S4-7 (a)), the committee (Q-S5-1 (a) to Q-S5-11 (a)) and the sealed bid
(Q-S6-1 (b) to Q-S6-9 (a)) the log (Q-S7-1 (a) to Q-S7-10 (a)) and the machine `Word`, the field and
the C oracle (Q-S8-1 (a) to Q-S8-9 (a)). It does not complete M0. S8 is
DONE (commit pending). The next slice is S8a: Poseidon, keccak-256, the G1
curve and the Schnorr check.

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
- The committee in `domain/domain.lang` (S5): `Hash`, `Party`, `Members`,
  `Committee` (members, t, f), `Certificate`, `Params` (rule, reserve,
  committee, amend threshold ta), `checkCert` and `outcomeOf`, with the
  Size and Sum words `szLe`, `szLt`, `szEq`, `szAdd`, `sAnd`, `sOr`,
  `sNot`, `ptEq`, `hsEq`, `mbLen`, `mbHas` and `mbDistinct`. G1 to G3 are
  `Path` proof fields of `Committee` and `Params`, checked by the kernel
  as types. A field that does not check gives `REFUSE_SAFETY` (G1),
  `REFUSE_CENSOR` (G2) or `REFUSE_FORK` (G3). A closed committee with more
  than 1024 members gives `REFUSE_BOUND`.
- The sealed bid in `domain/domain.lang` (S6): `Slot`, `Height`, the
  deadline fields of `Params` (`prSlot`, `prStampEnd`, `prRelease`, and the
  proof field `prOrder`), `AucID`, `Root`, `Handle`, `Commitment`,
  `Deposit`, `EligibilityProof`, `Sealed`, `SealedBid`, `Reason`,
  `Verdict`, `elgVrf`, `admit`, `slash`, `Opening` and `release`. P2 is the
  proof field `opOk` of `Opening`. P3 is the path law `p3Slash` on `slash`.
  A guest def that reads a `Sealed` value, or an `opOk` field that does
  not check, gives `REFUSE_PEEK`. A `prOrder` field that does not check
  gives `REFUSE_DEADLINE`. A closed `SealedBids` or `Handles` list with
  more than 1024 entries gives `REFUSE_BOUND`.
- The log in `domain/domain.lang` (S7): `Ballot`, `Ballots`, `Amendment`,
  `authorize`, `amendCheck`, `Entry` (`enAmend`, `enSlash`, `enAdmit`),
  `Log` (`lgNil`, `lgCons`), `append`, `paramsAt`, `Machine`, `Verdicts`,
  `replay` and `slashedOf`. G4 is `appendNilL`, `appendNilR` and
  `appendAssoc`. G5 is the type of the proof field `eaOk` of `enAmend`: an
  `eaOk` that does not check gives `REFUSE_FIXPOINT`. P3 on the log is
  `p3Replay`. A name from `now`, `block`, `timestamp`, `time`, `clock`,
  `epoch`, `deadline`, `timeout` and `expires` gives `REFUSE_CLOCK`: time
  is the log height. A closed `Ballots` list with more than 1024 ballots
  gives `REFUSE_BOUND`. The S7 word is `amendCheck`, not `amend`, because
  the DAO prelude has a word `amend`.
- The machine `Word` (S8): a uint32 value that wraps, with the ops `wAdd`,
  `wSub`, `wMul`, `wMulHi`, `wAnd`, `wOr`, `wXor`, `wNot`, `wShl`, `wShr`,
  `wRotl`, `wRotr`, `wLt` and `wEq`, and the literals `0x1fw` and `31w`
  (`LEX_WORD_RANGE` above 0xffffffff). A guest program that names the
  type, an op or a literal gives `REFUSE_WORD`; only `domain/domain.lang`
  and `langc --kit` may. The old bag family `Word` is now `SwapWord`.
  `Word` lowers to Wasm i32 ops (`examples/crypto.lang`).
- The field (S8): `Fe` (8 Words, Montgomery form inside, canonical at each
  boundary), `FeMod`, `frMod`, `fpMod`, `feAdd`, `feSub`, `feMul`,
  `feToMont`, `feFromMont`, `fePow`, `feInv`, `Fr` and `Fp`. The C oracle
  is `build/cryptotool` (`src/crypto` from quorum-lang 8d1a113). It prints
  424 rows, and `test/gate.sh` evaluates each one (`test/crypto/crypto.lang`
  holds the field rows, outside the Wasm set). A declaration or a name
  `ckks`, `approx`, `approximate`, `float` or `double` gives
  `REFUSE_APPROX`.
- The state and history part of the kernel (`state`, `REFUSE_STATE`,
  `REFUSE_HISTORY_WRITE`), byte equal to a46d5df (Q-S2-4 (a)).
- R0 (`docs/PLAN.md` section 2): the Nat operations are total, and a
  partial kit word gives `REFUSE_PARTIAL`.
- The IR lowering (`src/front/lower.c`, `src/ir.h`) and the Wasm writer
  `src/wasm.c`. The writer emits no trap form; `test/notraptool.c` checks
  each built module. The entry sets a Flag argument above 1 to 1.
- The gate: 38 examples, 18 parse refusals, 186 check refusals, 10
  kit-only refusals, 1 time hint, 150 evals, 424 crypto oracle rows with 0
  mismatches, 1 build, 1 ir lowered, 5 ir refusals, 2 ir lowerings, 4
  notrap modules, 7 notrap bad rows, 10 spine evals, 41 Wasm modules
  valid, 1846 differential cases with 0 mismatches;
  the review, domain and path regressions. Each differential module
  passes `build/notraptool`.

## Remaining M0 work

1. S8a to S8c: the crypto layers of `docs/PLAN.md` section 12 (the crypto
   prelude). Each slice needs the slice before it; S8a needs the S8 commit
   first.
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
- The `Party` key is an abstract `Size` until S8a, and `checkCert` matches
  an entry on its signer and message (OI-25).
- An open committee passes the check at any size. The T1 entry bounds it
  (OI-26).
- G3 sets no upper law on ta. A ta above n - f stops the amendments by
  the honest members (OI-24).
- `elgVrf` is an ideal check until S8b (OI-28). The message pairing is the
  abstract Cantor code until S8a (OI-30). `Sealed` is the clear denotation
  of the ciphertext and the shares until S13 and S14 (OI-31).
- A release is outside the Log (OI-35). The Log and `Verdicts` have no
  bound; only `Ballots` has one (OI-36).
- In a guest program, `amSucc` applies only to a literal Amount
  (`REFUSE_AMOUNT`). `examples/log.lang` uses literal Amounts.

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
