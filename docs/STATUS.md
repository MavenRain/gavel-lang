# gavel-lang status

The build implements slice S8c of `docs/PLAN.md`: the Cubical TT kernel of
ratify-lang a46d5df (a port of permit-lang 3d47120), R0 in the kernel and
the Wasm writer, ported from ratify-lang 71f2c22, without the affine layer
(Q-S0-14 (a), Q-S2-1 (a), Q-S3-1 (a)), the auction rule (Q-S4-1 (a) to
Q-S4-7 (a)), the committee (Q-S5-1 (a) to Q-S5-11 (a)) and the sealed bid
(Q-S6-1 (b) to Q-S6-9 (a)) the log (Q-S7-1 (a) to Q-S7-10 (a)) and the machine `Word`, the field and
the C oracle (Q-S8-1 (a) to Q-S8-9 (a)) and the crypto prelude part 1:
keccak-256, Poseidon, the G1 curve and the Schnorr check (Q-S8a-1 (a) to
Q-S8a-10 (a)) and the crypto prelude part 2: the BN254 pairing and the
Groth16 verifier of the Noir proofs (Q-S8b-1 (a) to Q-S8b-20 (a)) and the
crypto prelude part 3: ChaCha20, the seed input and the MPC output check
(Q-S8c-2 to Q-S8c-5). It does not complete M0. S8c is DONE (commit
pending). The next slice is S9: the Wasm module with the prelude.

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
  581 rows (424 at S8, 501 at S8a, 537 at S8b), and `test/gate.sh` evaluates each one
  (`test/crypto/crypto.lang` holds the field rows and the S8a rows,
  outside the Wasm set). A declaration or a name `ckks`, `approx`,
  `approximate`, `float` or `double` gives `REFUSE_APPROX`.
- The crypto prelude part 1 (S8a, Q-S8a-1 (a) to Q-S8a-10 (a)):
  keccak-256 (`keccakF`, `kcChallenge`), Poseidon circomlib t=3 on Fr
  (`poseidon2`; the round constants and the MDS values are data in
  `domain/domain.lang`), the G1 curve `Pt1` (`ptMul`) and
  `schnorrCheck`. The C builtins `feMulC` and `ptMulC` compute the field
  multiply and the scalar multiply. The guest `feMul` and `ptMul` stay
  the spec, and Wasm runs the spec. `hsMsg t x y` is poseidon2(t,
  poseidon2(x, y)). `Hash` holds an Fe tag (`hsOf` makes one from a
  `Size`), `Party` holds a `Pt1` key, and each certificate entry holds a
  signature (`ctR`, `ctS`). `ctHas` runs `schnorrCheck` on the entries of
  the signer. `szPair` and `szTri` are out. The fixtures (`fxKey1` to
  `fxKey5` and the signed entries) come from `build/cryptotool fixtures`.
  The kernel adds the open `wEq` rule, the sealed defs `hsMsg` and
  `schnorrCheck`, and the memo of closed sealed applications
  (`docs/PLAN.md` section 8.1). The `build/cryptotool` self-check
  verifies the Schnorr rows with the C `sc_verify`.
- The crypto prelude part 2 (S8b, Q-S8b-1 (a) to Q-S8b-20 (a)): the BN254
  pairing and the Groth16 verifier. `pairCheckC` is the C pairing check
  (a kit builtin). It also checks that each point is on its curve and
  that each G2 point is in the subgroup. The guest tower, Miller loop and
  final exponentiation (`pairCheck`) are the spec.
  `test/crypto/pair-spec.lang` compares the spec with the C oracle (16
  rows). `g16Verify vk pubs a b c` is the Groth16 check (sealed): the
  canonical checks, the MSM of the publics with `ptMulC`, then one
  `pairCheckC` on 4 pairs. `EligibilityProof` is `elgProof (epA : Pt1)
  (epB : Pt2) (epC : Pt1)`, and `elgVrf` runs `g16Verify eligVk` on the
  root, the handle and the commitment. `outcomeVrf` and `shareVrf` do the
  same for `OutcomeProof` and `ShareProof` with `outcomeVk` and
  `shareVk`. The keys are domain data. `test/noir/` holds 4 golden proofs
  (`elig`, `elig2`, `outcome`, `share`) of Noir circuits: the eligibility
  circuit is Alg 2 of the paper, and the outcome and share circuits are
  stubs. The gate is offline: it does not run the generation tools.
  `build/cryptotool noir NAME DIR` checks a golden proof in C and prints
  its def lines, and `test/crypto/g16.lang` has 18 rows for the guest
  verifier. `examples/sealed.lang` and `examples/log.lang` use the golden
  statements. Each declaration has a fresh fuel of 40000000 steps.
