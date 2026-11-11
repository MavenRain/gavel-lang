# Domain slot

In S5 the domain is the kernel prelude, ported from permit-lang 3d47120,
the IR test families of permit-lang 18deea2, the auction rule and the
committee.
`domain/domain.lang` holds the domain. At build time, `tcc -run
gen/embed.c` copies it into `build/domain.c`, so the domain is part of
`build/langc`. `langc` loads the domain first, then the program.

## What the file holds

- `family` declarations. Only this file can declare a family. In a
  program, `family` is refused with `REFUSE_DATA` (rule R1).
- `def` operations, written in the language.
- Line comments that start with `--`.

Each family, constructor, field and definition name in this file becomes
a core name. A program cannot use one of them as a new definition name
(rule R4, `REFUSE_NAME`).

## Family syntax

```
family NAME (param : T)* := ctor (field : T)* | ctor (field : T)* ...
```

- A family with only nullary constructors is an enum.
- A family with one constructor is a record. It gets one projection for
  each field name.
- A field can have the type of its own family. Such a field is recursive.
- A parameter is an index (former F15). A constructor takes the indices
  first.
- A `fold` over a family takes one argument for each constructor, in
  declaration order. A constant constructor takes a value. A constructor
  with fields takes a function. A recursive field arrives folded.

In the IR, a constructor value is a heap block. Word 0 is the constructor
index, in declaration order. Each op is total; a value is one word.
Field K is word 1 + K.
A `fold` over a family with one recursive field lowers to a loop.
A family with two or more recursive fields cannot be folded at run time
(`REFUSE_RUNTIME_TREE`, see `docs/CAPABILITY.md`).

## The prelude

The prelude comes from permit-lang 3d47120 (S2). The kit
sample (`Color`, `Item`, `Stack`, `Tree`, `Lot`) is not in the domain: it
uses `natMul`, and the kernel has no `natMul`.

| Kind | Names |
|---|---|
| Families | `S1`, `Trunc1`, `Trunc0`, `Size`, `Parity`, `Empty`, `Bag`, `FreeMon`, `Pos`, `SwapWord`, `ChoiceBag`, `OrbitN`, `Charter` |
| Path operations | `transport`, `subst`, `J`, `JRefl`, `trans`, `isProp`, `isSet` |
| Size and word operations | `sExtend`, `sDuplicate`, `sizeFlip`, `sizeEven`, `sizePred`, `OpPos`, `consP`, `swapFront`, `actW` |
| Free monoid operations | `fmCons`, `fmIsSet`, `fmFold`, `fmBind`, `fmMap` |
| Equivalences | `isContr`, `fiber`, `isEquiv`, `Equiv`, `idEquiv`, `isoCap`, `isoPath`, `isoToEquiv`, `ua`, `uaArrow` |

Many kernel rows in `examples/`, `test/check/` and `test/eval/expect.txt`
use these names.

## The auction rule

S4 adds the first gavel-lang words (`docs/PLAN.md` section 1, Q-S4-1 (a)
to Q-S4-7 (a)).

| Kind | Names |
|---|---|
| Families | `Amount`, `Bids`, `Rule`, `Outcome` |
| Constructors and fields | `amZero`, `amSucc`, `amPred`, `bdNil`, `bdCons`, `bdHead`, `bdTail`, `firstPrice`, `secondPrice`, `noSale`, `sale`, `slWinner`, `slPrice` |
| Amount operations | `amLe` (compare), `amSelect` (select), `amMax` (max), `amMin` (second max) |
| Bid list operations | `bdAt`, `bdTop`, `bdArg`, `bdSecond` |
| The rule | `priceOf`, `outcome` |
| Amount laws | `leRefl`, `maxLe`, `maxIdem`, `maxComm`, `maxAssoc`, `maxSwap`, `minMax`, `selMap`, `selRefl`, `selMax`, `lubLe`, `leMaxR`, `leOfMax` |
| List laws | `leTop`, `bdArgTop`, `floorEq`, `secondTop` |
| A1, A2, A4 | `a1Bid`, `a1Price`, `a1Sale`, `a2Second`, `a2Floor`, `a2Top`, `a4` |

