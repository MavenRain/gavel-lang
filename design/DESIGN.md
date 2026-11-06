# gavel-lang domain design

> Paste the domain design below the line, verbatim. Do not edit it.

This file is the only source of the core types and the core operations of
gavel-lang. `SPEC.md` sections 5 and 6 cite its sections. Do not add a core
type or a core operation that this file does not give.

When the design changes, paste the new version. Then record the change as an
open item in `SPEC.md` section 9.

---

## 1. Source

The request is verbatim in section 9 of this file. The paper is
arXiv:2606.14939v1, "Censorship-Resistant Sealed-Bid Auctions on
Blockchains" (Alpos, Heimbach, Nayak, Wadhwa, 12 Jun 2026). There is no
local copy of the paper text. `design/DIGEST-2606.14939.md` is the digest,
verbatim (sha256 45256713). It is data. "Alg N" and "sec N" below are
parts of the paper, as the digest gives them.

The paper runs ON chain: a deposit contract, FOCIL inclusion lists and
Groth16 on BN254. The request is OFF chain. Section 2 maps each on-chain
role to an off-chain threshold role. The S0 rulings are in `docs/PLAN.md`
section 1. The readings in this file are not ruled; `docs/PLAN.md`
section 14 lists them as questions.

## 2. The role map (on chain to off chain)

| Paper role | gavel-lang role | Paper source |
|---|---|---|
| Deposit contract with the Merkle root root_s for each slot | A committee of n members certifies root_s with a t-of-n `Certificate`. | Alg 1 registerDeposit, sec 5.2 |
| FOCIL inclusion lists (censorship resistance) | A t-of-n timestamp certificate on each bid commitment cm. To censor a bid, n - t + 1 members must refuse to sign. | sec 4, sec 5.3 |
| Simultaneous release | Threshold decryption (FHE key shares) or MPC reconstruction, only after a certified deadline. | sec 3 |
| No free bid withdrawal (slashUsr) | Slashing by evidence: two cm under one hu give a certified log entry. | Alg 1 slashUsr |
| Eligibility proof pi^m | The same relation, statement (root_s, hu, cm). T2 exports it. | sec 5.3, Alg 2 |
| Winner and price | T3 (FHE) or T4 (MPC) computes them, with a T2 proof or t attestations of correct evaluation. | sec 5.4 |
| The DAO that sets the auction rules | One `Params` record. Only a t-of-n authorized `Amendment` changes it. The amend rule can amend itself. | (request) |

## 3. Base types (draft)

| Type | Meaning | Source |
|---|---|---|
| `AucID` | The name of one auction. | sec 5.1 |
| `Slot` | A deposit slot s. | sec 5.2 |
| `Height` | A position in the log. The only time. There is no clock. | ratify-lang |
| `Hash` | An abstract hash (Poseidon at T2 and in `gaveld`). | sec 6 |
| `Root` | A Merkle root root_s. | Alg 2 |
| `Handle` | hu = Hash(rho_u \|\| AucID). | sec 5.3 |
| `Commitment` | cm = Hash(tx \|\| C_u). | sec 5.3 |
| `Deposit` | The leaf (A_u, C_u), with C_u = Hash(rho_u). | Alg 1 |
| `EligibilityProof` | pi^m for the statement (root_s, hu, cm). | Alg 2 |
| `Amount` | A bid or a price. Bounded. | sec 5.4 |
| `SealedBid` | Three carriers: the commitment cm, an FHE ciphertext, MPC shares. | sec 5.4 |
| `Party`, `Committee` | The members, n, t and the fault bound f. | ratify-lang |
| `Certificate` | t of n signatures on one message. | (role map) |
| `Verdict` | `valid` or `invalid Reason`. A rejection is a value. | R0 |
| `Outcome` | The winner and the price, or `noSale`. | sec 5.4 |
| `Params` | The auction rule (first or second price), the reserve, the committee, t, n, f, the deadlines. | (request) |
| `Amendment`, `Ballot`, `Log` | A change of `Params`, a vote on it, the ordered list of entries. | ratify-lang |

## 4. Operations (draft)

All operations are total. A failed check gives `invalid Reason`.

