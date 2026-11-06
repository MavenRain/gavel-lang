# gavel-lang plan

gavel-lang is a Cubical TT language for one job: off-chain sealed-bid
auctions under an off-chain threshold authority that amends itself. A
bidder commits to a bid under a one-time handle. A committee of n members
certifies the deposit root, timestamps each commitment and releases the
bids together after a certified deadline. FHE or MPC computes the winner
and the price. A t-of-n authorized amendment changes the auction rules,
and the old amend rule judges the new rules. The checker proves the rule
facts before any bid. A host replays the log and checks the certificates.
No program can have a runtime error.

The design is `design/DESIGN.md`: the role map, the base types, the
operations, the laws and the four C programs, and the request, verbatim.
The paper digest is `design/DIGEST-2606.14939.md`, verbatim. This file
gives the readings of the request, the route, the slices and the open
questions. "design sec N" is section N of `design/DESIGN.md`. Section 14
lists the questions for the USER. A reading that is not ruled stays a
recommendation.

Status: S0 (seed): the tcc-wasm kit and this plan. Next: S1 (the USER
rules section 14).

## 1. Rulings

| Date | Ruling | Source |
|---|---|---|
| 2026-11-05 | Name: gavel-lang. Repo MavenRain/gavel-lang, private at creation. The USER makes it public. | USER, S0 |
| 2026-11-05 | T1 (primary): a Wasm module and a TinyCC C host `gaveld`, from the lang-template `hosts/tcc-wasm` kit. The auction rule, the amend rule and the log replay compile to one Wasm module. The ZK, FHE and MPC crypto is behind a fixed import ABI. Differential tests under node and wasmtime. | USER, S0 |
| 2026-11-05 | Extra targets (later slices): T2 ZK relation export (the eligibility relation of Alg 2, statement (root_s, hu, cm), and the auction-outcome proof); T3 FHE compare circuit (winner and price over encrypted bids, TFHE, a static depth bound, CKKS refused); T4 MPC circuit (Bristol Fashion Boolean circuits for winner and price and for the amendment tally); T5 JSON report. | USER, S0 |
| 2026-11-05 | Not chosen: native C through TinyCC (a new host kit), a JS module (tcc-js) as the primary target. | USER, S0 |
| 2026-11-05 | x402 route: `langc serve`, an HTTP server in C that tcc builds. It links libcurl for the facilitator verify and settle calls and runs behind a TLS proxy. The repo ships `server.json`, `openapi.yaml`, `llms.txt` and `funding.json`. Not chosen: a Cloudflare Worker, files only. | USER, S0 |
| 2026-11-05 | Host: TinyCC. `langc`, `langc mcp`, `langc serve` and `gaveld` are C99 that TinyCC builds. | USER, S0 |
| 2026-11-05 | Each deploy, each registry submission, each AgentFund campaign and the payTo wallet are USER steps. The repo holds no wallet address that Claude made up. | USER, S0 |

The seed is `bin/new-lang.sh gavel-lang tcc-wasm` from lang-template
5265a2a. The kit gives the parser, the checker, the evaluator, the lowering
and the Wasm writer. No kit emits a C host, a ZK relation, an FHE circuit,
a Bristol circuit, a JSON report, an MCP server or an HTTP server, so
these are new code.

Carried default, not ruled for gavel-lang: proofs are guest-language terms
only (quorum-lang ruling, 2026-10-25). Lean export is not a target.

## 2. The reading of R0: no runtime error

R0: no program can trap or surface an exception, on any target.

The reading is the ratify-lang reading (ratify-lang 55310c5 docs/PLAN.md
lines 43 to 91), which is the permit-lang reading (permit-lang f9d8a15
docs/PLAN.md lines 147 to 212). Each operation is total on its declared
type. Each run of each target gives a value of the result type. A
rejection is a value, never an error:

- `Verdict = valid | invalid Reason` (Q-S0-2). The reasons: `badCert`,
  `badProof`, `late`, `early`, `slashed`, `notAccepted`, `wrongHeight`,
  `malformed`.
- A late bid, a failed proof check and a bad decryption share are values
  (`invalid late`, `invalid badProof`, `invalid badCert`).
- A host failure (a socket error, a missing file, bytes that do not parse)
  never reaches the module. `gaveld` maps it to `invalid malformed` first
  (relay-lang rule N6). `langc serve` maps a failed facilitator call to a
  402 or a 502 response, never a crash.
- `outcome` on an empty bid list, or with no bid at the reserve, gives
  `noSale`.
- Equal top bids give the lowest index (A4), so `outcome` is a function.

Amount arithmetic (Q-S0-1). (a) `Amount` admits only compare, select, max
and second max. That is enough for first price, second price, the reserve
and the tie-break, and no operation can overflow. (b) Bounded add and sub
with a static bound obligation.

Other sources of a trap and their reading:

- `paramsAt` past the end of the log: the height is `Fin (length + 1)` of
  a given `Log` (ratify-lang Q-S0-3 (a)).
- A committee or a bid list larger than the target bound gives
  `REFUSE_BOUND`.
- A partial kit word (`head`, `tail`, `pred`, `div`, `rem`) gives
  `REFUSE_PARTIAL` (permit-lang f9d8a15 docs/PLAN.md line 182).
- The Wasm writer emits no `unreachable`, puts a guard before each `div_u`
  and `rem_u`, and gives each `br_table` a default that runs the last arm.
  A C tool `test/notraptool.c` checks each module (port from ratify-lang).
- The kit gate row "a trap is equal to a trap" changes: R0 makes a trap a
  failure on each target (permit-lang f9d8a15 docs/PLAN.md lines 188 to
  201). Open item OI-4.

## 3. The reading of R1

R1: the type constructors are the cell monads and their duals, as
represented computationally in Cubical TT.

gavel-lang takes the ratify-lang reading, verbatim where the words are the
same (ratify-lang 55310c5 docs/PLAN.md lines 93 to 128, from permit-lang
and accrue-lang). The ambient structure is not limited: Pi, Sigma, `Path`,
`PathP`, the interval `I`, `transp`, `hcomp`, the universe `U`, `Glue` and
`ua`. A declared type constructor must be one of two classes:

- CM, cell monad: the free algebra monad of a cell signature. The parameter
  occurs only in the generator `eta`. Unit is `eta`, bind is the recursor,
  and the laws are paths by induction.
- CC, cocell comonad: the cofree coalgebra comonad of a cocell signature.
  One observation `eps`, each other field `D -> D` or `D -> E -> D`. Counit
  is `eps`, extend is the corecursor.

The formers that the design needs (Q-S0-3):

| Former | Class | Reading | Design |
|---|---|---|---|
| `Log` | CM (indexed) | The free category on accepted entries (amendments, admitted bids, slash entries, releases). `nil` is the unit, `cons e log` adds one cell, `append` is the bind. G4 is a path by induction. | sec 4, sec 5 |
| `Machine` | CC (dual of `Log`) | The cofree comonad of `Entry -> _`: a Moore machine. `eps` is the `Params` at the current height; `step` moves one height. `paramsAt h` is `eps` after h steps. | sec 5 |
| `List` | CM | The free monoid. The bid list of one auction. | sec 4 |
| `Sealed a`, `Share a` | CM, no cells | The identity monad at a base type: `seal` and `share` are the unit, and the denotation is the value. The parameter is a base type only (`Amount`, `Bool`). No projection to `a` exists outside `release` (P1). | sec 3, sec 4 |
| `Proof R x` | none (a base sort) | A proposition-indexed sort: the meaning is the claim that a witness exists. | sec 3 |
| `Protocol a b` | none (a base sort) | The meaning is the ideal functionality. Composition holds only up to indistinguishability, so it is ASSUMED (section 6). | sec 7 |