Two check codes keep a program inside the rule. This file is exempt from
both.

- `REFUSE_AMOUNT`: a program reads an `Amount` by an elim, a fold or a
  projection, or applies `amSucc` to a term that is not a literal. A
  program uses only the four Amount operations, so a target can lower
  `Amount` to a fixed-width word.
- `REFUSE_BOUND`: a closed `Bids` value in a def type or a def body has
  more than 1024 bids (`BID_LIST_BOUND` in `src/front/check.c`). An open
  input list passes; the T1 entry bounds it. A closed `Members` value
  with more than 1024 members has the same code (`COMMITTEE_BOUND`, S5).

## The committee

S5 adds the committee words (`docs/PLAN.md` section 1, Q-S5-1 (a) to
Q-S5-11 (a)).

| Kind | Names |
|---|---|
| Families | `Hash`, `Party`, `Members`, `Committee`, `Params`, `Certificate` |
| Constructors and fields | `hash`, `hsTag`, `party`, `ptKey`, `mbNil`, `mbCons`, `mbHead`, `mbTail`, `committee`, `cmMembers`, `cmT`, `cmF`, `cmSafe`, `cmCensor`, `params`, `prRule`, `prReserve`, `prCommittee`, `prAmendT`, `prFork`, `ctNil`, `ctCons`, `ctSigner`, `ctR`, `ctS`, `ctRest` |
| Size and Sum words | `szLe`, `szLt`, `szEq`, `szAdd`, `sAnd`, `sOr`, `sNot` (the result `Sum Unit Unit`: `inr unit` is yes) |
| Party, hash and member words | `ptEq`, `hsEq`, `hsOf`, `mbLen`, `mbHas`, `mbDistinct` |
| The laws | `cmG1` (G1: no repeated party and f < t), `cmG2` (G2: t + f <= n), `prG3` (G3: n + f < ta + ta) |
| The certificate | `ctHas`, `ctCount`, `checkCert` |
| The rule on `Params` | `outcomeOf` |

G1 to G3 are proof fields: the type of `cmSafe` and `cmCensor` in
`Committee`, and of `prFork` in `Params`, is a `Path` to `inr unit` in the
law word. The kernel checks them as types, so each law holds for open
numbers too. A field that does not check gives the law code (Q-S5-9 (a)).
Each code is rewritten at `ctor_app` in `src/front/check.c`. This file is
exempt from all of them.

- `REFUSE_SAFETY`: a `cmSafe` field does not check (t <= f, or a repeated
  party).
- `REFUSE_CENSOR`: a `cmCensor` field does not check (n - f < t).
- `REFUSE_FORK`: a `prFork` field does not check (2ta - n <= f).
- `REFUSE_BOUND` (committee): a closed `Members` value with more than
  1024 members. An open committee passes; the T1 entry bounds it.

`checkCert c h e` counts each member of the committee one time when the
certificate e has an entry by that member with a valid Schnorr signature
on the message h (`ctHas`, S8a), and accepts when the count is at least
t. Two entries by one member count one time.

## The sealed bid

S6 adds the sealed-bid words (`docs/PLAN.md` section 1, Q-S6-1 (b) to
Q-S6-9 (a)).

| Kind | Names |
|---|---|
| Families | `Slot`, `Height`, `AucID`, `Root`, `Handle`, `Commitment`, `Deposit`, `EligibilityProof`, `Sealed`, `SealedBid`, `SealedBids`, `Handles`, `Reason`, `Verdict`, `Opening` |
| Deadline fields of `Params` | `prSlot`, `prStampEnd`, `prRelease`, and the proof field `prOrder` |
| Height words | `htLe`, `htLt` |
| Sealed words | `seal`, `sealOf` (the sealed family), `release` |
| Bid words | `admit`, `slash`, `elgVrf`, `vdCheck`, `hlHas` |
| Message words | `stampMsg`, `openMsg` (`szPair` and `szTri` are out at S8a) |
| Linear Size words | `szIsZ`, `szDec` (S5 `szEq` reads y with them) |
| The laws | `p3Slash` (P3), `szEqRefl`, and the proof field `opOk` of `Opening` (P2) |