- `checkCert : Committee -> Message -> Certificate -> Bool`.
- `elgVrf : Root -> Handle -> Commitment -> EligibilityProof -> Bool`. The
  proof check is a host import at T1 (Alg 2 elgVrf).
- `admit : Params -> Log -> SealedBid -> Certificate -> Verdict`. The bid
  needs a timestamp certificate before the deadline, a handle that is not
  slashed, and a valid eligibility proof.
- `slash : Handle -> (Commitment, EligibilityProof) -> (Commitment,
  EligibilityProof) -> Verdict`. Both proofs verify and cm differs from
  cm' (Alg 1 slashUsr).
- `release : Params -> Certificate -> Verdict`. Decryption shares count
  only after a certified deadline.
- `outcome : Params -> List Amount -> Outcome`. The denotation in the
  clear: first price or second price, the reserve, and a fixed tie-break
  (the lowest index). Amount admits only compare, select, max and
  second max (reading 1 (a)).
- `authorize : Params -> List Ballot -> Bool` and `amend : Params ->
  Amendment -> Certificate -> Verdict`. The old amend rule judges the new
  `Params`.
- `paramsAt : (l : Log) -> Fin (length l + 1) -> Params` and `replay :
  Log -> List Verdict`.

## 5. The governance layer

The governance layer is the ratify-lang reading (self-amendment by log
height). `Params` holds the auction rule and the committee. An amendment
at height h is valid only when the `Params` at height h authorize it.
Amendments order by log height, and each height holds at most one entry.
Ballots use ZK eligibility and an FHE or MPC tally, as in ratify-lang.

## 6. Laws and their sources

| Law | Statement | Source |
|---|---|---|
| P1 Hiding | No party learns a bid before the release. | paper sec 3 |
| P2 Simultaneous release | All bids open at one certified height, or none open. | paper sec 3 |
| P3 No free bid withdrawal | Two valid commitments under one handle give a slash entry. | paper sec 3, Alg 1 |
| P4 Handle uniqueness | One deposit gives one handle for each auction. | paper Lemma 14 |
| P5 Completeness | An honest eligible bid is admitted. | paper Lemma 12 |
| G1 Safety | t > f: the faulty members alone cannot make a certificate. | role map |
| G2 Censorship resistance | n - f >= t: the honest members alone can certify a bid. | role map |
| G3 Fork freedom | Two accepting sets of the amend rule overlap in more than f members. | ratify-lang O5 |
| G4 Log category | `nil` is the unit; `append` is associative. | ratify-lang TCM-E |
| G5 Causality | An entry reads only the `Params` of earlier heights. | ratify-lang O10 |
| A1 First price | The winner bid is at least each other bid; the price is the winner bid. | auction-lang |
| A2 Second price | The price is the second highest bid, at most the winner bid. | auction-lang |
| A3 Truthfulness | Under second price, a true bid is a dominant strategy. | auction-lang (theorem name only) |
| A4 Tie-break | Equal top bids give the lowest index, so the outcome is a function. | R0 |

`docs/PLAN.md` section 6 puts each law in a bucket: DISCHARGED, ASSUMED,
AT TARGET or EXTERNAL.

## 7. Four C programs

These are four different programs. Do not merge them.

- `langc`: the compiler (parse, check, fmt, build, the T2 to T5 exports).
- `langc mcp`: the AUTHORING MCP server over stdio for agents that WRITE
  gavel-lang. Tools: check, fmt, explain, list-base, show-law.
- `langc serve`: the same tools over HTTP, each behind an x402 payment,
  plus MCP streamable HTTP at `/mcp`. It links libcurl for the facilitator
  calls and runs behind a TLS proxy. The USER gives the facilitator URL
  and the payTo address. The repo holds no wallet address.
- `gaveld`: the T1 host. It runs a compiled module on bids, certificates
  and logs, with the crypto behind a fixed host-import ABI. It holds no
  secret key and does not bid.

## 8. Not in v1

On-chain auctions are sealed-lang. A governance layer with no auction is
ratify-lang. CKKS is refused (approximate arithmetic). A wall clock is
refused (time is log height).

## 9. The request (USER, 2026-11-05, verbatim)