REFUSED formers: general recursion and fixpoints (`REFUSE_FIXPOINT`),
exceptions, effects, a clock (`REFUSE_CLOCK`). Section 7 lists the codes.

## 4. The minimal base signature (R2)

R2: admit only what off-chain ZK, FHE and MPC sealed-bid auctions and
their threshold governance need. Each name cites the design. Nothing
outside this list is core.

### 4.1 Types

The draft list is design sec 3 (Q-S0-5): `AucID`, `Slot`, `Height`,
`Hash`, `Root`, `Handle`, `Commitment`, `Deposit`, `EligibilityProof`,
`Amount`, `SealedBid`, `Party`, `Committee`, `Certificate`, `Verdict`,
`Reason`, `Outcome`, `Params`, `Amendment`, `Ballot`, `Log`.

The role map of design sec 2 is the core reading (Q-S0-4): the deposit
root, the timestamp and the release each become a t-of-n certificate, and
slashing becomes a certified log entry.

Hash (Q-S0-8). (a) An abstract `Hash` in the language (leaf-lang
precedent); Poseidon at T2 and in `gaveld`. (b) Poseidon fixed in the
language.

### 4.2 Operations

The draft list is design sec 4: `checkCert`, `elgVrf`, `admit`, `slash`,
`release`, `outcome`, `authorize`, `amend`, `paramsAt`, `replay`. Each one
is total and gives a `Verdict`, a `Bool` or an `Outcome`.

Governance (Q-S0-6). (a) One `Params` record (the auction rule, the
reserve, the committee, t, n, f, the deadlines). Only a t-of-n authorized
`Amendment` changes it. The amend rule can amend itself. Amendments order
by log height (ratify-lang 55310c5 docs/PLAN.md sections 5 and 8). (b) The
auction rule is fixed for each `AucID`; amendments change only the
committee.

The price list of `langc serve` (Q-S0-7). (a) Not in `Params`: it is a
deploy setting of the USER. (b) In `Params`, so the governance layer sets
the prices.

## 5. Time

There is no clock. Time is the log height. A deadline is a height that a
t-of-n certificate fixes. A bid is late when its timestamp certificate
names a height after the deadline. `release` needs the deadline
certificate (P2). A wall clock or a host tick gives `REFUSE_CLOCK`.

## 6. Laws and obligations

Each law is a guest-language term with a `Path` type, or a decision of the
checker on finite data. `langc check --json` reports the counts in
`progress` (Q-S0-27): `laws_discharged` and `laws_total`, `laws_assumed`,
`laws_at_target` and `laws_external`. An ASSUMED row is never counted as
discharged.

| Row | Statement | Kind | How |
|---|---|---|---|
| P1 | Hiding: no party learns a bid before the release | ASSUMED | scheme security (FHE, MPC, commitment) |
| P2 | Simultaneous release: shares count only after the deadline certificate | DISCHARGED | the type of `release` |
| P3 | No free bid withdrawal: two valid cm under one hu give a slash entry | DISCHARGED | `replay` by construction (Alg 1) |
| P4 | Handle uniqueness for each deposit and auction | ASSUMED | collision resistance (paper Lemma 14) |
| P5 | Completeness: an honest eligible bid is admitted | ASSUMED | proof-system completeness (paper Lemma 12) |
| Z1 | Knowledge soundness of pi^m | ASSUMED | proof system; the paper states no such assumption |
| Z2 | Threshold decryption matches the ideal | ASSUMED | noise flooding proof |
| Z3 | dec(eval g cs) = g(dec cs) at T1 | ASSUMED | scheme correctness |
| G1 | Safety: t > f | DISCHARGED | decided on `Params` |
| G2 | Censorship resistance: n - f >= t | DISCHARGED | decided on `Params` |
| G3 | Fork freedom of the amend rule: 2t - n > f | DISCHARGED | decided on `Params` (ratify-lang O5) |
| G4 | `nil` is the unit, `append` is associative | DISCHARGED | paths by induction (ratify-lang TCM-E) |
| G5 | Causality: an entry reads only earlier `Params` | DISCHARGED | by construction (ratify-lang O10) |
| G6 | At most one entry for each height | EXTERNAL | G3 and honest single signing (ratify-lang O11) |
| A1 | First price: the winner bid is at least each bid; the price is the winner bid | DISCHARGED | path by induction on the list |
| A2 | Second price: the price is the second highest bid, at most the winner bid | DISCHARGED | path by induction on the list |
| A3 | Under second price, a true bid is a dominant strategy | EXTERNAL | auction-lang theorem (name only) |
| A4 | Tie-break: equal top bids give the lowest index | DISCHARGED | definition of `outcome` |
| F1 | FHE exactness for `outcome` | AT TARGET (T3) | a static depth bound for each circuit |
| Z4 | The T2 relation is the guest relation | AT TARGET (T2) | golden files |
| M1 | MPC security and the corruption model | EXTERNAL | the protocol, not langc (T4 emits the circuit only) |