`Sealed` is a sealed family: `seal` builds a value, and `sealOf` reads it.
A guest program def that elims, folds or projects a sealed family gives
`REFUSE_PEEK`. Only the domain word `release` reads one. This file is
exempt. The check records reads while elaborating the written type and
body of the def. Quoting types and generating proofs can unfold a domain
def, so those normal forms do not count as guest reads. A `refl` proof
about an authorized `release` call is allowed.

`release` takes an `Opening` and a `SealedBids` list and gives `Bids`. An
`Opening` holds the auction, the `Params`, a certificate and the proof
field `opOk`. The type of `opOk` says that `checkCert` accepts the
certificate on `openMsg` at `prRelease`. A field that does not check gives
`REFUSE_PEEK` (P2).

`admit` checks a sealed bid against the `Params`, the slashed set
`Handles`, the root, the height and the certificate. It gives a `Verdict`,
which is `valid` or `invalid` with a `Reason`. `slash` gives `valid` when
both `elgVrf` checks say yes and the two commitments differ. For equal
commitments it gives `invalid notAccepted`, and `p3Slash` proves it (P3).

`hsCanonical` checks whether a hash tag is below r. `admit` gives
`invalid badCert` when the commitment tag is at least r.
This keeps a stamp certificate from accepting another tag congruent mod r.

`stampMsg` and `openMsg` give `hash (hsMsg t x y)` with the kind tag t
(0 for a stamp, 1 for an open), so the two messages differ (S8a).
`szIsZ` and `szDec` are linear words: they do not call `ih` in a step.
The evaluator computes `ih` eagerly, so a step that calls it grows very
fast. `szEq` reads y with them. `szLe` still calls `ih` and stays for small
values.

`prOrder` is a proof field after `prFork`. Its type says that
`prStampEnd` is before `prRelease` (`htLt`). A field that does not check
gives `REFUSE_DEADLINE`. A closed `SealedBids` or `Handles` list with more
than 1024 entries gives `REFUSE_BOUND`.

## The log

S7 adds the log words (`docs/PLAN.md` section 1, Q-S7-1 (a) to
Q-S7-10 (a)).

| Kind | Names |
|---|---|
| Families | `Ballot`, `Ballots`, `Amendment`, `Entry`, `Log`, `Verdicts` |
| Entries | `enAmend` (with the proof field `eaOk`), `enSlash`, `enAdmit` |
| Log words | `lgNil`, `lgCons`, `append`, `slashedOf` |
| Amend words | `blHas`, `blCount`, `authorize`, `amendMsg`, `amendCheck` |
| Machine words | `paramsAt`, `paramsAtSize`, `Machine`, `machineOf` |
| Replay words | `entryVerdict`, `entrySlashed`, `replay` |
| The laws | `appendNilL`, `appendNilR`, `appendAssoc` (G4), `p3Replay` (P3) |

A `Ballot` holds a voter, a topic and a yes or no. `authorize` is yes when
at least ta distinct committee members voted yes on the topic. An
`Amendment` holds a tag, the new `Params` and the ballots. `amendCheck`
gives `invalid notAccepted` when `authorize` is no, `invalid badCert` when
`checkCert` rejects the certificate on `amendMsg`, and `valid` otherwise.
The word is `amendCheck`, not `amend`: the DAO prelude has a word `amend`,
and the same name gave `REFUSE_NAME` (rule R4).

`Entry` and `Log` are indexed by two `Params`: the Params before and the
Params after. `enAmend` moves the Params. `enSlash` and `enAdmit` keep
them. The proof field `eaOk` says that the EARLIER Params accept the
amendment. So an entry reads only earlier Params (G5), and an `eaOk` that
does not check gives `REFUSE_FIXPOINT`. `lgNil` is a unit of `append`, and
`append` is associative (G4): the three laws are path defs by induction.

`paramsAt` gives the Params at a height of the log. Height 0 gives the
first Params, and a height past the end gives the last Params. Time is the
log height, so `deadline`, `timeout` and `expires` join the time words
that give `REFUSE_CLOCK`. `Machine` is a `Stream Params`, built by
`machineOf`.

