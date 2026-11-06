# gavel-lang

gavel-lang is a domain-specific language for off-chain sealed-bid auctions
under an off-chain threshold authority that amends itself. It comes from
lang-template with the tcc-wasm host. The reference is arXiv:2606.14939
(see `design/DIGEST-2606.14939.md`). No program can have a runtime error.

Status: S4 (the auction rule): the Cubical TT kernel of ratify-lang
a46d5df, R0 from ratify-lang 71f2c22 (total Nat operations,
`REFUSE_PARTIAL`, and a Wasm writer that emits no trap form), and the first
gavel-lang words in `domain/domain.lang`: `Amount`, `Bids`, `Rule`,
`Outcome`, `outcome`, the laws A1, A2 and A4, `REFUSE_AMOUNT` and
`REFUSE_BOUND`. The governance and crypto layers and the targets do not
exist yet.
`docs/PLAN.md` gives the readings, the route and the slices.

## When to reach for gavel-lang (DRAFT)

| Use gavel-lang when | Use something else when |
|---|---|
| You run a sealed-bid auction off chain, with a t-of-n committee that certifies deposits, timestamps bids and releases them together. | The auction runs on chain (EVM): use sealed-lang. |
| You need first price or second price, a reserve and a fixed tie-break, with no runtime error. | You need only a self-amending threshold authority, with no auction: use ratify-lang. |
| The winner and the price come from FHE (TFHE) or MPC (Bristol Fashion circuits), with a ZK eligibility proof for each bid. | You need approximate arithmetic (CKKS): gavel-lang refuses it. |
| The auction rules change only by a t-of-n authorized amendment, and you want the checker to prove the rule facts first. | You need a wall clock: gavel-lang has log heights only. |

## Files

- `docs/PLAN.md`: the plan, the rulings and the questions for the USER.
- `design/DESIGN.md`: the domain design. It is the only source of the core
  types and the core operations.
- `design/DIGEST-2606.14939.md`: the paper digest, verbatim.
- `AGENTS.md`: notes for coding agents (DRAFT).
- `SPEC.md`: the specification. Fill each section.
- `formers/FORMERS.md`: the type formers. `formers/tcc-wasm.md` gives their
  status on the tcc-wasm host.
- `probe/CAPABILITY.md`: the host probe.
- `docs/STATUS.md` and `docs/VALIDATION.md`: the status and the gate results.
- `docs/host/README.md`: how to build and test the host kit, and how to
  replace the sample domain in `domain/`.
