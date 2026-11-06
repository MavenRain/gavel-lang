# AGENTS.md (DRAFT)

This file is for coding agents that work in this repo or meet gavel-lang
code. It is a DRAFT at S0. Slice S20 makes it final (`docs/PLAN.md`
section 12).

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