The words below are verbatim from the USER ask. The headings are new.
Three changes only. The paste had one trailing space after "critera:";
that space is not here, because `git diff --check` refuses trailing white
space. Block 2 had one en dash in "$0.01-$0.20"; it is a hyphen here,
because the kit gate refuses en dashes and em-dashes. Block 2 had curly
quotes; they are straight quotes here.
### 9.1 The ask, verbatim

I want to write a new programming language with the following characteristics:

First, the compiler shall not permit any runtime error (aka trap), period.

As far as type constructors/higher-kinded types, admit only cell monads and their duals as represented computationally in Cubical Type Theory.

As far as base types and operations, admit only what is needed to express off-chain ZK/FHE/MPC sealed-bid auction protocols (reference: arXiv:2606.14939) governed by an off-chain ZK/FHE/MPC threshold-authorized self-amending system (like the off-chain version of a DAO).

The host language shall be TinyCC.  Use your judgment to surface multiple-choice option(s) for compilation target(s).

Finally, I want agents to reach for this language when it first their use case(s).  To that end, adhere to the following additional critera:

### 9.2 Pasted block 1, verbatim

```
  - one canonical form per operation and a langc fmt canonical printer;
  - langc check --json, giving fix hints and progress counts (parsed, typed, laws
    discharged);
  - a SKILL.md with 3 to 5 graded examples;
  - an MCP server written in TinyCC (langc mcp);
  - AGENTS.md and a README table saying when to use the language;
  - a token-count comparison against Python and Rust.
  - List where agents look. That means an MCP server entry in the public
     registries, an OpenAPI description, and the price in the 402 response.
```

### 9.3 Pasted block 2, verbatim

```
1. Make the work agent-consumable and payable (highest leverage)

Wrap existing formalizations or verification workflows as MCP tools or simple x402-gated endpoints. Examples: "verify this Halo2 gadget against the formalization," "search formalized properties of this circuit," "check this snippet for known invariants," or "request a formalization sketch of gadget X."
Price per use in the $0.01-$0.20 range (or higher for heavier verification). Agents (or their principals) will use tools that improve success rate or reduce verification cost.
Publish Agent Skills (SKILL.md with name + description frontmatter) that teach agents how and when to use your libraries or tools. Skills are an open standard (originally from Anthropic, now under the Agentic AI Foundation) and load progressively, so discovery is cheap.

2. Publish machine-readable discovery and funding signals

Add FUNDING.yml plus a simple funding.json (or equivalent) to GitHub repos.
Put an llms.txt (and, where relevant, .well-known/ai-donate.json or similar) on any site or content property that describes the projects, transparency, and explicit welcome for agent payments.
Include AGENTS.md or Skills in the repos so coding agents that encounter your code can surface funding options.
Agents parse these files the way humans read a README or Sponsors page.

3. Use agent-native funding rails for larger efforts

Run specific formalization milestones on AgentFund (or similar) with escrow and clear deliverables. Agents can discover, evaluate, contribute, and vote.
Keep human channels (GitHub Sponsors, Open Collective, Patreon) as the primary near-term path; treat agent micropayments and campaigns as complementary. Better tools that agents actually use also generate recommendations and visibility that drive human sponsors.

4. Frame the value in agent terms

Position the work as shared infrastructure for agentic systems that need verifiable correctness: formalized payment or agent protocols, ZK compliance layers, verified gadgets that other agents can rely on. Agents (and the people who fund them) respond to clear ROI on reliability and reduced verification cost more than to general appeals.
Practical starting sequence

Add machine-readable funding metadata (FUNDING.yml, llms.txt, funding manifest) to the main repos.
Expose one useful Lean/ZK capability as an MCP tool or lightweight x402 endpoint (Cloudflare Workers + facilitator, or a simple Express + x402 middleware, are common starting points).
Publish a short Skill that documents how to use it.
List the endpoint/tool in relevant MCP or x402 discovery surfaces and mention it in content that reaches agent developers.
Measure actual agent traffic and payments before investing heavily in larger campaigns.
```

(The original uses curly quotes in block 2; this copy keeps the words and uses straight quotes.)