`replay` gives one `Verdict` for each entry. It carries the slashed
handles from left to right. A valid slash entry adds its handle. An
`enAdmit` entry is judged by `admit` with the handles so far. An amend
entry gives `valid`. `p3Replay` proves that a one-entry log with a slash on
two equal commitments gives `invalid notAccepted`.

`szLe` is now linear (it reads y with `szIsZ` and `szDec`). A closed
`Ballots` list with more than 1024 ballots gives `REFUSE_BOUND`. The `Log`
has no bound.

`examples/log.lang` shows the words with 11 eval rows. A guest program
cannot apply `amSucc` to a value that is not a literal (`REFUSE_AMOUNT`),
so `setup2` and `setupOI` use literal Amounts. The row `setupOI` states the
liveness item OI-24: with ta above n - f, `authorize` on all n - f honest
yes ballots gives `inl unit` (no).

## The field

S8 adds the machine `Word` and the BN254 field (`docs/PLAN.md` section 1,
Q-S8-1 (a) to Q-S8-9 (a)).

`Word` is a uint32 value that wraps. The ops are `wAdd`, `wSub`, `wMul`,
`wMulHi` (the high word of the 64-bit product), `wAnd`, `wOr`, `wXor`,
`wNot`, `wShl`, `wShr`, `wRotl`, `wRotr`, `wLt` and `wEq`. A compare gives
`Sum Unit Unit`. The literals are `0x1fw` and `31w`; a value above
0xffffffff gives `LEX_WORD_RANGE`. A guest program that names the type, an
op or a literal gives `REFUSE_WORD`. Only this file and `langc --kit` may.
`Word` lowers to masked Wasm i64 ops. The old bag family `Word` is now `SwapWord`;
its constructors and fields keep their names.

| Kind | Names |
|---|---|
| Family | `Fe` (8 Words, least significant first), `FeMod` (`modM`, `modN0`, `modR2`, `modR1`, `modE`) |
| Moduli | `frMod`, `fpMod` |
| Field ops | `feAdd`, `feSub`, `feMul`, `feToMont`, `feFromMont`, `fePow`, `feInv` |
| Types | `Fr`, `Fp` (each is `Fe`) |

An `Fe` holds a value in Montgomery form. `feMul` is the CIOS multiply with
8 rounds, unrolled. A value is canonical (below the modulus) at each
boundary: the ops take and give canonical values. `feInv` is a power by m
minus 2. The constants come from quorum-lang `bn254.h`.

The C oracle is `build/cryptotool`. `src/crypto` is a byte copy of
quorum-lang 8d1a113 (`u256`, `bn254`, `keccak`, `poseidon`). The tool
prints 581 rows (424 at S8, 501 at S8a, 537 at S8b) `FILE NAME ARGS => RESULT` from a fixed
seed and known answers. `examples/crypto.lang` has the 360 Word rows (it lowers).
`test/crypto/crypto.lang` has the 64 field rows (add 9, sub 9, mul 8, inv
6 for each of `Fr` and `Fp`). `test/gate.sh` evaluates each row with `langc
--kit eval` and prints `crypto oracle: 581 rows, 0 mismatches`.

The field rows are outside the Wasm set. The lowering refuses `frInv` with
`EVAL_DEPTH`, because it nests deeper than 2000 levels. The evaluator
handles it: `feMul` costs 9182 eval steps, and the `frInv` row costs
3713353 steps and 303.4 MiB. The limits are `EVAL_FUEL_STEPS` 40M (20M before S8b) and the
1 GiB arena. A def that calls `feInv` on a concrete modulus costs about
3.3M steps and 300 MiB at check time, so the row helpers take the modulus
as a binder.

`double` and `float` are approximate words now (`REFUSE_APPROX`), so no def
or binder can take them.

## The crypto prelude part 1

S8a adds the first crypto layer (`docs/PLAN.md` section 1, Q-S8a-1 (a) to
Q-S8a-10 (a)). A guest program can name its defs. It cannot name the
`Word` type, the `Word` ops, the `Word` literals or the kit builtins
(`REFUSE_WORD`). The guest examples use the fixtures and the committee
words.