Count at S0: DISCHARGED 10 (P2, P3, G1, G2, G3, G4, G5, A1, A2, A4),
ASSUMED 6 (P1, P4, P5, Z1, Z2, Z3), AT TARGET 2 (F1, Z4), EXTERNAL 3 (G6,
A3, M1). Total 21.

## 7. Refusals

New check codes (proposals, Q-S0-28). Each one gets a `fix` hint at A2.

| Code | When | Source |
|---|---|---|
| `REFUSE_PARTIAL` | A partial kit word. | R0 |
| `REFUSE_BOUND` | A committee or a bid list larger than the target bound. | R0 |
| `REFUSE_AMOUNT` | An `Amount` operation other than compare, select, max, second max. | R0, Q-S0-1 |
| `REFUSE_FORMER` | A type constructor that is not CM or CC. | R1 |
| `REFUSE_FIXPOINT` | General recursion, or `Params` that authorize themselves. | R1 |
| `REFUSE_CLOCK` | A wall clock or a host tick. | section 5 |
| `REFUSE_PEEK` | A read of a `Sealed` value outside `release`. | P1 |
| `REFUSE_SAFETY` | t <= f. | G1 |
| `REFUSE_CENSOR` | n - f < t. | G2 |
| `REFUSE_FORK` | 2t - n <= f for the amend rule. | G3 |
| `REFUSE_APPROX` | An approximate scheme (CKKS). | T3 |

## 8. Route

### 8.1 Kernel

Port a Cubical TT kernel (Q-S0-14), then add R0 (section 2) and the
classes check of R1 (section 3). Survey first: SAME, CHANGED,
SOURCE-ONLY.

### 8.2 The five targets

- T1 (primary): one Wasm module for `admit`, `slash`, `release`,
  `outcome`, `authorize`, `amend`, `paramsAt` and `replay`. `gaveld` reads
  a log, the bids and the certificates, runs the module and prints the
  verdicts. The crypto is C in `gaveld` behind a fixed host-import ABI
  (Q-S0-13). The Wasm engine of `gaveld` is Q-S0-15. Differential rows
  under node and wasmtime.
- T2: the eligibility relation of Alg 2 (statement (root_s, hu, cm);
  witnesses rho_u, A_u, the Merkle path, tx) and the auction-outcome proof
  (Q-S0-9), in the format of Q-S0-10, with golden files (Z4).
- T3: the FHE compare circuit for winner and price over encrypted bids
  (Q-S0-11), with a static depth bound (F1). `REFUSE_APPROX` for CKKS.
- T4: Bristol Fashion Boolean circuits for winner and price and for the
  amendment tally. The MPC protocol is EXTERNAL (M1, Q-S0-12).
