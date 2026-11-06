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
| Families | `S1`, `Trunc1`, `Trunc0`, `Size`, `Parity`, `Empty`, `Bag`, `FreeMon`, `Pos`, `Word`, `ChoiceBag`, `OrbitN`, `Charter` |
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
| Constructors and fields | `hash`, `hsTag`, `party`, `ptKey`, `mbNil`, `mbCons`, `mbHead`, `mbTail`, `committee`, `cmMembers`, `cmT`, `cmF`, `cmSafe`, `cmCensor`, `params`, `prRule`, `prReserve`, `prCommittee`, `prAmendT`, `prFork`, `ctNil`, `ctCons`, `ctSigner`, `ctMsg`, `ctRest` |
| Size and Sum words | `szLe`, `szLt`, `szEq`, `szAdd`, `sAnd`, `sOr`, `sNot` (the result `Sum Unit Unit`: `inr unit` is yes) |
| Party, hash and member words | `ptEq`, `hsEq`, `mbLen`, `mbHas`, `mbDistinct` |
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

`checkCert` counts each member of the committee one time when the
certificate has an entry with that signer and that message, and accepts
when the count is at least t. Equal entries count one time. At S8 the
entry match becomes the Schnorr check.

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
