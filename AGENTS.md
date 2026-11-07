# AGENTS.md (DRAFT)

This file is for coding agents that work in this repo or meet gavel-lang
code. It is a DRAFT at S0. Slice S20 makes it final (`docs/PLAN.md`
section 12).

## State at S6

- S6 adds the sealed bid to `domain/domain.lang`: `Slot`, `Height`, the
  deadline fields of `Params` (`prSlot`, `prStampEnd`, `prRelease` and the
  proof field `prOrder`), `AucID`, `Root`, `Handle`, `Commitment`,
  `Deposit`, `EligibilityProof`, `Sealed`, `SealedBid`, `Reason`,
  `Verdict`, `elgVrf`, `admit`, `slash`, `Opening` and `release`.
- P2 is the proof field `opOk` of `Opening`. P3 is the path law `p3Slash`.
  A guest def that elims, folds or projects a `Sealed` value gives
  `REFUSE_PEEK`, and so does an `opOk` field that does not check. A
  `prOrder` field that does not check gives `REFUSE_DEADLINE`. A closed
  `SealedBids` or `Handles` list has at most 1024 entries
  (`REFUSE_BOUND`).
- `elgVrf` is an ideal check until S8b. `Handles` is an input of `admit`
  until S7. The `Log` and `replay` are S7.
- The rest of the state is as at S5, below.

## State at S5

- S5 adds the committee to `domain/domain.lang`: `Hash`, `Party`,
  `Members`, `Committee`, `Certificate`, `Params`, `checkCert` and
  `outcomeOf`. A `Committee` holds the members, t and f. `Params` holds
  the rule, the reserve, the committee and the amend threshold ta.
- G1 to G3 are `Path` proof fields (`cmSafe`, `cmCensor`, `prFork`). A
  field that does not check gives `REFUSE_SAFETY` (G1: no repeated party
  and f < t), `REFUSE_CENSOR` (G2: t + f <= n) or `REFUSE_FORK` (G3:
  n + f < ta + ta). A closed committee has at most 1024 members
  (`REFUSE_BOUND`).
- `Party` is an abstract key and `checkCert` matches an entry on its
  signer and message, until S8 (the Schnorr check).
- The rest of the state is as at S4, below.

## State at S4

- The code is the Cubical TT kernel of ratify-lang a46d5df (S2) and R0
  from ratify-lang 71f2c22 (S3): `langc` (C99, TinyCC) and its domain in
  `domain/`. The Wasm writer `src/wasm.c` emits no trap form, and
  `test/notraptool.c` checks each built module.
- S4 adds the auction rule to `domain/domain.lang`: `Amount`, `Bids`,
  `Rule`, `Outcome`, `outcome` and the laws A1, A2 and A4. A program uses
  an `Amount` only through `amLe`, `amSelect`, `amMax` and `amMin`
  (`REFUSE_AMOUNT`), and a closed bid list has at most 1024 bids
  (`REFUSE_BOUND`). The kernel signature slot is `DaoOutcome`, not
  `Outcome`.
- The governance, sealed-bid and crypto layers, the targets and `gaveld`
  are a plan only: `docs/PLAN.md` sections 4 to 8 and section 12.

## What gavel-lang is for

Off-chain sealed-bid auctions (first price or second price, a reserve, a
fixed tie-break) under an off-chain t-of-n threshold authority that amends
itself. The checker proves the rule facts before any bid. No program can
have a runtime error: a rejection is a `Verdict` value.

Use sealed-lang for on-chain auctions. Use ratify-lang for a threshold
authority with no auction.

## Build and test

- `make -C <repo> check` runs the gate. Run it serially.
- Tests are in C (host) and in gavel-lang (guest). Do not add a node,
  Python or shell harness.

## Programs

Four C programs, all built by TinyCC (`docs/PLAN.md` section 8.3):

- `langc`: the compiler.
- `langc mcp`: the authoring MCP server over stdio (planned, S17).
- `langc serve`: the same tools over HTTP behind x402 (planned, S18).
- `gaveld`: the T1 host that runs a compiled module (planned, S10).

## Rules for changes

- No em-dashes or en dashes in any file. The gate refuses them.
- `design/DESIGN.md` section 9 and `design/DIGEST-2606.14939.md` are
  verbatim. Do not edit them.
- Do not add a core type or a core operation that `design/DESIGN.md` does
  not give.
- Do not put a wallet address, a facilitator URL or a key in the repo.
  These are USER inputs.