| Kind | Names |
|---|---|
| keccak-256 | `Lane`, `lnXor`, `lnChi`, `lnRot`, `lnSwap`, `KState`, `KRcs`, `keccakF`, `kcRcs`, `kcHash160`, `kcChallenge` |
| Poseidon | `PsState`, `PsMds`, `PsRcs`, `poseidonPerm`, `poseidon2`, `psMdsFr`, `psRcsFr` |
| The G1 curve | `Pt1`, `Jac`, `ptDbl`, `ptMadd`, `ptToAffine`, `ptMul`, `g1Gen` |
| Schnorr | `scOnCurve`, `scValid`, `scPtEq`, `scEquation`, `schnorrCheck` |
| Messages | `szToFe`, `hsMsg`, `hsOf` |
| Fixtures | `fxKey1` to `fxKey5`, `fx<MSG>K<k>R`, `fx<MSG>K<k>S` |
| Kit builtins | `feMulC`, `ptMulC` |

`feMulC md a b` is the C Montgomery multiply, and `ptMulC md k p` is the
C scalar multiply k p on G1 (Q-S8a-6 (a), Q-S8a-7 (a)). The guest `feMul`
and `ptMul` stay the spec: oracle rows compare each builtin with its spec,
and the Wasm lowering runs the spec. A guest program that names a builtin
gets `REFUSE_WORD`.

A keccak lane is two Words (lo, hi), little-endian. `keccakF` runs its
rounds by an elim over a `KRcs` list, so the check does not unroll them.
`poseidon2` is circomlib width 3 (2 inputs) on Fr, alpha 5, 8 full and 57
partial rounds. Its 195 round constants (`psRcsFr`) and 9 MDS values
(`psMdsFr`) are data in Montgomery form. `build/cryptotool poseidon`
prints the same values (Q-S8a-2 (a)).

`schnorrCheck rcs P R s m` is the ratify-lang scheme. The challenge e is
keccak-256 reduced mod r (`kcChallenge`), and the check is s G = R + e P
on canonical points. `ptMul` uses Jacobian points; `schnorrCheck` uses
`ptMulC`.

`szToFe n` gives the Fe of a `Size` n. `hsMsg t x y` is poseidon2(t,
poseidon2(x, y)) on Fr. `hsOf n` is the `Hash` of the canonical Fe of n,
so a test can write `hsOf 2` for a message.

The committee words change at S8a (OI-25, OI-30). `Hash` is `hash (hsTag
: Fe)` (a canonical Fr value), and `hsEq` is `feEq` on the tags. `Party`
is `party (ptKey : Pt1)`, and `ptEq` is `scPtEq` on the keys. A
certificate entry is `ctCons (ctSigner : Party) (ctR : Pt1) (ctS : Fe)
(ctRest : Certificate)`. `ctHas p h c` runs `schnorrCheck` only on the
entries whose signer is p. `stampMsg`, `openMsg` and `amendMsg` give
`hash (hsMsg t x y)` with the kind tag t (0, 1 or 2). `szPair` and
`szTri` are out.

The fixtures (Q-S8a-3 (a)): `fxKey1` to `fxKey5` are the public keys of
fixed secret keys. Each signed entry has the names `fx<MSG>K<k>R` and
`fx<MSG>K<k>S` for the message MSG and the key k. The messages are Hs2
(`hsOf 2`), Hs3, StA, StL, OpA, OpE and AmA. `build/cryptotool fixtures`
prints the same 49 def lines. The keys are for tests only.

`hsMsg` and `schnorrCheck` are sealed defs (Q-S8a-9 (a)): they unfold
only on closed arguments. A memo keeps each closed value (Q-S8a-10 (a)).
`docs/PLAN.md` section 8.1 gives the kernel rules.

Costs in eval steps (measured early in S8a, before the switch of `Hash`,
`Party` and the certificate): `feMulC` 68 (the guest `feMul` about
8926), one `poseidon2` about 476000, one `schnorrCheck` about 828000.