- T5: the JSON report: the auction facts, the governance facts (the G1 to
  G3 margins), the law counts and the verdict for each log height.

### 8.3 Four C programs

design sec 7. `langc`, `langc mcp`, `langc serve` and `gaveld` are
different programs. Do not merge them. `langc mcp` and `langc serve` give
the same tools: `check`, `fmt`, `explain`, `list-base`, `show-law`.

### 8.4 The x402 route

`langc serve` (Q-S0-16): JSON POST routes for the five tools and MCP
streamable HTTP at `/mcp`, each behind x402. A request with no payment
gets a 402 response whose body lists the price (Q-S0-17) and the network
(Q-S0-18). The server calls the facilitator verify and settle endpoints
with libcurl. The shape is the mandate-lang x402 route (mandate-lang
33556e9 docs/VALIDATION.md line 51, test/run/mandate-x402.script). The
facilitator URL and the payTo address are USER inputs; `langc serve` does
not start without them (OI-12). It writes a local count log (calls, 402
responses, settled payments); nothing leaves the host (Q-S0-22).

### 8.5 The demo

A committee of 4 members, t = 3, f = 1 (G1 margin 2, G2 margin 0, G3
margin 1). One second-price auction with a reserve and four bidders.
`gaveld` gives a verdict for each of D1 to D8: an admitted bid, a late bid,
a bad eligibility proof, a double bid that gives a slash entry, a release
before the deadline, the outcome, an amendment that changes the auction
rule to first price, and bytes that do not parse (`malformed`).

## 9. Agent adoption

The request asks for seven items (block 1) and gives four advice items
(block 2) so that agents reach for gavel-lang when it fits. Each item has
one deliverable and one slice. USER marks a step that only the USER can
do.

| Item | Pasted line | Deliverable | Slice |
|---|---|---|---|
| A1 | "one canonical form per operation and a langc fmt canonical printer" | `langc fmt`, `langc fmt --check`; no aliases. Gate: idempotent, same core term. | S15 |
| A2 | "langc check --json, giving fix hints and progress counts (parsed, typed, laws discharged)" | `diagnostics` (code, span, expected, found, `fix`) and `progress` (section 6 counts). `docs/CHECK-JSON.md`. | S16 |
| A3 | "a SKILL.md with 3 to 5 graded examples" | `skills/gavel-lang/SKILL.md`: G1 a second-price auction, G2 a reserve and a tie, G3 a slash by evidence, G4 an amendment of the auction rule, G5 the demo. | S20 |
| A4 | "an MCP server written in TinyCC (langc mcp)" | MCP over stdio (JSON-RPC 2.0), in C. Spec version pinned in `docs/MCP.md`. `test/mcptool.c`. | S17 |
| A5 | "AGENTS.md and a README table saying when to use the language" | `AGENTS.md` and the README table. DRAFT at S0, final at S20. | S0, S20 |
| A6 | "a token-count comparison against Python and Rust" | G1 to G5 and the demo in three languages, equal outputs, `docs/TOKEN-BASELINE.md` (Q-S0-26). `make tokens`. | S21 |
| A7 | "List where agents look. That means an MCP server entry in the public registries, an OpenAPI description, and the price in the 402 response." | `server.json` (Q-S0-19), `openapi.yaml` (Q-S0-20), the 402 body (section 8.4). Registry submission: USER. | S18, S19 |
| B1 | block 2 item 1 (agent-consumable and payable) | `langc serve` behind x402; the SKILL.md of A3. | S18, S20 |
| B2 | block 2 item 2 (discovery and funding signals) | `.github/FUNDING.yml`, `funding.json`, `llms.txt` (Q-S0-21); AGENTS.md. | S19 |
| B3 | block 2 item 3 (agent-native funding rails) | Candidate milestones in this plan only (Q-S0-23). Any AgentFund campaign: USER. | S22 |
| B4 | block 2 item 4 (frame the value in agent terms) | README section "What an agent gets" (Q-S0-24). | S20 |