- The state and history part of the kernel (`state`, `REFUSE_STATE`,
  `REFUSE_HISTORY_WRITE`), byte equal to a46d5df (Q-S2-4 (a)).
- R0 (`docs/PLAN.md` section 2): the Nat operations are total, and a
  partial kit word gives `REFUSE_PARTIAL`.
- The crypto prelude part 3 (S8c, Q-S8c-2 to Q-S8c-5): ChaCha20 of RFC
  8439 as guest defs over `Word` (`ccQr`, `ccBlock`, `ccBlockOf`), the
  family `Seed` (8 Words) and `sdExpand sd tag ix`: one block of 16 Words
  with the seed as the key, the block index as the counter and the nonce
  (tag, 0, 0). `ccGate` keeps a block stuck at check time while the first
  seed word is open. `test/crypto/chacha.lang` compares the defs with the
  C oracle (34 rows; the known answers are from RFC 8439). `outcomeAtt c a
  h p cs` is the MPC output check: the certificate must have a signature
  of each member of the committee on the outcome message of the auction,
  the winner handle and the price. `test/crypto/att.lang` has its 10 rows.
  It rejects noncanonical winner handles, so signatures cannot authorize
  a different handle with the same modular hash.
- The IR lowering (`src/front/lower.c`, `src/ir.h`) and the Wasm writer
  `src/wasm.c`. The writer emits no trap form; `test/notraptool.c` checks
  each built module. The entry sets a Flag argument above 1 to 1.
- The gate: 38 examples, 18 parse refusals, 192 check refusals, 10
  kit-only refusals, 1 time hint, 152 evals, 581 crypto oracle rows with 0
  mismatches, 4 golden Noir proofs, 1 build, 1 ir lowered, 5 ir refusals, 3 ir lowerings, 4
  notrap modules, 7 notrap bad rows, 10 spine evals, 42 Wasm modules
  valid, 1910 differential cases with 0 mismatches;
  the review, domain and path regressions. Each differential module
  passes `build/notraptool`.

## Remaining M0 work

1. S9 to S22: the targets T1 to T5, A1 to A7 and the close. S9 needs
   the S8c commit first.

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
- `hsMsg`, `schnorrCheck` and `g16Verify` are sealed: on an open argument
  they do not unfold, so a law cannot look inside them (Q-S8a-9 (a)). The
  memo holds at most 256 rows; above that, a closed application is
  computed again.
- The Wasm path of `ptMulC` has no differential row. `test/ir/femul.lang`
  covers `feMulC` only.
- One gate run at S8b takes about 420 to 630 s of CPU time (about 400 to
  650 s at S8a, 116 s at S8). In one S8a run of 645 s,
  the Wasm and differential section (42 modules, 1910 cases) took 403 s,
  and the crypto oracle (501 rows) took 150 s. The cause of the increase
  is not known.
- An open committee passes the check at any size. The T1 entry bounds it
  (OI-26).
- G3 sets no upper law on ta. A ta above n - f stops the amendments by
  the honest members (OI-24).
- `elgVrf` is a Groth16 check from S8b (OI-28 is closed). The keys in
  `test/noir/` are for tests only: the setup entropy is fixed, and the
  outcome and share circuits are stubs that bind only their 3 publics
  (OI-39).
- AucID is a private witness of the eligibility circuit, so one deposit
  can get a second handle with a second AucID (OI-37).
- A full guest pairing does not fit the 1 GiB arena. The gate compares
  the spec with the C oracle on value rows and short chain rows, and
  `g16Verify` uses `pairCheckC` (OI-38). No gate row runs the pairing in
  Wasm (OI-17).
- `Sealed` is the clear denotation of the ciphertext and the shares until
  S13 and S14 (OI-31).
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