The oracle has 501 rows at S8a: `test/crypto/crypto.lang` adds the
keccak, Poseidon, curve, `ptMulC`, Schnorr, `hsMsg` and fixture rows.

## The crypto prelude part 2

S8b adds the pairing and the Groth16 verifier (`docs/PLAN.md` section 1,
Q-S8b-1 (a) to Q-S8b-20 (a)).

| Kind | Names |
|---|---|
| The pairing spec | `mdGate2`, the Fp2 words (`f2Add`, `f2Mul`, `f2Inv`), the Fp12 words (`f12Mul`, `f12Frob`, `f12PowFrom`), `Pt2`, `g2Gen`, `g2Zero`, `g2MulBits`, `millerBits`, `pairCheck` |
| Groth16 | `G16Ics`, `G16Pubs`, `G16Vk`, `p1Canon`, `f2Canon`, `p2Canon`, `g16Msm`, `g16Verify` |
| Keys | `eligVk`, `outcomeVk`, `shareVk` |
| Proofs | `EligibilityProof` (`elgProof`), `OutcomeProof` (`outcomeProof`), `ShareProof` (`shareProof`) |
| Verifiers | `elgVrf`, `outcomeVrf`, `shareVrf` |
| Golden data | `eligPrfA` to `eligPrfC`, `eligPub0` to `eligPub2`, `elig2PrfA` to `elig2PrfC`, `elig2Pub0` to `elig2Pub2` |
| Kit builtin | `pairCheckC` |

`pairCheckC` is the C pairing check on BN254 (Q-S8b-1 (a)): it gives yes
when the product of the pairings of its points is 1. First it checks that
each point is on its curve and that each G2 point is in the subgroup of
order r (Q-S8b-8 (a)). The guest words are the spec: the tower Fp2, Fp6
and Fp12, the Miller loop (`millerBits`), the final exponentiation and
`pairCheck`. `test/crypto/pair-spec.lang` has 16 rows that compare the
spec with the C oracle: 8 rows on small cases, 4 value rows (a `Word`
digest of an Fp12 product, a Frobenius map, a Miller double step and a
Miller add step) and 4 chain rows (a G2 multiply from zero, a G2 multiply,
an Fp12 power and a Miller loop, each on a short bit chain). A full guest
pairing does not fit the 1 GiB arena, so no row runs one (`docs/PLAN.md`
OI-38). The `build/cryptotool` self-check has the EIP-197 known answers
and two Fp12 bilinearity rows for the C pairing.

The checker evaluates each argument of an application at check time.
Thus each Fp2 word takes its modulus through `mdGate2 md (fun (gu : Unit)
=> ...)`: the body stays stuck while `md` is open (Q-S8b-16 (b)). The
check of the spec defs went from 7.3M steps to 184k steps. Use the same
gate for a new heavy word that takes a modulus.

`g16Verify vk pubs a b c` is the Groth16 check on BN254 (Q-S8b-2 (a)). It
checks that each coordinate is canonical (below p). It computes vk_x, the
sum of IC0 and each public times its IC point, with `ptMulC` (`g16Msm`): a
public that is not below r, or a wrong count of publics, gives no. Then it
calls `pairCheckC` on the 4 pairs (-A, B), (alpha, beta), (vk_x, gamma)
and (C, delta). The key and proof values are raw words, and `g16Verify`
converts them to Montgomery form. `g16Verify` is the third sealed def.
`pairCheck` has no seal, because no guest def gives it open points.

`elgVrf r h c p` runs `g16Verify eligVk` on the publics (root, handle,
commitment) of the eligibility circuit (Q-S8b-3 (b), Q-S8b-13 (c)).
`EligibilityProof` is `elgProof (epA : Pt1) (epB : Pt2) (epC : Pt1)`.
`outcomeVrf a h p pf` uses `outcomeVk` on (auction, winner handle, price),
and `shareVrf a i c pf` uses `shareVk` on (auction, share index, share
commitment) (Q-S8b-4 (b), Q-S8b-12 (a)). `outcomeVrf` reads the `Amount`
price by an elim and gives only a yes or no bit: it is the fifth `Amount`
reader after `amLe`, `amSelect`, `amMax` and `amMin`. No word gives an
`Amount` as an `Fe` (Q-S8b-18 (a)).