USER steps: deploy `langc serve` and its TLS proxy; give the facilitator
URL, the network and the payTo address; publish `server.json` to the MCP
Registry; list the endpoint in x402 discovery surfaces; run any AgentFund
campaign; make the repo public.

## 10. Tests

The dev-language rule (ruling 10-09) holds (Q-S0-25): unit and integration
tests in C (host) and in gavel-lang (guest). No new node, Python or shell
harness. The kit's node differential stays.

- The kit gate rows, with the R0 change of the trap row (OI-4).
- One check row for each code of section 7, and one `fix` row at A2.
- Eval rows for each operation of section 4.2 on closed data.
- Law rows for G1 to G3 on the demo committee, A1, A2, A4 on bid lists.
- `test/notraptool.c` on each module; differential rows under node and
  wasmtime (T1).
- Golden files for T2 to T5.
- x402: a C client against a local `langc serve` and a stub facilitator in
  C.

## 11. Port sources

| Source | What | Where |
|---|---|---|
| lang-template 5265a2a | the tcc-wasm kit | `hosts/tcc-wasm` |
| ratify-lang 796f442 | R0, R1, the governance layer, the kernel, `test/notraptool.c`, the Wasm engine, the host-import ABI (Q-S1-1 (a)) | docs/PLAN.md; seed 55310c5 docs/PLAN.md lines 43 to 128 |
| sealed-lang 30d4d5f | the eligibility relation, hu, cm, `slashUsr` on chain | SPEC.md |
| quorum-lang 8d1a113 | `src/crypto` (bn254, elgamal, bgw, dkg) | src/crypto |
| permit-lang b7e1228 | `langc mcp`, `check --json`, SKILL.md, the token comparison | docs/PLAN.md |
| mandate-lang 33556e9 | the x402 route | docs/VALIDATION.md line 51, test/run/mandate-x402.script |
| auction-lang 0c599f1 | the auction laws (theorem names) | Lean sources |

## 12. Slices

One session for each slice: scope, gate, mutants, docs, close.

| Slice | Work | Status |
|---|---|---|
| S0 | Seed from tcc-wasm, DESIGN.md and the digest, this plan, README table, AGENTS.md; gate EXIT 0; seed commit; private repo | DONE |
| S1 | The USER rules section 14. Record the rulings here. | NEXT |
| S2 | Port the Cubical TT kernel (8.1, Q-S0-14). Survey first. | |
| S3 | R0 in the kernel and the Wasm writer: `REFUSE_PARTIAL`, `test/notraptool.c`, the trap row (OI-4). | |
| S4 | `Amount`, `List`, `Outcome`, `outcome`; A1, A2, A4; `REFUSE_AMOUNT`, `REFUSE_BOUND`. | |
| S5 | `Party`, `Committee`, `Certificate`, `Params`, `checkCert`; G1 to G3; `REFUSE_SAFETY`, `REFUSE_CENSOR`, `REFUSE_FORK`. | |
| S6 | `AucID`, `Slot`, `Hash`, `Root`, `Handle`, `Commitment`, `Deposit`, `EligibilityProof`, `SealedBid`, `elgVrf`, `admit`, `slash`, `release`; P2, P3; `REFUSE_PEEK`. | |
| S7 | `Amendment`, `Ballot`, `Log` (CM), `Machine` (CC), `authorize`, `amend`, `paramsAt`, `replay`; G4, G5; `REFUSE_FIXPOINT`, `REFUSE_CLOCK`. | |
| S8 | The crypto layer (Q-S0-13): the ASSUMED rows, `REFUSE_APPROX`. | |
| S9 | T1 part 1: the Wasm module, the import ABI, the notrap rows. | |
| S10 | T1 part 2: `gaveld`, the Wasm engine (Q-S0-15), the demo D1 to D8. | |
| S11 | T5: the JSON report, `docs/REPORT-JSON.md`. | |
| S12 | T2: the ZK relation export (Q-S0-9, Q-S0-10). | |
| S13 | T3: the FHE compare circuit, the depth bound (F1). | |
| S14 | T4: the Bristol Fashion circuits. | |
| S15 | A1: canonical forms and `langc fmt`. | |
| S16 | A2: `langc check --json`. | |
| S17 | A4: `langc mcp`. | |
| S18 | `langc serve`: routes, `/mcp`, x402, the stub facilitator, the count log. | |
| S19 | A7 and B2: `server.json`, `openapi.yaml`, `llms.txt`, `funding.json`, `.github/FUNDING.yml`. | |
| S20 | A3, A5, B4: SKILL.md, G1 to G5, AGENTS.md and the README final. | |
| S21 | A6: the token comparison. | |
| S22 | Close: SPEC.md, probe/CAPABILITY.md, VALIDATION.md, B3 milestones, final gate. | |