The golden files are in `test/noir/` (Q-S8b-6 (a)): the dirs `elig`,
`elig2`, `outcome` and `share`, 7 files each. The eligibility circuit is
Alg 2 of the paper (Q-S8b-7 (b)): a Merkle path of depth 8 with the hash
hsMsg(tag, x, y), and the tags 3 leaf, 4 node, 5 handle, 6 commitment and
7 secret (Q-S8b-11 (a), Q-S8b-14 (a)). AucID is a private witness
(OI-37). `elig2` is a second proof for the same deposit and handle with a
second bid, so a second commitment (Q-S8b-17 (a)). The outcome and share
circuits are stubs: they bind only their 3 publics. The proofs come from
Noir with a Groth16 back end, and snarkjs, with the Hermez powers of tau
file of power 14 and one phase-2 contribution (Q-S8b-5 (a), Q-S8b-10
(a)). These tools made the files once. The gate does not use them, and
the generation script is not in the repo. The keys are for tests only
(OI-39).

`build/cryptotool noir NAME DIR` checks one golden proof in C and prints
its 14 def lines. The gate runs it on the 4 dirs (`noir: 4 golden proofs
passed`, Q-S8b-20 (a)). The 18 rows of `test/crypto/g16.lang` check that
the guest verifier gives the same result. The refusal rows are a changed
public, a public plus r, a short list of publics, swapped proof points, a
proof of a different circuit, the commitment of the other proof, a wrong
price, a wrong index and a coordinate that is not canonical.

`examples/sealed.lang` and `examples/log.lang` use the golden statements
(Q-S8b-17 (a)): the root, the handle and the two commitments are the
publics of `elig` and `elig2`, so the slash example has two valid proofs
for one handle. The StA and StL fixtures sign the stamp message of the
golden commitment (`eligPub2`). With these statements, `replayAll` in
`examples/log.lang` needs more than 20M steps, thus the fuel is 40M for
each declaration (Q-S8b-15 (a), Q-S8b-19 (a)). One closed `g16Verify` row
takes 0.2 to 0.6 s of CPU time.

The oracle has 537 rows at S8b.

## The crypto prelude part 3

S8c adds ChaCha20, the seed input and the MPC output check (Q-S8c-2 to
Q-S8c-5). The checker, the lowering and the Wasm entry do not change.

ChaCha20 is RFC 8439 as guest defs over `Word`; there is no C builtin.
`CcSt` is the state of 16 Words (`ccSt`, the readers `cc0` to `cc15`).
`ccQr` is the quarter round and `ccDbl` is the double round. `ccRounds`
runs `ccDbl` one time for each step of a `CcRn` value (`ccRn10` has 10
steps), `ccAdd` adds two states and `ccBlock` is the block function.
`Seed` is `seed8` of 8 Words (`sd0` to `sd7`). `ccBlockOf sd ctr n0 n1
n2` is the block of RFC 8439 section 2.3 with the seed as the key.
`sdExpand sd tag ix` is `ccBlockOf sd ix tag 0w 0w`: the counter is the
block index and the nonce is (stream tag, 0, 0) (Q-S8c-2). The output is
the 16 Words. There is no field sampler (OI-40), and no host entry gives
the OS seed before S14b and S14c (OI-41).

The gate rule. `ccGate w k` is an elim on `wEq w 0w` with the same branch
two times, and the block is behind it with the first seed word as `w`.
While that word is open, the block is stuck and the check of a def that
uses it is cheap. The gate is on the first seed word only: a closed seed
with an open counter opens the gate (about 44000 check steps). `wEq w w`
is not a gate, because `wEq` of one open Word and the same Word gives
`inr`.

`test/crypto/chacha.lang` has the 34 ChaCha20 rows: `ccQA` to `ccQD` (the
4 words of one quarter round), `ccB0`, `ccB15` and `ccBX` (word 0, word
15 and the xor of the 16 words of one block) and `sdX` (the xor of the 16
words of `sdExpand`). The C oracle in `test/cryptotool.c` is written from
the RFC text. Its known answers are the vectors of the sections 2.1.1,
2.3.2 and 2.4.2, and a wrong known answer stops the tool.