## 13. Open items

- OI-1 The x402 field names (the `accepts` entries, the header names, the
  version) are checked against the x402 spec at S18. This plan does not
  fix them.
- OI-2 The `server.json` schema version and the package type of the stdio
  `langc mcp` bundle are checked against the MCP Registry docs at S19.
- OI-3 The `funding.json` schema and its version are checked at S19.
- OI-4 The kit row "a trap is equal to a trap" (section 2). S3 changes it.
- OI-5 The paper text says the proof shows the deposit is active; the
  displayed relation has no activity term (digest, sec 5.3).
- OI-6 The paper text lists AucID as a witness; Alg 2 does not (digest).
- OI-7 The slashUsr signature has no slot argument; signCom passes one
  (digest, Alg 1).
- OI-8 The paper gives no leaf encoding, node hash, Poseidon parameters or
  field encoding of hu, cm and root (digest, sec 6). T2 must fix them.
- OI-9 Committee loss can freeze the system. Recovery needs a clock. Not a
  v1 feature (ratify-lang OI-1).
- OI-10 The outcome leaks the winner and the price. T5 states it.
- OI-11 The views hold only up to indistinguishability (P1, Z1 to Z3).
  They stay ASSUMED.
- OI-12 The facilitator URL, the network and the payTo address are USER
  inputs. `langc serve` does not start without them.
- OI-13 The TLS proxy and the deploy are USER steps.

## 14. Questions for the USER (S0)

Each question has a recommendation (rec). S1 records the rulings.

- Q-S0-1 Amount arithmetic. (a) rec: compare, select, max and second max
  only. (b) Bounded add and sub with a static bound obligation.
- Q-S0-2 Rejections. (a) rec: `Verdict = valid | invalid Reason` with the
  eight reasons of section 2. (b) `Bool` only, and the host prints the
  reason.
- Q-S0-3 R1 classes. (a) rec: the table of section 3 (`Log` CM, `Machine`
  CC, `List` CM, `Sealed` and `Share` the identity CM at base types,
  `Proof` and `Protocol` base sorts). (b) `Protocol` as a CM too.
- Q-S0-4 The role map. (a) rec: design sec 2. (b) A variant that the USER
  names.
- Q-S0-5 Base types. (a) rec: the list of section 4.1. (b) No `Slot`: one
  root for each auction.
- Q-S0-6 Governance. (a) rec: one `Params` record, t-of-n amendments, the
  amend rule can amend itself, order by log height. (b) The auction rule is
  fixed for each `AucID`.
- Q-S0-7 The price list. (a) rec: a deploy setting, not in `Params`. (b) In
  `Params`.
- Q-S0-8 Hash. (a) rec: abstract `Hash`; Poseidon at T2 and in `gaveld`.
  (b) Poseidon fixed in the language.
- Q-S0-9 Proof of the outcome. (a) rec: a T2 proof of the auction outcome
  (the paper has an auction proof, Table 1). (b) t attestations of correct
  evaluation.