`outcomeAtt c a h p cs` is the MPC output check (Q-S8c-4). The winner
handle must be canonical: a handle at or above the Fr modulus is
rejected, so modular hashing cannot alias it to a signed handle.
The outcome message is hsMsg (3, auction tag, hsMsg (4, winner handle,
price)); 3 and
4 are new tags. The result is `inr unit` only when `cs` has an accepted
signature of each member of `c` on the hash of that message: the count
`ctCount` must reach the number of members, not the threshold `cmT`. A
signer with two entries counts one time. `outcomeAtt` reads the `Amount`
price by an elim and gives only a yes or no bit: it is the sixth `Amount`
reader. The message is inline, so no word gives an `Amount` as an `Fe`.
There is no message for `noSale` (OI-44). design/DESIGN.md says "t
attestations"; the ruling makes it n (OI-42).

The fixture OuA is the outcome message of the auction 1, the winner
handle `hsOf 2` and the price 2, with a signature of each of the keys 1
to 5 (`fxOuAK1R` to `fxOuAK5S`). `test/crypto/att.lang` has the 10 rows:
`ckOuAK1` to `ckOuAK5` check each signature on the guest message
`msgOuA`; `attGood` (5 signatures) gives `inr unit`; `attFour` (4
signatures), `attPrice` (the price 3) and `attTwice` (the key 4 two
times, no key 5) give `inl unit`.
`attNonCanon` reuses the five signatures with handle r + 2 instead of
2, where r is the Fr modulus, and gives `inl unit`. The rows have their
own file because
each row of a kit file checks the full file, and the check of `msgOuA`
calculates its two hashes (OI-45).

Mutants at S8c. M1 (`ccRn10` with 9 steps), M2 (the last rotate of
`ccQr`, 7 to 8), M3 (`sdExpand` with the tag and the index in the other
order) and M5 (the same rotate in the C oracle) are KILLED. No row sees
M4 (`ccGate` with `wEq w w`): each value is the same and only the check
cost changes (OI-43). A1 (`mbLen (cmMembers c)` to `cmT c`), A2 (the
price out of the message) and A3 (the tag 3 to 1) are KILLED by
`attFour`, `attPrice` and `attGood`.

The oracle has 581 rows at S8c.

## The IR test families

S3 adds `Tree`, `Signal`, `Window`, `Hop` and `Echo` from permit-lang
18deea2 for the IR rows. `Size` is in the prelude.

| Family | Kind | Use |
|---|---|---|
| `Tree` | two recursive fields, `forkLeft` and `forkRight` | the tree refusal `test/ir/tree.lang` |
| `Size` | one recursive field, `szPred` | `sizeSum` and `sizeTrap`; `test/ir/spine.lang` (the heap refusal and the spine evals) |
| `Signal`, `Window` | a block with no recursive field | the block case split `test/ir/block.lang` |
| `Hop`, `Echo` | a block; each has one recursive field of the other family | the block refusal `test/ir/hop.lang` |

`family Tree` exists only for the tree refusal. The gate row checks that
the lowering refuses a fold over a run-time tree with
`REFUSE_RUNTIME_TREE`. Rule R1 lets only this file declare a family, so
the family must be here. The blocks `Signal` / `Window` and `Hop` /
`Echo` exist only for the block rows.

## How to replace the domain

1. Write your families and definitions in `domain/domain.lang`. Keep the
   names apart from the core names in `README.md`.
2. Change the examples and the tests that use the prelude names and the
   IR test families: the kernel rows in `examples/`, `test/check/` and
   `test/eval/`, their lines in the `expect.txt` files, and
   `test/ir/spine.lang`, `test/ir/tree.lang`, `test/ir/block.lang` and
   `test/ir/hop.lang`.
3. Keep a family with two recursive fields for `test/ir/tree.lang`, or
   remove that file and its block in `test/gate.sh`.
4. Run `make check` in the kit directory. The build makes `build/domain.c`
   again when `domain/domain.lang` changes.