- Q-S0-10 T2 format. (a) rec: Noir (the USER ruled Noir for ratify-lang,
  Q-S0-13 (b), 2026-11-03). (b) arkworks R1CS for Groth16 on BN254 (the
  paper's system).
- Q-S0-11 T3 scheme. (a) rec: TFHE (integer compare is native). (b) BFV.
- Q-S0-12 T4. (a) rec: Bristol Fashion only; the MPC protocol and its
  corruption model are EXTERNAL (M1). (b) Pin one MPC protocol in this
  plan.
- Q-S0-13 Crypto location. (a) rec: C in `gaveld` behind a fixed
  host-import ABI, a port of quorum-lang `src/crypto` (ratify-lang Q-S1-1
  (a)). (b) In the module.
- Q-S0-14 Kernel source. (a) rec: the ratify-lang kernel (the permit-lang
  kernel without the affine layer, ratify-lang Q-S0-10 (a)). (b) The
  permit-lang kernel. (c) Extend the kit checker in place.
- Q-S0-15 Wasm engine in `gaveld`. (a) rec: the own C99 interpreter
  (ratify-lang Q-S0-11 (a)); node and wasmtime are oracles in the gate.
  (b) Vendor wasm3. (c) The wasmtime C API.
- Q-S0-16 `langc serve` routes. (a) rec: JSON POST routes for the five
  tools and MCP streamable HTTP at `/mcp`, each behind x402. (b) Only
  `/mcp`.
- Q-S0-17 Price in the 402 response. (a) rec: $0.01 for check, fmt,
  explain, list-base and show-law; $0.05 for build (T1 module); $0.20 for
  the T2 to T4 exports. (b) A flat $0.01. (c) A USER price list. The
  pasted range is $0.01 to $0.20.
- Q-S0-18 Network. (a) rec: USDC on Base mainnet; base-sepolia in tests.
  (b) A network that the USER names.
- Q-S0-19 Registry entry (`server.json`, name
  `io.github.MavenRain/gavel-lang`). (a) rec: a `packages` entry (a bundle
  of the stdio `langc mcp` from a GitHub release) and a `remotes` entry
  (streamable HTTP at the `langc serve` `/mcp` URL, after the USER
  deploys it). (b) `remotes` only. (c) `packages` only.
- Q-S0-20 OpenAPI. (a) rec: `openapi.yaml` 3.1; each route lists its 402
  response with the x402 body schema and the price. (b) 3.0.
- Q-S0-21 Funding files. (a) rec: `.github/FUNDING.yml` (`github:
  [MavenRain]`), `funding.json`, `llms.txt`; no
  `.well-known/ai-donate.json` (no published standard). (b) Ship
  `ai-donate.json` too.
- Q-S0-22 Measurement. (a) rec: `langc serve` writes a local count log;
  nothing leaves the host. (b) None.
- Q-S0-23 AgentFund and the human channels. (a) rec: this plan lists
  candidate milestones only; the USER runs any campaign. (b) None.
- Q-S0-24 Agent framing. (a) rec: a README section "What an agent gets" in
  ROI terms (fewer failed runs, cheaper verification). (b) No section.
- Q-S0-25 Tests. (a) rec: C and gavel-lang tests, a stub facilitator in C,
  the kit node differential stays. (b) Also a test against a public
  test-network facilitator (the network in the gate).
- Q-S0-26 Token comparison. (a) rec: the permit-lang ruling (count_tokens,
  equal outputs). (b) An offline tokenizer.
- Q-S0-27 Law counts. (a) rec: the four buckets of section 6;
  `laws_discharged` counts DISCHARGED rows only. (b) One `laws_total` for
  all rows.
- Q-S0-28 Refusal codes. (a) rec: the eleven codes of section 7. (b) Fewer
  codes: one `REFUSE_PARAMS` for G1 to G3.
