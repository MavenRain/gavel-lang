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

Status: S8b DONE (commit pending), S8c NEXT. S8b adds the BN254 pairing
(the C builtin `pairCheckC` and the guest spec), the Groth16 verifier
`g16Verify`, `elgVrf` on Groth16, `outcomeVrf`, `shareVrf` and the golden
Noir proofs in `test/noir/` (Q-S8b-1 (a) to Q-S8b-20 (a)).
S8a DONE: keccak-256,
Poseidon, the BN254 G1 curve, the Schnorr check, the builtins `feMulC` and
`ptMulC`, the signed certificate and the Poseidon messages, and the
kernel parts for them: the open `wEq` rule, the sealed defs and the memo
(Q-S8a-1 (a) to Q-S8a-10 (a)). S8 DONE: the machine `Word`
and its ops, the `SwapWord` rename, the BN254 field `Fr` and `Fp`, the C
oracle, `REFUSE_WORD` and `REFUSE_APPROX` (Q-S8-1 (a) to Q-S8-9 (a)).
S7 DONE (the log): the Cubical TT kernel of ratify-lang
a46d5df (a port of permit-lang 3d47120), R0 from ratify-lang 71f2c22 and
the domain words are in: `Amount`, `Bids`, `Rule`, `Outcome`, `outcome`,
the laws A1, A2 and A4 (S4), `Hash`, `Party`, `Members`, `Committee`,
`Certificate`, `Params`, `checkCert`, `outcomeOf`, the laws G1 to G3 as
proof fields, `REFUSE_SAFETY`, `REFUSE_CENSOR`, `REFUSE_FORK` and the
committee half of `REFUSE_BOUND` (S5), and from S6 `Slot`, `Height`, the
deadline fields of `Params`, `AucID`, `Root`, `Handle`, `Commitment`,
`Deposit`, `EligibilityProof`, `Sealed`, `SealedBid`, `Reason`, `Verdict`,
`elgVrf`, `admit`, `slash`, `Opening`, `release`, the laws P2 and P3,
`REFUSE_PEEK`, `REFUSE_DEADLINE` and the sealed and handle half of
`REFUSE_BOUND` (Q-S6-1 (b) to Q-S6-9 (a)), and from S7 `Ballot`,
`Ballots`, `Amendment`, `authorize`, `amendCheck`, `Entry`, `Log`,
`append`, `paramsAt`, `Machine`, `Verdicts`, `replay`, `slashedOf`, the
laws G4 and G5, `REFUSE_FIXPOINT`, `REFUSE_CLOCK` and the ballot half of
`REFUSE_BOUND` (Q-S7-1 (a) to Q-S7-10 (a)). Section 1 has the rulings.
Next: S8c (ChaCha20, the seed input and the MPC output check).

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
| 2026-11-05 | Q-S0-1 "(a) Compare only (Recommended)": `Amount` admits compare, select, max and second max only. | USER, S1 |
| 2026-11-05 | Q-S0-2 "(a) Verdict value (Recommended)": `Verdict = valid \| invalid Reason` with the eight reasons of section 2. | USER, S1 |
| 2026-11-05 | Q-S0-3 "(a) Section 3 table (Recommended)": the R1 classes of section 3. | USER, S1 |
| 2026-11-05 | Q-S0-4 "(a) Design sec 2 (Recommended)": the role map of design sec 2. | USER, S1 |
| 2026-11-05 | Q-S0-5 "(a) Section 4.1 list (Recommended)": the base types of section 4.1, with `Slot`. Q-S1-7 adds `Word` for the prelude only. | USER, S1 |
| 2026-11-05 | Q-S0-6 "(a) Params + t-of-n (Recommended)": one `Params` record, t-of-n amendments, the amend rule can amend itself, order by log height. | USER, S1 |
| 2026-11-05 | Q-S0-7 "(a) Deploy setting (Recommended)": the price list is a deploy setting, not in `Params`. | USER, S1 |
| 2026-11-05 | Q-S0-8 "(a) Abstract Hash (Recommended)": an abstract `Hash`. Poseidon at T2 and, by Q-S0-13 (b), in the crypto prelude, not in `gaveld`. | USER, S1 |
| 2026-11-05 | Q-S0-9 "(a) T2 outcome proof (Recommended)": a T2 proof of the auction outcome. | USER, S1 |
| 2026-11-05 | Q-S0-10 "(a) Noir (Recommended)": T2 exports Noir. | USER, S1 |
| 2026-11-05 | Q-S0-11 "(a) TFHE (Recommended)": T3 uses TFHE. | USER, S1 |
| 2026-11-05 | Q-S0-12 "(b) Pin one protocol": this plan pins one MPC protocol. Not the recommendation. Edits: section 6 (M1), 8.2 (T4), 11, 12, 13. Q-S1-1 and Q-S1-2 come from this ruling. | USER, S1 |
| 2026-11-05 | Q-S0-13 "(b) In the module": the ZK, FHE and MPC crypto is in the Wasm module, not in `gaveld`. Not the recommendation. It replaces the clause "The ZK, FHE and MPC crypto is behind a fixed import ABI" of the S0 T1 row. Edits: section 2, 4.1, 4.2, 6, 7, 8.2, 8.3, 10, 11, 12, 13; design sec 3, 4 and 7. Q-S1-3, Q-S1-4, Q-S1-7, Q-S1-8 and Q-S1-10 come from this ruling. | USER, S1 |
| 2026-11-05 | Q-S0-14 "(a) ratify-lang kernel (Recommended)": S2 ports the ratify-lang kernel (the permit-lang kernel without the affine layer). | USER, S1 |
| 2026-11-05 | Q-S0-15 "(a) Own C99 interpreter (Recommended)": `gaveld` runs the module on its own C99 Wasm interpreter; node and wasmtime are oracles in the gate. | USER, S1 |
| 2026-11-05 | Q-S0-16 "(a) POST routes + /mcp (Recommended)": JSON POST routes for the five tools and MCP streamable HTTP at `/mcp`, each behind x402. | USER, S1 |
| 2026-11-05 | Q-S0-17 "(a) Tiered 0.01/0.05/0.20 (Recommended)": $0.01 for check, fmt, explain, list-base and show-law; $0.05 for build (T1 module); $0.20 for the T2 to T4 exports. | USER, S1 |
| 2026-11-05 | Q-S0-18 "(a) USDC on Base (Recommended)": USDC on Base mainnet; base-sepolia in tests. | USER, S1 |
| 2026-11-05 | Q-S0-19 "(a) packages + remotes (Recommended)": `server.json` has a `packages` entry (the stdio `langc mcp` bundle) and a `remotes` entry (the `langc serve` `/mcp` URL, after the USER deploys it). | USER, S1 |
| 2026-11-05 | Q-S0-20 "(a) 3.1 with 402s (Recommended)": `openapi.yaml` 3.1; each route lists its 402 response with the x402 body schema and the price. | USER, S1 |
| 2026-11-05 | Q-S0-21 "(a) No ai-donate.json (Recommended)": `.github/FUNDING.yml`, `funding.json`, `llms.txt`; no `.well-known/ai-donate.json`. | USER, S1 |
| 2026-11-05 | Q-S0-22 "(a) Local count log (Recommended)": `langc serve` writes a local count log; nothing leaves the host. | USER, S1 |
| 2026-11-05 | Q-S0-23 "(a) Candidate milestones (Recommended)": this plan lists candidate milestones only; the USER runs any campaign. | USER, S1 |
| 2026-11-05 | Q-S0-24 '(a) "What an agent gets" (Recommended)': the README gets the section "What an agent gets" in ROI terms. | USER, S1 |
| 2026-11-05 | Q-S0-25 "(b) Also public testnet": also a test against a public test-network facilitator; the network is in the gate. Not the recommendation. Edits: section 9 (USER steps), 10, 12 (S18), 13 (OI-19). Q-S1-5 and Q-S1-6 come from this ruling. | USER, S1 |
| 2026-11-05 | Q-S0-26 "(a) permit-lang ruling (Recommended)": count_tokens, equal outputs. | USER, S1 |
| 2026-11-05 | Q-S0-27 "(a) Four buckets (Recommended)": the four buckets of section 6; `laws_discharged` counts DISCHARGED rows only. | USER, S1 |
| 2026-11-05 | Q-S0-28 "(a) Eleven codes (Recommended)": the codes of section 7. Q-S1-7 (a) adds `REFUSE_WORD`, thus twelve codes. | USER, S1 |
| 2026-11-05 | Q-S1-1 "(b) WRK authenticated garbling": T4 targets the protocol of Wang, Ranellucci and Katz (CCS 2017): dishonest majority, malicious, up to n - 1 corrupt members. Not the recommendation. M1 becomes ASSUMED. Edits: section 6 (M1), 8.2 (T4), 11, 13 (OI-14, OI-15). | USER, S1 |
| 2026-11-05 | Q-S1-2 "(b) Party program too": the module also runs the WRK rounds of one member, and `gaveld` carries the messages. Not the recommendation. Edits: section 8.2 (T1, T4), 8.3 (`gaveld` member mode), 10, 12 (S14b, S14c); design sec 7. | USER, S1 |
| 2026-11-05 | Q-S1-3 "(b) Guest-language prelude": the crypto is gavel-lang terms over a new total `Word` type, checked by the kernel and lowered by langc. quorum-lang `src/crypto` is the reference and a C oracle in the gate. Not the recommendation. Edits: section 2, 4.2, 8.2, 10, 11, 12 (S8, S8b, S8c). | USER, S1 |
| 2026-11-05 | Q-S1-4 "(b) Also TFHE share checks": the module checks Poseidon, the Schnorr certificates on BN254 G1 (keccak-256 challenge, the ratify-lang Q-S1-3 (a) scheme), the Noir proofs with the BN254 pairing, the MPC outputs and each TFHE decryption share. `gaveld` has no crypto code. Not the recommendation. Edits: section 4.2, 6 (Z5), 8.2 (T1, T3), 12 (S8b, S13), 13 (OI-16, OI-17). | USER, S1 |
| 2026-11-05 | Q-S1-5 "(a) Strict: gate fails (Recommended)": from S18, `make check` runs the test-network row and fails when the network or a test input is absent. The USER gives the inputs in environment variables; the repo holds none of them. | USER, S1 |
| 2026-11-05 | Q-S1-6 "(a) Verify and settle (Recommended)": each run pays $0.01 test USDC on base-sepolia; the log keeps the settle transaction hash. The C test client signs the EIP-3009 authorization (keccak-256 and secp256k1 in C99 under `test/`). | USER, S1 |
| 2026-11-05 | Q-S1-7 "(a) Prelude only (Recommended)": only the crypto prelude names `Word` (wrapping 32-bit: add, sub, mul, mulhi, and, or, xor, not, shifts, rotates, compare; no division). A user program that names it gets `REFUSE_WORD`. Edits: section 4.1, 7, 12 (S8). | USER, S1 |
| 2026-11-05 | Q-S1-8 "(a) Noir proof per share (Recommended)": each member gives a Noir proof that its TFHE share is the partial decryption under its committed key share with bounded noise. T2 exports the relation; the in-module Noir verifier checks it. Edits: section 6 (Z5), 8.2 (T2, T3), 12 (S12). | USER, S1 |
| 2026-11-05 | Q-S1-9 "(b) TCP links": in member mode `gaveld` opens TCP links to the other members, with TLS by a proxy. Not the recommendation. Edits: section 2, 6 (M2), 8.3, 9 (USER steps), 12 (S14c), 13 (OI-18). | USER, S1 |
| 2026-11-05 | Q-S1-10 "(a) OS seed per run (Recommended)": in member mode `gaveld` reads 32 bytes from the OS (getentropy) for each run and gives them to the module as an input; the prelude expands them with ChaCha20 on `Word`. Edits: section 2, 6 (M3), 8.3, 12 (S8c). | USER, S1 |
| 2026-11-06 | Q-S2-1 "(a) ratify a46d5df (Recommended)": the port base is ratify-lang a46d5df (its S2 kernel commit), with name edits only. S3 takes the 71f2c22 R0 delta. The ratify-lang S4 to S7 domain parts are not ported. Edits: sections 11, 12. | USER, S2 |
| 2026-11-06 | Q-S2-2 "(a) Accept until S3 (Recommended)": `src/front/lower.c`, `src/wasm.c` and `src/ir.h` stay at the kit text and out of the build. `langc build` gives `REFUSE_LOWER` "target pending S3". The IR rows, the Wasm modules and the differential cases come back at S3. Edits: sections 12, 13 (OI-20). | USER, S2 |
| 2026-11-06 | Q-S2-3 "(a) Keep as refusals (Recommended)": the four affine rows stay as parse or check refusals: affine-arrow.lang (`LEX_CHAR`), affine-budget.lang and affine-unpack.lang (`NAME_UNKNOWN`), affine-mult.lang (`TYPE_MISMATCH`). No affine code. Edits: section 12 (S2). | USER, S2 |
| 2026-11-06 | Q-S2-4 "(a) Keep in S2 (Recommended)": the state and history part of the kernel (`state`, `REFUSE_STATE`, `REFUSE_HISTORY_WRITE`) stays byte equal to a46d5df. A later slice can cut it. Edits: section 12 (S2). | USER, S2 |
| 2026-11-06 | Q-S3-1 "(a) Take 71f2c22 files (Recommended)": `docs/host/CAPABILITY.md` and `docs/host/README.md` come from ratify-lang 71f2c22 with one name edit (CAPABILITY.md line 6, "gavel-lang S3"). Edits: section 12 (S3). | USER, S3 |
| 2026-11-06 | Q-S3-2 "(a) Gavel S3 status (Recommended)": the README.md status line and the AGENTS.md section "State at S3" give the S3 state in gavel-lang words, on the model of ratify-lang 71f2c22 README.md lines 13 to 18 and AGENTS.md lines 6 to 14. Edits: README.md, AGENTS.md. | USER, S3 |
| 2026-11-06 | Q-S4-1 "(a) Inductive (Recommended)": `family Amount := amZero | amSucc (amPred : Amount)` in `domain/domain.lang`, so A1, A2 and A4 are proofs by elim and stay DISCHARGED. In a program, an elim, a fold or a projection of `Amount`, or `amSucc` on a term that is not a literal, gives `REFUSE_AMOUNT`: a program uses only `amLe`, `amSelect`, `amMax` and `amMin`. Edits: sections 12 (S4), 13 (OI-21). | USER, S4 |
| 2026-11-06 | Q-S4-2 "(a) Bids family (Recommended)": `family Bids := bdNil | bdCons (bdHead : Amount) (bdTail : Bids)`, the free monoid at `Amount`. The section 3 `List` row reads as `Bids`; the core `List` is kit only. Edits: sections 3, 12 (S4). | USER, S4 |
| 2026-11-06 | Q-S4-3 "(a) Domain keeps it (Recommended)": the domain owns `Outcome` and `outcome`. The kernel signature slot `Outcome` becomes `DaoOutcome` in `src/front/check.c`, in the DAO prelude of `src/front/front.c` and in 23 `.lang` files; the code of each row stays the same. The guest `outcome` rename was not necessary. Edits: sections 12 (S4), 13 (OI-23). | USER, S4 |
| 2026-11-06 | Q-S4-4 "(a) Rule + reserve (Recommended)": `family Rule := firstPrice | secondPrice` and `outcome : Rule -> Amount -> Bids -> Outcome` (the rule, the reserve, the bids). At S5, `outcomeOf (p : Params)` gives the DESIGN.md form. Edits: section 12 (S4). | USER, S4 |
| 2026-11-06 | Q-S4-5 "(a) Reserve floor (Recommended)": `family Outcome := noSale | sale (slWinner : Size) (slPrice : Amount)`, with a 0-based winner index into `Bids`. A bid below the reserve is dropped. First price: the price is the winner bid. Second price: the price is the larger of the reserve and the second highest eligible bid. No eligible bid gives `noSale`. Edits: sections 6 (A2), 12 (S4). | USER, S4 |
| 2026-11-06 | Q-S4-6 "(a) 1024 bids (Recommended)": a closed `Bids` value with more than 1024 bids in a def type or a def body gives `REFUSE_BOUND` (`BID_LIST_BOUND` in `src/front/check.c`). An open input list passes at check time; the T1 entry bounds it at S9. Edits: sections 12 (S4), 13 (OI-22). | USER, S4 |
| 2026-11-06 | Q-S4-7 "(a) General in domain (Recommended)": the general A1, A2 and A4 terms (`Path` types, proofs by elim on `Bids` and `Amount`) are in `domain/domain.lang`. `examples/auction.lang` has the closed `refl` rows and the eval rows. Edits: section 12 (S4). | USER, S4 |
| 2026-11-06 | Q-S5-1 "(a) Member list (Recommended)": `family Party := party (ptKey : Size)` (abstract key; S8 makes it a BN254 G1 key); `family Members := mbNil | mbCons (mbHead : Party) (mbTail : Members)`; a `Committee` holds the members, t and f; n is the length of the members. A repeated party breaks G1 (`REFUSE_SAFETY`). Edits: sections 4.1, 12 (S5), 13 (OI-25). | USER, S5 |
| 2026-11-06 | Q-S5-2 "(a) Size + szLe, szEq (Recommended)": t, n, f and ta are `Size`. The words are `szLe`, `szLt`, `szEq`, `szAdd`, `sAnd`, `sOr`, `sNot`, `ptEq`, `hsEq`, `mbLen`, `mbHas` and `mbDistinct`. The result type is `Sum Unit Unit`, and `inr unit` means yes (the `amLe` convention). Edits: section 12 (S5). | USER, S5 |
| 2026-11-06 | Q-S5-3 "(a) Abstract Hash now (Recommended)": `family Hash := hash (hsTag : Size)`. The DESIGN `Message` argument reads as `Hash`. S6 keeps this `Hash`. Edits: sections 4.1, 12 (S5, S6). | USER, S5 |
| 2026-11-06 | Q-S5-4 "(a) Ideal signature list (Recommended)": `family Certificate := ctNil | ctCons (ctSigner : Party) (ctMsg : Hash) (ctRest : Certificate)`. `checkCert` counts each member one time when the certificate has an entry with that signer and that message, and accepts when the count is at least t. Equal entries count one time. At S8 the entry match becomes the Schnorr check. Edits: section 12 (S5), 13 (OI-25). | USER, S5 |
| 2026-11-06 | Q-S5-5 "(c) Proof fields": G1 to G3 are `Path` proof fields, and the kernel checks them as types. There is no C decision of G1 to G3 on closed values. Not the recommendation. Edits: the law fields `cmSafe`, `cmCensor` and `prFork`; sections 6, 12 (S5). | USER, S5 |
| 2026-11-06 | Q-S5-6 "(b) Separate amend t": `Params` has a separate amend threshold ta (`prAmendT : Size`). G3 holds on ta (2ta - n > f, in the field n + f < ta + ta). G1 and G2 hold on the certificate t. Not the recommendation. Edits: `prAmendT` and `prFork` in `Params`; sections 6, 12 (S5). | USER, S5 |
| 2026-11-06 | Q-S5-7 "(a) 1024 members (Recommended)": `#define COMMITTEE_BOUND 1024u`. A closed `Members` value with more than 1024 members in a program def type, body, nested value or consumed subterm gives `REFUSE_BOUND`. An open committee passes; the T1 entry bounds it later. Edits: sections 7, 12 (S5), 13 (OI-26). | USER, S5 |
| 2026-11-06 | Q-S5-8 "(a) At S6 (Recommended)": there are no deadlines at S5. S6 adds the deadline fields with `Slot` and `release` (P2). Edits: sections 12 (S6), 13 (OI-27). | USER, S5 |
| 2026-11-06 | Q-S5-9 "(a) Field failure = code (Recommended)": a law field that does not check gives the law code: the G1 field `REFUSE_SAFETY`, the G2 field `REFUSE_CENSOR`, the G3 field `REFUSE_FORK`. This holds for every term (`refl` on closed numbers that break the law, or no proof for open numbers). G1 to G3 hold for open inputs too: DISCHARGED by the type, as P2. Edits: sections 6, 7, 12 (S5). | USER, S5 |
| 2026-11-06 | Q-S5-10 "(a) G1, G2 in Committee (Recommended)": `Committee := committee (cmMembers) (cmT) (cmF) (cmSafe : G1 field) (cmCensor : G2 field)`. G1: no repeated party and f < t. G2: t + f <= n. `Params := params (prRule) (prReserve) (prCommittee) (prAmendT) (prFork : G3 field)`. G3: n + f < ta + ta. Edits: sections 6, 12 (S5). | USER, S5 |
| 2026-11-06 | Q-S5-11 "(a) G3 only (Recommended)": there is no upper law on ta. New open item: ta > n - f stops amendments by the honest members (a loss of liveness, not of safety). S7 states it. Edits: sections 12 (S5), 13 (OI-24). | USER, S5 |
| 2026-11-06 | Q-S6-1 "(b) (a) plus an order proof": new `family Height := height (htOf : Size)`. `Params` gets `prSlot : Slot`, `prStampEnd : Height` and `prRelease : Height` before `prFork`, and a proof field after `prFork`: `prOrder : Path (Sum Unit Unit) (htLt prStampEnd prRelease) (inr unit)`. Edits: sections 4.1, 12 (S6), 13 (OI-27). | USER, S6 |
| 2026-11-06 | Q-S6-2 "(a) Opening proof field": `family Opening := opening (opAuc : AucID) (opParams : Params) (opCert : Certificate) (opOk : Path (Sum Unit Unit) (checkCert (prCommittee opParams) (openMsg opAuc (prRelease opParams)) opCert) (inr unit))`. `release : Opening -> SealedBids -> Bids` opens the whole list in one call. A failed `opOk` gives `REFUSE_PEEK`. P2 is DISCHARGED by the type. Edits: sections 6, 7, 12 (S6). | USER, S6 |
| 2026-11-06 | Q-S6-3 "(a) Any guest read": `sealed family Sealed (A : Type 0) := seal (sealOf : A)`. A guest program def that elims, folds or projects a sealed family gives `REFUSE_PEEK`, law-typed defs included. `REFUSE_LAW` stays for the other law-only names (decrypt, observe). The domain, where `release` is, is exempt. A build with `seal` is allowed. Edits: sections 7, 12 (S6). | USER, S6 |
| 2026-11-06 | Q-S6-4 "(a) slash decision + law": `slash : Root -> Handle -> Commitment -> EligibilityProof -> Commitment -> EligibilityProof -> Verdict` gives `valid` when both `elgVrf` checks say yes and the two commitments differ. Else it gives `invalid badProof`, or `invalid notAccepted` for equal commitments. P3 at S6 is the path law `p3Slash` on `slash`. The log entry and `replay` are S7. The `Root` argument is new (OI-7). Edits: sections 6, 12 (S6), 13. | USER, S6 |
| 2026-11-06 | Q-S6-5 "(a) Ideal proof record": `EligibilityProof := elgProof (epRoot : Root) (epHandle : Handle) (epCommit : Commitment)`. `elgVrf : Root -> Handle -> Commitment -> EligibilityProof -> Sum Unit Unit` says yes when the three match. At S8b the match becomes the Noir verifier. New open item. Edits: sections 12 (S6), 13. | USER, S6 |
| 2026-11-06 | Q-S6-6 "(a) Size tags + Hash wraps": `aucId (auTag : Size)`, `slot (soTag : Size)`; `root (rtHash : Hash)`, `handle (hdHash : Hash)`, `commitment (coHash : Hash)`; `Deposit := deposit (dpOwner : Size) (dpSecret : Hash)`, with no amount. Edits: sections 4.1, 12 (S6). | USER, S6 |
| 2026-11-06 | Q-S6-7 "(a) Verdict + Handles now": `Reason` (badCert, badProof, late, early, slashed, notAccepted, wrongHeight, malformed) and `Verdict := valid | invalid (vdWhy : Reason)`. `admit : Params -> Handles -> Root -> SealedBid -> Height -> Certificate -> Verdict`. `Handles` is the slashed set; S7 takes it from the Log. A closed `SealedBids` or `Handles` list with more than 1024 entries gives `REFUSE_BOUND` (two new bounds). Edits: sections 7, 12 (S6), 13. | USER, S6 |
| 2026-11-06 | Q-S6-8 "(a) szPair + msg words": a total injective pairing `szPair : Size -> Size -> Size` (Cantor, by fold and `szAdd`), and a kind tag so the two messages differ: `stampMsg : Commitment -> Height -> Hash` and `openMsg : AucID -> Height -> Hash`. At S8 and T2 the message is Poseidon. Edits: sections 12 (S6), 13. | USER, S6 |
| 2026-11-06 | Q-S6-9 "(a) New REFUSE_DEADLINE": a follow-up to Q-S6-1 (b). A failed `prOrder` gives `REFUSE_DEADLINE` (the stamp end is before the release). Section 7 has one new row. `prOrder` is after `prFork`, so `fork.lang` still fails at `prFork` first. Edits: sections 7, 12 (S6). | USER, S6 |
| 2026-11-07 | Q-S7-1 "(a) Indexed Entry and Log": `family Entry : Params -> Params -> Type 0` with `enAmend` (a proof that the EARLIER Params accept the amendment; it moves the Params), `enSlash` (raw slash evidence; the Params stay) and `enAdmit` (a raw bid claim; the Params stay). `family Log : Params -> Params -> Type 0 := lgNil \| lgCons`, and `append`. The ballots are inside the amendment. Releases stay outside the Log (OI-35). | USER, S7 |
| 2026-11-07 | Q-S7-2 "(a) Ballot and authorize": `Ballot := ballot (blVoter) (blTopic) (blYes)`, `Ballots := blNil \| blCons`. `authorize` is yes: at least ta yes ballots on the topic from distinct committee members. `Amendment := amendment (amTag) (amTo) (amBallots)`. The amend check does `authorize` first (else `notAccepted`), then `checkCert` on `amendMsg` with the kind tag `szS (szS sz0)` (else `badCert`), else `valid`. The S7 word is `amendCheck`, not `amend` (the DAO prelude has the word `amend`). Edits: sections 4.2, 12 (S7). | USER, S7 |
| 2026-11-07 | Q-S7-3 "(a) Total paramsAt": `paramsAt : (p q : Params) -> Log p q -> Height -> Params`. Height 0 gives p, height h + 1 gives the tail at h, and a height past the end gives q (the clamp replaces the DESIGN `Fin` bound). `Machine := Stream Params`, `machineOf` by `sCorec`. | USER, S7 |
| 2026-11-07 | Q-S7-4 "(a) replay with Handles from the Log": `Verdicts := vdNil \| vdCons`. `replay` is one elim with the motive `Handles -> Verdicts`. A valid slash entry adds its handle. `admit` judges an admit entry with the handles so far. An amend entry gives `valid`. `slashedOf` gives the handles of a log. `admit` keeps its S6 signature. OI-29 is CLOSED. P3 on the log is the path law `p3Replay`. | USER, S7 |
| 2026-11-07 | Q-S7-5 "(a) G4 and G5": G4 is `appendNilL`, `appendNilR` and `appendAssoc` (paths by induction) with closed `refl` rows. G5 holds by the type (the `enAmend` proof field reads only the earlier Params), with `paramsAt` rows and the negative row `amend-self.lang`. | USER, S7 |
| 2026-11-07 | Q-S7-6 "(a) REFUSE_FIXPOINT": the kit code `REFUSE_REC` is renamed `REFUSE_FIXPOINT` at its 2 sites (rule R3). The `LAW_FIELDS` row `Entry`, `eaOk` gives `REFUSE_FIXPOINT`: an amendment that the earlier Params do not accept is Params that authorize themselves. New test `test/check/amend-self.lang`. | USER, S7 |
| 2026-11-07 | Q-S7-7 "(a) REFUSE_CLOCK": the kit code `REFUSE_TIME` is renamed `REFUSE_CLOCK` at its 4 sites. The time words get `deadline`, `timeout` and `expires` (9 words). The hint says that time is the log height and names `Height`. New tests `time-deadline`, `time-timeout`, `time-expires`. | USER, S7 |
| 2026-11-07 | Q-S7-8 "(a) State OI-24": OI-24 is stated, with no law and no refusal. One closed eval row: ta > n - f, and `authorize` on all n - f honest yes ballots gives `inl unit`. | USER, S7 |
| 2026-11-07 | Q-S7-9 "(a) Linear szLe": `szLe` uses the linear words `szIsZ` and `szDec`, and `szLt` is below `szDec`. The S5 and S6 rows stay green. OI-34 is CLOSED. | USER, S7 |
| 2026-11-07 | Q-S7-10 "(a) Ballot bound": `BALLOT_BOUND` is 1024u, with the `LIST_BOUNDS` row `Ballots`. A closed `Ballots` value with more than 1024 ballots gives `REFUSE_BOUND`. New test `test/check/bound-ballots.lang`. The indexed `Log` has no bound (OI-36), and `Verdicts` has no bound. | USER, S7 |
| 2026-11-07 | Q-S8-1 "(a) Two commits": S8 is `Word` and its native ops, `REFUSE_WORD`, the `SwapWord` rename, the Fr and Fp field, `REFUSE_APPROX`, the C oracle tool with the field rows and the ASSUMED rows. The new row S8a is Poseidon, keccak-256, the G1 curve, the Schnorr check, the Party key and the Poseidon message words (it closes OI-25 and OI-30). S8b and S8c do not change. Each part ends in a USER commit. `elgVrf` stays ideal until S8b (OI-28). | USER, S8 |
| 2026-11-07 | Q-S8-2 "(a) Rename bag to SwapWord": only the domain family `Word` (bag words of adjacent swaps) is now `SwapWord`. Its constructors and fields keep their names. The machine type is `Word` (Q-S1-7 (a)). | USER, S8 |
| 2026-11-07 | Q-S8-3 "(a) Native uint32 value": `CORE_WORD` and `VAL_WORD` (uint32, wrapping). O(1) ops `wAdd`, `wSub`, `wMul`, `wMulHi`, `wAnd`, `wOr`, `wXor`, `wNot`, `wShl`, `wShr`, `wRotl`, `wRotr`, `wLt`, `wEq` (a compare gives `Sum Unit Unit`). Literals `0x1fw` and `31w` (`LEX_WORD_RANGE` above 0xffffffff). A guest program that names them gets `REFUSE_WORD`; only the domain file and the kit mode may. `Word` lowers to Wasm i32 ops. | USER, S8 |
| 2026-11-07 | Q-S8-4 "(a) 8 limbs, Montgomery": `Fr` and `Fp` are 8 Words, least significant first. The form is Montgomery inside (CIOS multiply) and canonical (value below the modulus) at each boundary. The curve at S8a is BN254 G1 only, type `Pt1`, affine with (0, 0) as infinity at the boundary and Jacobian inside the scalar multiply. G2 waits for the pairing (S8b). | USER, S8 |
| 2026-11-07 | Q-S8-5 "(a) circomlib t=3" (built at S8a): Poseidon is circomlib width 3 (2 inputs) on Fr, alpha 5, R_F 8, R_P 57, with the quorum-lang constants. `Hash` carries an Fr. Each message is poseidon2(tag, poseidon2(x, y)) with the kind tags 0, 1, 2; `szPair` and `szTri` go. The Merkle leaf and node encoding stay open in OI-8 for T2. | USER, S8 |
| 2026-11-07 | Q-S8-6 "(a) ratify scheme, Pt1 key" (built at S8a): `party (ptKey : Pt1)`, and `ptEq` compares canonical points. A certificate entry gains (ctR : Pt1) and (ctS : Fr). `schnorrCheck` tests s*G == R + e*P with e = keccak256(R.x, R.y, P.x, P.y, m) mod r, where m is the 32-byte big-endian Hash value. `checkCert` counts the members whose entry passes. The nonce rule is signer side, not in the prelude. | USER, S8 |
| 2026-11-07 | Q-S8-7 "(a) cryptotool + kit example": `src/crypto` is a byte copy of quorum-lang 8d1a113 (u256, bn254, keccak, poseidon). `test/cryptotool.c` builds `build/cryptotool`, which prints `FILE NAME ARGS => RESULT` rows from a fixed seed and known answers. `test/gate.sh` evaluates each row with `langc --kit eval` and prints `crypto oracle: N rows, M mismatches`. Q-S8-9 (a) moved the field rows to `test/crypto/crypto.lang` (outside the Wasm set); the Word rows stay in `examples/crypto.lang`. | USER, S8 |
| 2026-11-07 | Q-S8-8 "(a) 3 rows + ratify list": section 6 gets three ASSUMED rows (K1 Poseidon collision resistance, K2 keccak-256 as a random oracle for the challenge, K3 Schnorr EUF-CMA under discrete log on BN254 G1) and one EXTERNAL row (E1, the Word and field ops equal the C oracle). `REFUSE_APPROX` has the list `ckks`, `approx`, `approximate`, `float`, `double` (from ratify-lang 2aaaac6), checked right after `time_word` at the four `REFUSE_CLOCK` sites. Tests `approx-ckks` and `approx-float`. | USER, S8 |
| 2026-11-08 | Q-S8-9 "(a) test/crypto/crypto.lang": the field oracle rows (add, sub, mul, inv on Fr and Fp) are in the new kit file `test/crypto/crypto.lang`, outside the Wasm set. The Word rows stay in `examples/crypto.lang`. Cause: the lowering refuses `frInv` with `EVAL_DEPTH` (more than 2000 levels). S8a rows use the same file. | USER, S8 |
| 2026-11-08 | Q-S8a-1 "(a) Measure first": build the guest `Pt1`, `ptDbl`, `ptMadd` and `ptMul` first, and measure a 16-bit and a 32-bit scalar multiply under the current limits before the route is asked (Q-S8a-4). No fuel or arena change. |
| 2026-11-08 | Q-S8a-2 "(a) Bake, Montgomery": `build/cryptotool poseidon` prints the 195 round constants and the 9 MDS values (quorum-lang 8d1a113, BN254 Fr) in Montgomery form, and `domain/domain.lang` holds them as data (`psRcsFr`, `psMdsFr`). One oracle row checks perm(0, 1, 2) against the quorum-lang known answer. |
| 2026-11-08 | Q-S8a-3 "(a) Domain fixtures": the domain file exports fixed test keys (`fxKey1` to `fxKey5`) and signed certificate entries by name, made by `build/cryptotool fixtures` from fixed secret keys. The examples and the `test/check` files use these names, because a guest program cannot name a Word literal. |
| 2026-11-08 | Q-S8a-4 "(a) Native field mul": a C builtin for the Fe CIOS multiply. The guest `feMul` stays the spec, and oracle rows compare the builtin with it. |
| 2026-11-08 | Q-S8a-5 "(a) Raise cap to 64": `ENTRY_PARAMS_MAX` is 64 (`src/front/check.h`), so the curve rows stay full-equality rows. No fuel or arena change. |
| 2026-11-08 | Q-S8a-6 "(a) Builtin feMulC, lower as guest feMul, fixed 64": `feMulC md a b` (`OP_FE_MUL`, `RULE_FE_MUL`, `VIS_WORD`: a guest program gets `REFUSE_WORD`). The C CIOS is in `src/front/eval.c`, and one call charges 64 eval steps. The lowering applies the domain def `feMul` to the arguments and lowers the result, so Wasm runs the spec (`test/ir/femul.lang` compares the two paths). |
| 2026-11-08 | Q-S8a-7 "(a) Native scalar mul": `ptMulC md k p` (`OP_PT_MUL`, `RULE_PT_MUL`, `VIS_WORD`) is a C builtin for `ptMul`. The guest `ptMul` stays the spec, oracle rows compare the two, and Wasm runs the spec. Asked with the s7 numbers: one `schnorrCheck` cost 6352241 steps and 519 MiB, so a `checkCert` of 2 or more members did not fit one eval. |
| 2026-11-08 | Q-S8a-8 "(a) Kernel wEq rule": `wEq x y` gives `inr unit` when x and y have the same form, also for open words (`word_same` in `src/front/eval.c`, a bounded structural test). `feEqRefl` proves `feEq a a` is `inr unit` for an open a, and `p3Slash` uses it, so P3 stays an open law on an Fe tag. The rule is in `src/front/eval.c`, not in `check.c`, because the check only builds the Core term. |
| 2026-11-08 | Q-S8a-9 "(a) Closed-only unfolding": the sealed defs `hsMsg` and `schnorrCheck` unfold only on closed arguments. On an open argument the application stays neutral (`OP_DEF_APP`), and conversion compares it by the def and the arguments. The eval rows do not change, and the proof field `eaOk` (Q-S7-4 (a)) stays. |
| 2026-11-09 | Q-S8a-10 "(a) Memo of closed sealed applications": the machine keeps a memo of each closed sealed application (the def and the arguments). Each distinct `hsMsg` or `schnorrCheck` value is computed one time in a langc run, and the list-bound walk and the eval rows use the memo. The memo-ok rule: a sealed def whose body reaches a `LIST_BOUNDS` family gets no memo, so a memo hit hides no list from the bound check. |
| 2026-11-09 | Q-S8b-1 "(a) C pairing builtin": the pairing check is the C builtin `pairCheckC` (`VIS_WORD`). The guest tower, the Miller loop and the final exponentiation are the spec, and Wasm runs the spec (OI-17). |
| 2026-11-09 | Q-S8b-2 "(a) Groth16": the verifier is Groth16 on BN254: an MSM on G1 with `ptMulC`, then one check of 4 pairs. |
| 2026-11-09 | Q-S8b-3 "(b) Real Noir proofs now": the golden proofs are real proofs of Noir circuits. |
| 2026-11-09 | Q-S8b-4 "(b) All three now": `elgVrf`, `outcomeVrf` and `shareVrf` are all built at S8b. |
| 2026-11-09 | Q-S8b-5 "(a) noir-groth16 + snarkjs": the proofs come from Noir with a Groth16 back end, and snarkjs. These are generation tools only (circomlibjs 0.1.7 too). The gate does not use them. |
| 2026-11-09 | Q-S8b-6 "(a) Golden files, offline gate": the golden files are in `test/noir/`, and the gate is offline. |
| 2026-11-09 | Q-S8b-7 "(b) Real eligibility now": the eligibility circuit is the real Alg 2 relation. The outcome and share circuits are stubs. |
| 2026-11-09 | Q-S8b-8 "(a) On-curve + G2 subgroup": `pairCheckC` and the spec check that each point is on its curve and that each G2 point is in the subgroup. |
| 2026-11-09 | Q-S8b-9 "(a) USER runs script": the USER runs the generation script once, outside the gate. The script and its tools are not in the repo. |
| 2026-11-09 | Q-S8b-10 "(a) hez ptau 14 + fixed p2": the setup is the Hermez powers of tau file of power 14 and one phase-2 contribution with fixed entropy. The keys are for tests only (OI-39). |
| 2026-11-09 | Q-S8b-11 "(a) Tagged hsMsg, depth 8": the circuit hash is hsMsg(tag, x, y) = poseidon2(tag, poseidon2(x, y)), and the Merkle depth is 8. |
| 2026-11-09 | Q-S8b-12 "(a) Fixture constants": the verification keys are domain data defs (`build/cryptotool noir` converts `verification_key.json`). The stub publics are (auction, winner handle, price) for the outcome and (auction, share index, share commitment) for a share. |
| 2026-11-09 | Q-S8b-13 "(c) Private witness (paper)": AucID is a private witness of the eligibility circuit. The statement is (root, handle, commitment), and the `elgVrf` signature does not change. OI-37 records the result. |
| 2026-11-09 | Q-S8b-14 "(a) Fifth tag": the secret commitment is C_u = hsMsg(7, rho_u, 0). The tags are 3 leaf, 4 node, 5 handle, 6 commitment and 7 secret (0, 1 and 2 are the message tags). |
| 2026-11-10 | Q-S8b-15 (a): `check_program` gives each declaration a fresh fuel, and a law field keeps the kept failure codes: `EVAL_FUEL` does not become a law refusal. |
| 2026-11-10 | Q-S8b-16 (b): cut the check-time evaluation of the spec defs: each Fp2 word takes its modulus through `mdGate2`. |
| 2026-11-10 | Q-S8b-17 (a): a second golden eligibility proof (`test/noir/elig2`): the same deposit and handle, a second bid, so a second commitment. The examples use the golden statements, and the slash example still gives `slashed`. |
| 2026-11-10 | Q-S8b-18 (a): there is no `amToFe` word. `outcomeVrf` reads the price by an inline elim and is the fifth `Amount` reader. |
| 2026-11-10 | Q-S8b-19 (a): `EVAL_FUEL_STEPS` goes from 20000000 to 40000000: with the golden statements, `replayAll` in `examples/log.lang` replays an amend, a slash and an admit. |
| 2026-11-10 | Q-S8b-20 (a): `test/gate.sh` runs `build/cryptotool noir` on the 4 golden dirs and prints `noir: 4 golden proofs passed`. |
| 2026-11-10 | Q-S8c-2 "Words only (Recommended)": the ChaCha20 key is the seed as 8 Words, the nonce is (stream tag, 0, 0), the counter is the block index, and the output is the 16 Words of the block (`sdExpand`). There is no field sampler. |
| 2026-11-10 | Q-S8c-3 "Domain Seed, kit rows (Recommended)": `Seed` is a domain family and the ChaCha20 rows are kit rows (`test/crypto/chacha.lang`). The checker, the lowering and the Wasm entry do not change. |
| 2026-11-10 | Q-S8c-4 "n-of-n attestations (Recommended)": the MPC output check is `outcomeAtt`. Each member signs the hash of (tag, AucID, winner handle, price), and the check needs the signatures of all n members. design/DESIGN.md line 38 says "t attestations": for the MPC output this ruling makes it n (OI-42). |
| 2026-11-10 | Q-S8c-5 "One commit (Recommended)": S8c is one commit. Q-S8c-1 was not asked: ChaCha20 is a guest def, and there is no C builtin for it. |

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
  The entry sets a Flag argument above 1 to 1 (`flag_norm` in
  `src/wasm.c`). A C tool `test/notraptool.c` checks each module (port
  from ratify-lang 71f2c22).
- The kit gate row "a trap is equal to a trap" changes: R0 makes a trap a
  failure on each target (permit-lang f9d8a15 docs/PLAN.md lines 188 to
  201). Closed at S3 (OI-4).

The crypto prelude (Q-S0-13 (b), Q-S1-3 (b)) is guest code, so R0 holds
for it. Each `Word` operation wraps and does not trap. There is no
division. Each loop is a fold on a static bound. In member mode
(Q-S1-2 (b)), a lost TCP link, a late message or a bad message is a
value, not a crash (Q-S1-9 (b), OI-18). The OS seed is an input of the
module, not an import (Q-S1-10 (a)).

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
| `Bids` | CM | The free monoid at `Amount`, `family Bids`: the bid list of one auction. The core `List` is kit only (Q-S4-2 (a)). | sec 4 |
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
`Reason`, `Outcome`, `Params`, `Amendment`, `Ballot`, `Log`. `Word`
(wrapping 32-bit) is a type of the crypto prelude only (Q-S1-7 (a)).

The role map of design sec 2 is the core reading (Q-S0-4): the deposit
root, the timestamp and the release each become a t-of-n certificate, and
slashing becomes a certified log entry.

Hash (Q-S0-8 (a)): an abstract `Hash` in the language (leaf-lang
precedent); Poseidon at T2 and in the crypto prelude (Q-S0-13 (b)). From
S5 (Q-S5-3 (a)): `family Hash := hash (hsTag : Size)` is the abstract
`Hash`, the DESIGN `Message` argument reads as `Hash`, and S6 keeps it.
`Party` is `party (ptKey : Pt1)`, a BN254 G1 key from S8a (Q-S5-1 (a), Q-S8-6 (a)).
`Members` is the committee list, and a `Committee` holds the members, t
and f.

### 4.2 Operations

The draft list is design sec 4: `checkCert`, `elgVrf`, `admit`, `slash`,
`release`, `outcome`, `authorize`, `amend`, `paramsAt`, `replay`. Each one
is total and gives a `Verdict`, a `Bool` or an `Outcome`. The S7 word for
`amend` is `amendCheck`: the DAO prelude (`src/front/front.c`) has a word
`amend`, and the domain name gave `REFUSE_NAME` (rule R4) in the regimes
and oneuse examples.

The crypto prelude (Q-S0-13 (b), Q-S1-3 (b), Q-S1-4 (b)) is gavel-lang
terms over `Word`: the BN254 field and curve, Poseidon, keccak-256, the
Schnorr certificate check, the BN254 pairing, the Noir verifier (for
`elgVrf`, the outcome proof and the TFHE share proof of Q-S1-8 (a)), the
MPC output check, the WRK rounds of one member (Q-S1-2 (b)) and ChaCha20
(Q-S1-10 (a)). `checkCert` and `elgVrf` call the prelude. There is no
crypto import.

At S8c the MPC output check is `outcomeAtt` (Q-S8c-4): the certificate
must have a signature of each of the n members on the outcome message of
the auction, the winner handle and the price. The seed input is the
family `Seed` (8 Words), and `sdExpand` gives one ChaCha20 block of 16
Words for a stream tag and a block index (Q-S8c-2).

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
| P2 | Simultaneous release: an `Opening` needs t certificates of `openMsg` at `prRelease` | DISCHARGED | the type of `release`: the proof field `opOk` of `Opening` (Q-S6-2 (a)) |
| P3 | No free bid withdrawal: a slash on two equal commitments is never valid | DISCHARGED | the path law `p3Slash` on `slash` (Q-S6-4 (a)); on the log, the path law `p3Replay` on `replay` (Q-S7-4 (a)) |
| P4 | Handle uniqueness for each deposit and auction | ASSUMED | collision resistance (paper Lemma 14) |
| P5 | Completeness: an honest eligible bid is admitted | ASSUMED | proof-system completeness (paper Lemma 12) |
| Z1 | Knowledge soundness of pi^m | ASSUMED | proof system; the paper states no such assumption |
| Z2 | Threshold decryption matches the ideal | ASSUMED | noise flooding proof |
| Z3 | dec(eval g cs) = g(dec cs) at T1 | ASSUMED | scheme correctness |
| G1 | Safety: t > f | DISCHARGED | the type of `Committee` (a proof field) |
| G2 | Censorship resistance: n - f >= t | DISCHARGED | the type of `Committee` (a proof field) |
| G3 | Fork freedom of the amend rule: 2ta - n > f | DISCHARGED | the type of `Params` (a proof field on ta) (ratify-lang O5) |
| G4 | `nil` is the unit, `append` is associative | DISCHARGED | the Log is a category: `lgNil` is a unit and `append` is associative, by the path defs `appendNilL`, `appendNilR` and `appendAssoc` (paths by induction, ratify-lang TCM-E; Q-S7-5 (a)) |
| G5 | Causality: an entry reads only earlier `Params` | DISCHARGED | by construction: the `enAmend` proof field reads only the earlier Params; `paramsAt` gives the Params at a height of the log (Q-S7-3 (a), Q-S7-5 (a), ratify-lang O10) |
| G6 | At most one entry for each height | EXTERNAL | G3 and honest single signing (ratify-lang O11) |
| A1 | First price: the winner bid is at least each bid; the price is the winner bid | DISCHARGED | path by induction on the list |
| A2 | Second price: the price is the larger of the reserve and the second highest eligible bid, at most the winner bid (Q-S4-5 (a)) | DISCHARGED | path by induction on the list |
| A3 | Under second price, a true bid is a dominant strategy | EXTERNAL | auction-lang theorem (name only) |
| A4 | Tie-break: equal top bids give the lowest index | DISCHARGED | definition of `outcome` |
| F1 | FHE exactness for `outcome` | AT TARGET (T3) | a static depth bound for each circuit |
| Z4 | The T2 relation is the guest relation | AT TARGET (T2) | golden files |
| M1 | MPC security and the corruption model | ASSUMED | the WRK proof (Wang, Ranellucci, Katz, CCS 2017): malicious, up to n - 1 corrupt members, abort possible (Q-S1-1 (b)) |
| M2 | Private, authenticated channels between members | EXTERNAL | the TLS proxy of each member (Q-S1-9 (b)) |
| M3 | The OS seed is uniform and fresh for each run | ASSUMED | getentropy (Q-S1-10 (a)); the guest takes the seed as a `Seed` of 8 Words and expands it with `sdExpand` (Q-S8c-2); no host entry gives the seed before S14b and S14c (OI-41) |
| Z5 | Soundness of the TFHE share proof | ASSUMED | proof system (Q-S1-8 (a)) |
| K1 | Poseidon is collision resistant | ASSUMED | the S8a def of Poseidon (Q-S8-5 (a), Q-S8-8 (a)) |
| K2 | keccak-256 is a random oracle for the Schnorr challenge | ASSUMED | the S8a def of `schnorrCheck` (Q-S8-6 (a), Q-S8-8 (a)) |
| K3 | Schnorr is EUF-CMA under discrete log on BN254 G1 | ASSUMED | the S8a def of `schnorrCheck` (Q-S8-6 (a), Q-S8-8 (a)) |
| K4 | Groth16 on BN254 is knowledge sound | ASSUMED | the S8b def of `g16Verify` (Q-S8b-2 (a)) |
| K5 | The Groth16 setup of each circuit is honest | EXTERNAL | the setup of a deploy; the keys of `test/noir/` are for tests only (Q-S8b-10 (a), OI-39) |
| E1 | The Word and field ops equal the C oracle on the vectors | EXTERNAL | `crypto oracle: 581 rows, 0 mismatches` (`build/cryptotool`, `src/crypto` from quorum-lang 8d1a113; Q-S8-7 (a), Q-S8-9 (a)) |
| E2 | The guest pairing spec, the guest Groth16 verifier and the C check agree on the vectors and the golden proofs | EXTERNAL | the 16 rows of `test/crypto/pair-spec.lang`, the 18 rows of `test/crypto/g16.lang`, `noir: 4 golden proofs passed`, the EIP-197 known answers in the `build/cryptotool` self-check (Q-S8b-6 (a), Q-S8b-20 (a)) |

Count at S1: DISCHARGED 10 (P2, P3, G1, G2, G3, G4, G5, A1, A2, A4),
ASSUMED 9 (P1, P4, P5, Z1, Z2, Z3, Z5, M1, M3), AT TARGET 2 (F1, Z4),
EXTERNAL 3 (G6, A3, M2). Total 24.

Count at S8: DISCHARGED 10 (P2, P3, G1, G2, G3, G4, G5, A1, A2, A4),
ASSUMED 12 (P1, P4, P5, Z1, Z2, Z3, Z5, M1, M3, K1, K2, K3), AT TARGET 2
(F1, Z4), EXTERNAL 4 (G6, A3, M2, E1). Total 28.

Count at S8b: DISCHARGED 10 (no change), ASSUMED 13 (K4 is new), AT
TARGET 2 (F1, Z4), EXTERNAL 6 (K5 and E2 are new). Total 31.

## 7. Refusals

New check codes (Q-S0-28 (a), Q-S1-7 (a), Q-S6-9 (a)): thirteen. Each one gets a `fix`
hint at A2.

| Code | When | Source |
|---|---|---|
| `REFUSE_PARTIAL` | A partial kit word. | R0 |
| `REFUSE_BOUND` | A committee or a bid list larger than the target bound: a closed `Bids` value with more than 1024 bids (Q-S4-6 (a)), a closed `Members` value with more than 1024 members (Q-S5-7 (a)), a closed `SealedBids` or `Handles` value with more than 1024 entries (Q-S6-7 (a)), or a closed `Ballots` value with more than 1024 ballots (Q-S7-10 (a)). | R0 |
| `REFUSE_AMOUNT` | An `Amount` operation other than compare, select, max, second max. | R0, Q-S0-1 |
| `REFUSE_FORMER` | A type constructor that is not CM or CC. | R1 |
| `REFUSE_FIXPOINT` | General recursion (rule R3: a def uses itself or a later def), or `Params` that authorize themselves (an `eaOk` field of `enAmend` that does not check). The kit code `REFUSE_REC` is renamed. | R1, G5, Q-S7-6 (a) |
| `REFUSE_CLOCK` | A wall clock or a host tick: a name from `now`, `block`, `timestamp`, `time`, `clock`, `epoch`, `deadline`, `timeout` and `expires`. Time is the log height: use a `Height`. The kit code `REFUSE_TIME` is renamed. | section 5, Q-S7-7 (a) |
| `REFUSE_PEEK` | A read of a `Sealed` value by a guest def (an elim, a fold or a projection), or an `opOk` field that does not check. | P1, P2, Q-S6-2 (a), Q-S6-3 (a) |
| `REFUSE_SAFETY` | A G1 field does not check (t <= f or a repeated party). | G1, Q-S5-9 (a) |
| `REFUSE_CENSOR` | A G2 field does not check (n - f < t). | G2, Q-S5-9 (a) |
| `REFUSE_FORK` | A G3 field does not check (2ta - n <= f). | G3, Q-S5-9 (a) |
| `REFUSE_DEADLINE` | A `prOrder` field does not check (the stamp end is not before the release). | Q-S6-9 (a) |
| `REFUSE_APPROX` | A declaration or a name is an approximate word: `ckks`, `approx`, `approximate`, `float` or `double` (an exact name match). | T3, Q-S8-8 (a) |
| `REFUSE_WORD` | A guest program names the `Word` type, a `Word` op or a `Word` literal. Only `domain/domain.lang` and `langc --kit` may. | Q-S1-7 (a), Q-S8-3 (a) |

## 8. Route

### 8.1 Kernel

Port a Cubical TT kernel (Q-S0-14), then add R0 (section 2) and the
classes check of R1 (section 3). Survey first: SAME, CHANGED,
SOURCE-ONLY.

S8a adds four kernel parts for the crypto prelude (`src/front/core.h`,
`src/front/check.c`, `src/front/eval.c`, `src/front/lower.c`):

- Two C builtins, `feMulC md a b` (Q-S8a-6 (a)) and `ptMulC md k p`
  (Q-S8a-7 (a)). Each one is `VIS_WORD`: only `domain/domain.lang` and
  `langc --kit` can name it. The guest defs `feMul` and `ptMul` stay the
  spec, and oracle rows compare each builtin with its spec. The Wasm
  lowering applies the spec def to the arguments, so Wasm runs the spec.
- The open `wEq` rule (Q-S8a-8 (a)). `wEq x y` gives `inr unit` when x
  and y have the same form, also when they are open. The test is bounded
  (depth 64, 4096 nodes) and uses no fuel.
- The sealed defs (Q-S8a-9 (a)): `hsMsg` and `schnorrCheck`. An
  application of a sealed def to all its arguments unfolds only when each
  argument is closed. Otherwise it stays neutral (`OP_DEF_APP`), and
  conversion compares two such terms by the def and the arguments. Thus
  the check does not run Poseidon or the curve on open values (the
  `eaOk` binder in `entryVerdict`). A value with more than 65536 nodes
  counts as closed. At most 4 defs can be sealed (`SEAL_MAX`).
- The memo (Q-S8a-10 (a)). The machine keeps each closed sealed
  application and its value, at most 256 rows (`SEAL_MEMO_MAX`). A second
  application of the same def to the same argument words uses the value
  and no fuel. The memo-ok rule: before the list-bound walk, the check
  marks each sealed def, and a sealed def whose body reaches a
  `LIST_BOUNDS` family gets no memo. Thus a memo hit hides no list from
  the bound check (Q-S4-6 (a), Q-S5-7 (a), Q-S6-7 (a)).

A test fails when each part is off (S8a mutants, 9 of 9 killed): the
guest `schnorrCheck` (the 7 `scCheck` oracle rows), the C `sc_verify`
(the `build/cryptotool` self-check), `ptMulC` (the `ck` rows of
`test/crypto/crypto.lang`), the `wEq` rule (`test/check/weq-open.lang`
and the domain def `feEqRefl`), the seal (`entryVerdict` in the domain
check) and the memo (the 3 main examples in guest mode give
`EVAL_FUEL`).

S8b adds three kernel parts (`src/front/core.h`, `src/front/check.c`,
`src/front/eval.c`, `src/front/lower.c`):

- The C builtin `pairCheckC` (Q-S8b-1 (a), Q-S8b-8 (a)), `VIS_WORD` as
  the S8a builtins (the check row `test/check/pair-check-c.lang`). It
  gives yes when the product of the pairings of its points is 1. First it
  checks that each point is on its curve and that each G2 point is in the
  subgroup of order r. The guest def `pairCheck` (the tower Fp2, Fp6 and
  Fp12, the Miller loop and the final exponentiation) stays the spec. The
  Wasm lowering applies the spec, so Wasm runs the spec. No gate row runs
  the pairing in Wasm (OI-17, OI-38).
- The third sealed def, `g16Verify` (3 of `SEAL_MAX` 4). `pairCheck` has
  no seal: no guest def gives it open points, because `g16Verify` calls
  `pairCheckC`, and a C builtin does not unfold on an open argument.
- The fuel (Q-S8b-15 (a), Q-S8b-19 (a)). `check_program` gives each
  declaration a fresh `EVAL_FUEL_STEPS`, which is 40000000 (20000000
  before S8b). A law field that uses all the fuel gives `EVAL_FUEL`, not
  the law refusal (`test/check/fuel-law.lang`).

A test fails when each part is off (S8b mutants, 29 of 29 KILLED). At
the close, 10 mutants: the root and commitment publics of `elgVrf`
swapped, a wrong price elim in `outcomeVrf` and a wrong index in
`shareVrf` (the rows `vrfElig`, `vrfOutcome` and `vrfShare` of
`test/crypto/g16.lang`); two Frobenius constants swapped (2 of the 16
`test/crypto/pair-spec.lang` rows); the fuel back at 20000000 (the check
of `examples/log.lang`); no fresh fuel for each declaration (`slashValid`
in `examples/sealed.lang`); one golden proof for both bids
(`slashValid`); the C want of each g16 row set to yes (10 of the 18
rows); one digit of the fixture commitment (`admitValid`); one public of
a golden proof (the `noir` gate line). During the build, 19 mutants: the
law-field fuel rule; the EIP-197 and Fp12 rows of the C oracle (2); the
Frobenius and Miller value rows (2); the golden check (4 data and 3 code
mutants); the chain rows (4); the public below r check; the second
eligibility witness; the canonical coordinate check.

### 8.2 The five targets

- T1 (primary): one Wasm module for `admit`, `slash`, `release`,
  `outcome`, `authorize`, `amend`, `paramsAt` and `replay`. `gaveld` reads
  a log, the bids and the certificates, runs the module and prints the
  verdicts. The crypto is in the module: the guest prelude of section
  4.2 (Q-S0-13 (b), Q-S1-3 (b), Q-S1-4 (b)). `gaveld` has no crypto code
  and no crypto import. In member mode the module also runs the WRK
  rounds of one member (Q-S1-2 (b)). The Wasm engine of `gaveld` is its
  own C99 interpreter (Q-S0-15 (a)). Differential rows under node and
  wasmtime.
- T2: the eligibility relation of Alg 2 (statement (root_s, hu, cm);
  witnesses rho_u, A_u, the Merkle path, tx) and the auction-outcome proof
  (Q-S0-9 (a)) and the TFHE share relation (Q-S1-8 (a)), in Noir
  (Q-S0-10 (a)), with golden files (Z4).
- T3: the FHE compare circuit for winner and price over encrypted bids
  (Q-S0-11 (a)), with a static depth bound (F1). `REFUSE_APPROX` for CKKS.
  The module checks each decryption share by its Noir proof (Q-S1-4 (b),
  Q-S1-8 (a), Z5).
- T4: Bristol Fashion Boolean circuits for winner and price and for the
  amendment tally, for the WRK protocol (Q-S0-12 (b), Q-S1-1 (b)). The
  module runs the rounds of one member (Q-S1-2 (b)): base OT, OT
  extension, the authenticated preprocessing, the garbling and the
  evaluation (OI-15). M1 is ASSUMED.
- T5: the JSON report: the auction facts, the governance facts (the G1 to
  G3 margins), the law counts and the verdict for each log height.

### 8.3 Four C programs

design sec 7. `langc`, `langc mcp`, `langc serve` and `gaveld` are
different programs. Do not merge them. `langc mcp` and `langc serve` give
the same tools: `check`, `fmt`, `explain`, `list-base`, `show-law`.

`gaveld` has two modes. Replay mode reads a log, the bids and the
certificates and prints the verdicts. It holds no secret. Member mode
(Q-S1-2 (b)) runs the WRK rounds of one member. It opens TCP links to the
other members, with TLS by a proxy (Q-S1-9 (b), M2). It reads 32 bytes
from the OS (getentropy) for each run as the module seed (Q-S1-10 (a),
M3). The seed and the member state stay in memory for the run only.

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
URL, the network and the payTo address; give the test-network inputs of
the gate (Q-S1-5 (a)): the test facilitator URL, a funded base-sepolia
payer key and the test payTo address; deploy the TLS proxy of each member
(Q-S1-9 (b)); publish `server.json` to the MCP
Registry; list the endpoint in x402 discovery surfaces; run any AgentFund
campaign; make the repo public.

## 10. Tests

The dev-language rule (ruling 10-09) holds (Q-S0-25 (b)): unit and integration
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
- x402 on the test network (Q-S0-25 (b), Q-S1-5 (a), Q-S1-6 (a)): from
  S18 the gate runs a C client against a local `langc serve` that calls a
  public base-sepolia facilitator. The client signs the EIP-3009
  authorization (keccak-256 and secp256k1 in C99 under `test/`). The row
  verifies and settles $0.01 test USDC and logs the settle transaction
  hash. The gate fails when the network or an input is absent (OI-19).
- Prelude rows (Q-S1-3 (b)): each prelude operation against quorum-lang
  `src/crypto` as a C oracle on fixed vectors.
- WRK rows (Q-S1-2 (b)): n `gaveld` members in member mode on local TCP
  links. The outputs equal the clear `outcome`.

## 11. Port sources

| Source | What | Where |
|---|---|---|
| lang-template 5265a2a | the tcc-wasm kit | `hosts/tcc-wasm` |
| ratify-lang a46d5df, 71f2c22, 796f442 | a46d5df (S2, Q-S0-14 (a), Q-S2-1 (a)): the Cubical TT kernel, a port of permit-lang 3d47120 without R0, the affine layer and the domain words: `src/front/`, `src/main.c`, `test/fronttool.c`, `test/pathtool.c`, `test/review.sh`, `test/gate.sh`, `domain/domain.lang` and the kernel rows. 71f2c22 (S3): the R0 reading, `REFUSE_PARTIAL`, `src/wasmop.h`, the no-trap Wasm writer, `test/notraptool.c`, the trap row. 796f442: R1, the governance layer, the Wasm engine; not its host-import ABI (Q-S0-13 (b)) | docs/PLAN.md; seed 55310c5 docs/PLAN.md lines 43 to 128; S2 surveyed the kernel; S3 surveys the R0 delta |
| sealed-lang 30d4d5f | the eligibility relation, hu, cm, `slashUsr` on chain | SPEC.md |
| quorum-lang 8d1a113 | `src/crypto` (bn254, elgamal, bgw, dkg): the reference of the guest prelude and a C oracle in the gate (Q-S1-3 (b)) | src/crypto |
| Wang, Ranellucci, Katz (CCS 2017) | WRK authenticated garbling (Q-S1-1 (b)); no sibling source | the paper |
| permit-lang b7e1228 | `langc mcp`, `check --json`, SKILL.md, the token comparison | docs/PLAN.md |
| mandate-lang 33556e9 | the x402 route | docs/VALIDATION.md line 51, test/run/mandate-x402.script |
| auction-lang 0c599f1 | the auction laws (theorem names) | Lean sources |

## 12. Slices

One session for each slice: scope, gate, mutants, docs, close.

| Slice | Work | Status |
|---|---|---|
| S0 | Seed from tcc-wasm, DESIGN.md and the digest, this plan, README table, AGENTS.md; gate EXIT 0; seed commit; private repo | DONE |
| S1 | The USER rules section 14 and Q-S1-1 to Q-S1-10. Section 1 has the rulings. | DONE |
| S2 | Port the ratify-lang Cubical TT kernel (8.1, Q-S0-14 (a), Q-S2-1 (a) to Q-S2-4 (a)): ratify-lang a46d5df, name edits only, no R0. Gate: 29 examples, 17 parse refusals, 154 check refusals, 10 kit-only refusals, 1 time hint, 78 evals, 1 build refusal; the review, domain and path regressions pass. Mutants M1 (path ends), M2 (glue face), M3 (universe level): KILLED. | DONE |
| S3 | Port the ratify-lang 71f2c22 R0 delta (Q-S3-1 (a), Q-S3-2 (a)): R0 in the kernel and the Wasm writer, `REFUSE_PARTIAL`, `src/wasmop.h`, `test/notraptool.c`, the trap row (OI-4). `src/front/lower.c`, `src/wasm.c` and `src/ir.h` are back in the build, with the IR rows, the Wasm modules and the differential cases (OI-20). Gate: 30 examples, 17 parse refusals, 158 check refusals, 10 kit-only refusals, 1 time hint, 79 evals, 1 build, 1 ir lowered, 5 ir refusals, 2 ir lowerings, 4 notrap modules, 7 notrap bad rows, 10 spine evals, 33 Wasm modules valid, 998 differential cases with 0 mismatches; the review, domain and path regressions pass. These are the 71f2c22 counts: the 71f2c22 `test/gate.sh` line 287 has `test/ir/proof-trap.lang` in the Wasm and differential rows (an earlier ratify-lang S3 run had 32 modules and 958 cases). Mutants M1, M2 (`REFUSE_PARTIAL`), M3 (the div guard of the no-trap tool), M4 (`flag_norm`): KILLED. M5 (the `br_table` default) and M6 (the IF condition): equivalent. | DONE |
| S4 | `Amount`, `Bids`, `Rule`, `Outcome`, `outcome` (Q-S4-1 (a) to Q-S4-7 (a)); A1, A2, A4 in `domain/domain.lang`; `REFUSE_AMOUNT`, `REFUSE_BOUND`; the kernel slot `DaoOutcome` (OI-23). Gate: 32 examples, 17 parse refusals, 163 check refusals, 10 kit-only refusals, 1 time hint, 83 evals, 1 build, 1 ir lowered, 5 ir refusals, 2 ir lowerings, 4 notrap modules, 7 notrap bad rows, 10 spine evals, 35 Wasm modules valid, 998 differential cases with 0 mismatches; the review, domain and path regressions pass. `test/gate.sh` builds a Wasm module for each `examples/*.lang` file, so the two new examples add two modules. Mutants M1 (the `REFUSE_AMOUNT` site), M2 (the `REFUSE_BOUND` site), M3 (`amMax`, A1), M4 (`bdSecond`, A2), M5 (the `bdArg` tie-break, A4): KILLED. | DONE |
| S5 | `Hash`, `Party`, `Members`, `Committee`, `Certificate`, `Params`, `checkCert`, `outcomeOf` and the Size and Sum words (Q-S5-1 (a) to Q-S5-11 (a)); G1 to G3 as proof fields (`cmSafe`, `cmCensor`, `prFork`); `REFUSE_SAFETY`, `REFUSE_CENSOR`, `REFUSE_FORK` (Q-S5-9 (a)); the committee half of `REFUSE_BOUND` (`COMMITTEE_BOUND` 1024). Gate: 34 examples, 17 parse refusals, 169 check refusals, 10 kit-only refusals, 1 time hint, 89 evals, 1 build, 1 ir lowered, 5 ir refusals, 2 ir lowerings, 4 notrap modules, 7 notrap bad rows, 10 spine evals, 37 Wasm modules valid, 998 differential cases with 0 mismatches; the review, domain and path regressions pass. Changes against S4: examples 32 to 34 (`examples/committee.lang`, `examples/committee-bound-edge.lang`), check refusals 163 to 169 (6 rows), evals 83 to 89 (6 committee rows), Wasm modules 35 to 37 (the 2 new examples); all other counts are the same. Mutants M1 (the `REFUSE_SAFETY` mapping), M2 (the `REFUSE_CENSOR` mapping), M3 (the `REFUSE_FORK` mapping), M4 (`COMMITTEE_BOUND` 1025), M5 (G1 `f <= t`), M6 (G2 with f dropped), M7 (G3 with `ta` for `ta + ta`): M1 to M7 KILLED (7/7). | DONE |
| S6 | `Slot`, `Height`, `AucID`, `Root`, `Handle`, `Commitment`, `Deposit`, `EligibilityProof`, `Sealed`, `SealedBid`, `Reason`, `Verdict`, `elgVrf`, `admit`, `slash`, `Opening`, `release`; the deadline fields of `Params` (`prSlot`, `prStampEnd`, `prRelease`, `prOrder`); P2, P3; `REFUSE_PEEK`, `REFUSE_DEADLINE`; the sealed and handle bounds (Q-S6-1 (b) to Q-S6-9 (a)). Gate: 36 examples, 17 parse refusals, 177 check refusals, 10 kit-only refusals, 1 time hint, 104 evals, 1 build, 1 ir lowered, 5 ir refusals, 2 ir lowerings, 4 notrap modules, 7 notrap bad rows, 10 spine evals, 39 Wasm modules valid, 998 differential cases with 0 mismatches; the review, domain and path regressions pass. Changes against S5: examples 34 to 36 (`examples/sealed.lang`, `examples/sealed-bound-edge.lang`), check refusals 169 to 177 (8 rows), evals 89 to 104 (15 sealed rows), Wasm modules 37 to 39; all other counts are the same. Mutants M1 (the `REFUSE_PEEK` site), M2 (the `prOrder` code), M3 (the `opOk` code), M4 (`openMsg` at the stamp end), M5 (`slash` without the equal-commitment test), M6 (`htLe` for `htLt`), M7 (`SEALED_BID_BOUND` 1025), M8 (`HANDLE_BOUND` 1025): M1 to M8 KILLED (8/8). | DONE |
| S7 | `Ballot`, `Ballots`, `Amendment`, `authorize`, `amendCheck`, `Entry`, `Log` (CM), `append`, `paramsAt`, `Machine` (CC), `Verdicts`, `replay`, `slashedOf` (Q-S7-1 (a) to Q-S7-10 (a)); G4 (`appendNilL`, `appendNilR`, `appendAssoc`), G5, P3 on the log (`p3Replay`); `REFUSE_REC` renamed `REFUSE_FIXPOINT`, `REFUSE_TIME` renamed `REFUSE_CLOCK`; the ballot half of `REFUSE_BOUND` (`BALLOT_BOUND` 1024); a linear `szLe`. The S7 word is `amendCheck`, not `amend`: the DAO prelude word `amend` (`src/front/front.c`) gave `REFUSE_NAME` (rule R4) in the regimes and oneuse examples. Gate: 37 examples, 17 parse refusals, 182 check refusals, 10 kit-only refusals, 1 time hint, 115 evals, 1 build, 1 ir lowered, 5 ir refusals, 2 ir lowerings, 4 notrap modules, 7 notrap bad rows, 10 spine evals, 40 Wasm modules valid, 998 differential cases with 0 mismatches; the review, domain and path regressions pass. Changes against S6: examples 36 to 37 (`examples/log.lang`), check refusals 177 to 182 (`amend-self`, `bound-ballots`, `time-deadline`, `time-expires`, `time-timeout`), evals 104 to 115 (11 log rows), Wasm modules 39 to 40; all other counts are the same. Mutants M1 (the `REFUSE_FIXPOINT` site), M2 (the `eaOk` code), M3 (the `deadline` time word), M4 (`append`, cons case: a domain load failure), M5 (`paramsAt` at height 0), M6 (`entryVerdict` slash case), M7 (`entrySlashed`), M8 (`BALLOT_BOUND` 1025): M1 to M8 KILLED (8/8). | DONE |
| S8 | `Word` and its native ops, `REFUSE_WORD`, the `SwapWord` rename, the Fr and Fp field, the C oracle, `REFUSE_APPROX`, the ASSUMED rows (Q-S8-1 (a) to Q-S8-9 (a)). Four units: u1 the `SwapWord` rename and the machine `Word` (CORE_WORD, VAL_WORD, ops, literals, `REFUSE_WORD`, the Wasm i32 lowering, `examples/crypto.lang` with 15 Word defs); u2 the field (`Fe` of 8 Words, `FeMod`, `frMod`, `fpMod`, `feAdd`, `feSub`, `feMul`, `feToMont`, `feFromMont`, `fePow`, `feInv`, `Fr`, `Fp`); u3 the C oracle (`src/crypto` from quorum-lang 8d1a113, `test/cryptotool.c`, `test/crypto/crypto.lang`, 424 rows); u4 `REFUSE_APPROX` (the list from ratify-lang 2aaaac6, four sites). Cost: `feMul` 9182 eval steps; the `frInv` eval row 3713353 steps and 303.4 MiB, under `EVAL_FUEL_STEPS` (20M at S8, 40M from S8b) and the 1 GiB arena. Gate: 38 examples, 18 parse refusals, 186 check refusals, 10 kit-only refusals, 1 time hint, 150 evals, 424 crypto oracle rows with 0 mismatches, 41 Wasm modules valid, 1846 differential cases with 0 mismatches; the other counts are the S7 counts. Changes against S7: examples 37 to 38, parse refusals 17 to 18, check refusals 182 to 186, evals 115 to 150, the new crypto oracle line, Wasm modules 40 to 41, differential cases 998 to 1846. Two old rows named a def `double` and now name it `twice` (`test/check/amount-elim.lang`, `test/fronttool.c`). Mutants M1 (the `REFUSE_WORD` site), M2 (the `APPROX_WORDS` list), M3 (the `frMod` n0 constant), M4 (`w_mulhi`), M5 (the `LEX_WORD_RANGE` test): M1 to M5 KILLED (5/5). | DONE |
| S8a | Poseidon circomlib t=3, keccak-256, `Pt1` with a Jacobian scalar multiply, `schnorrCheck`, `party (ptKey : Pt1)`, the certificate entry (ctR, ctS), the message words poseidon2(tag, poseidon2(x, y)), oracle rows in `test/crypto/crypto.lang`, `schnorr.c` from ratify-lang (closes OI-25 and OI-30). Built (Q-S8a-1 (a) to Q-S8a-10 (a)): the builtins `feMulC` and `ptMulC`; keccak-256 (`keccakF`, `kcChallenge`); Poseidon (`poseidon2`, the constants as data); `Pt1` and `ptMul`; `schnorrCheck` (the C check `sc_verify` is in `test/cryptotool.c`; there is no `schnorr.c` copy); `szToFe`, `hsMsg`, `hsOf`; the fixtures; the switch of `Hash` (Fe tag), `Party` (Pt1 key) and `Certificate` (signed entries); `szPair` and `szTri` out; the open `wEq` rule, the sealed defs and the memo. Gate: 501 oracle rows with 0 mismatches, 190 check refusals, 152 evals, 42 Wasm modules, 1910 differential cases, about 400 to 650 s. Mutants: 9 of 9 KILLED. | DONE |
| S8b | The crypto prelude part 2 (Q-S1-4 (b)): the BN254 pairing and the Noir verifier (`elgVrf`, the outcome proof, the TFHE share proof); the run time (OI-17). Built (Q-S8b-1 (a) to Q-S8b-20 (a)): the C builtin `pairCheckC` with the on-curve and G2 subgroup checks; the guest spec (the tower Fp2, Fp6 and Fp12, the Miller loop, the final exponentiation, `pairCheck`; `mdGate2` keeps the spec stuck at check time); `g16Verify` (sealed; the MSM with `ptMulC`, then one check of 4 pairs) with the families `G16Vk`, `G16Pubs` and `G16Ics`; the keys `eligVk`, `outcomeVk` and `shareVk` as domain data; `EligibilityProof := elgProof (epA : Pt1) (epB : Pt2) (epC : Pt1)` and `elgVrf` on Groth16 (closes OI-28); `OutcomeProof` with `outcomeVrf`, and `ShareProof` with `shareVrf`; the golden files in `test/noir/` (the Alg 2 eligibility circuit with 2 proofs, stub outcome and share circuits; the generation tools are not in the gate); `build/cryptotool noir NAME DIR` (the C Groth16 check and the key converter); the C pairing oracle with the EIP-197 known answers of revm-precompile 32.1.0; a fresh fuel for each declaration and `EVAL_FUEL_STEPS` 40M; the examples use the golden statements. There is no `amToFe` word: `outcomeVrf` is the fifth `Amount` reader. Gate: 537 oracle rows with 0 mismatches, `noir: 4 golden proofs passed`, 192 check refusals, 152 evals, 42 Wasm modules, 1910 differential cases, about 420 to 630 s of CPU time for one run. Mutants: 29 of 29 KILLED. New open items: OI-37, OI-38, OI-39. | DONE |
| S8c | The crypto prelude part 3: ChaCha20 and the seed input (Q-S1-10 (a)), the MPC output check. Built (Q-S8c-2 to Q-S8c-5): ChaCha20 of RFC 8439 as guest defs over `Word` (`CcSt`, `ccQr`, `ccDbl`, `ccRounds`, `ccBlock`, `ccBlockOf`; no C builtin); the family `Seed` (`seed8`, 8 Words) and `sdExpand` (key = the seed, counter = the block index, nonce = (stream tag, 0, 0)); `ccGate` keeps a block stuck at check time while the first seed word is open; the C ChaCha20 oracle in `test/cryptotool.c` with the RFC 8439 known answers of the sections 2.1.1, 2.3.2 and 2.4.2, and 34 rows in `test/crypto/chacha.lang`; `outcomeAtt`, the n-of-n attestation check of the MPC output (each member signs the outcome message hsMsg (3, auction tag, hsMsg (4, winner handle, price)); it rejects noncanonical winner handles and is the sixth `Amount` reader); the fixture OuA and 10 rows in `test/crypto/att.lang`. The checker, the lowering and the Wasm entry do not change. Gate: 581 oracle rows with 0 mismatches; the other counts are the S8b counts. Mutants: 7 of 8 KILLED (M1 `ccRn10` with 9 steps, M2 a rotate of `ccQr`, M3 the argument order of `sdExpand`, M5 a rotate of the C oracle, A1 `mbLen` to `cmT`, A2 the price out of the message, A3 the tag 3 to 1); no gate row sees M4 (`ccGate` with `wEq w w`, OI-43). New open items: OI-40 to OI-45. | DONE (commit pending) |
| S9 | T1 part 1: the Wasm module with the prelude, the notrap rows. | NEXT |
| S10 | T1 part 2: `gaveld`, the Wasm engine (Q-S0-15), the demo D1 to D8. | |
| S11 | T5: the JSON report, `docs/REPORT-JSON.md`. | |
| S12 | T2: the ZK relation export (Q-S0-9 (a), Q-S0-10 (a)) and the TFHE share relation (Q-S1-8 (a)). | |
| S13 | T3: the FHE compare circuit, the depth bound (F1), the share checks (Q-S1-4 (b), OI-16). | |
| S14 | T4: the Bristol Fashion circuits for WRK (Q-S1-1 (b)). | |
| S14b | T4 part 2: the WRK party program in the module (Q-S1-2 (b)): base OT, OT extension, preprocessing, garbling, evaluation (OI-15). | |
| S14c | `gaveld` member mode: the TCP links (Q-S1-9 (b)), the OS seed, the WRK rows (OI-18). | |
| S15 | A1: canonical forms and `langc fmt`. | |
| S16 | A2: `langc check --json`. | |
| S17 | A4: `langc mcp`. | |
| S18 | `langc serve`: routes, `/mcp`, x402, the stub facilitator, the count log, the test-network row (Q-S0-25 (b), Q-S1-5 (a), Q-S1-6 (a)). | |
| S19 | A7 and B2: `server.json`, `openapi.yaml`, `llms.txt`, `funding.json`, `.github/FUNDING.yml`. | |
| S20 | A3, A5, B4: SKILL.md, G1 to G5, AGENTS.md and the README final. | |
| S21 | A6: the token comparison. | |
| S22 | Close: SPEC.md, probe/CAPABILITY.md, VALIDATION.md, B3 milestones, final gate. | |

Design notes from S6:

- DN-1 (the P1 site). The sealed read check records operations while
  elaborating the written type and body of the def. Quoted types and
  generated proof cores can unfold domain defs (`slOut`, `release`), and
  a use of a domain def is not a read by the program def. Scanning those
  normal forms made the S5 fronttool row `settle` (`slOut` under `Trunc1`)
  and a `refl` proof about `release` give `REFUSE_PEEK`. Regression rows
  also keep direct reads refused in annotations and under `Trunc1`.
- DN-2 (against Q-S6-8 (a)). The Cantor code stays. The arena overflow (an
  out-of-memory at 1 GiB in `admitLate` and `open-early`) came from the S5
  word `szEq`. Its inner elim step called `ih`, and the evaluator computes
  `ih` eagerly, so the time grows very fast: `szEq` on 14 passes, and on 44
  it fills the arena. `szEq` now reads y with two new linear words, `szIsZ`
  and `szDec`. `szEqRefl` still checks by `ih`. The same shape was in
  `szLe`. S7 made `szLe` linear (Q-S7-9 (a), OI-34 CLOSED).

Design notes from S7:

- DN-3 (the word `amendCheck`). The DAO prelude has a word `amend`
  (`src/front/front.c`). A domain word with the same name gave
  `REFUSE_NAME` (rule R4) in 11 eval rows of the regimes and oneuse
  examples. The S7 word is `amendCheck`.
- DN-4 (literal Amounts in `examples/log.lang`). A guest program cannot
  apply `amSucc` to a value that is not a literal: the check gives
  `REFUSE_AMOUNT`. The Params `setup2` and `setupOI` use literal Amounts.
- DN-5 (no inner `ih`). The S7 words never call `ih` in the step of an
  inner elim on a recursive family: the evaluator computes the inner `ih`
  at every level. They branch on `szIsZ` and call `ih` once, as `szEq`.

## 13. Open items

- OI-1 The x402 field names (the `accepts` entries, the header names, the
  version) are checked against the x402 spec at S18. This plan does not
  fix them.
- OI-2 The `server.json` schema version and the package type of the stdio
  `langc mcp` bundle are checked against the MCP Registry docs at S19.
- OI-3 The `funding.json` schema and its version are checked at S19.
- OI-4 CLOSED at S3. The kit row "a trap is equal to a trap" (section 2)
  is gone: in the Wasm and differential rows a `trap` line is a FAIL. The
  row `pick 2 4 9` gives 4 and does not trap: the entry sets a Flag
  argument above 1 to 1 (`flag_norm` in `src/wasm.c`). Mutant M4 kills it.
- OI-5 The paper text says the proof shows the deposit is active; the
  displayed relation has no activity term (digest, sec 5.3).
- OI-6 The paper text lists AucID as a witness; Alg 2 does not (digest).
  S8b makes AucID a private witness of the eligibility circuit (Q-S8b-13
  (c), OI-37).
- OI-7 The slashUsr signature has no slot argument; signCom passes one
  (digest, Alg 1).
- OI-8 The paper gives no leaf encoding, node hash, Poseidon parameters or
  field encoding of hu, cm and root (digest, sec 6). T2 must fix them.
  S8 fixes the field encoding (Q-S8-4 (a)): 8 Words, Montgomery inside,
  canonical at each boundary. Poseidon is ruled for S8a (Q-S8-5 (a)).
  S8b fixes the Merkle encoding of the golden eligibility circuit
  (Q-S8b-11 (a), Q-S8b-14 (a)): the hash is hsMsg(tag, x, y), the depth is
  8, and the tags are 3 leaf, 4 node, 5 handle, 6 commitment and 7 secret.
  T2 must use the same encoding.
- OI-9 Committee loss can freeze the system. Recovery needs a clock. Not a
  v1 feature (ratify-lang OI-1).
- OI-10 The outcome leaks the winner and the price. T5 states it.
- OI-11 The views hold only up to indistinguishability (P1, Z1 to Z3).
  They stay ASSUMED.
- OI-12 The facilitator URL, the network and the payTo address are USER
  inputs. `langc serve` does not start without them.
- OI-13 The TLS proxy and the deploy are USER steps.
- OI-14 WRK has no guaranteed output: one corrupt member can abort the T4
  run (Q-S1-1 (b)). An aborted auction is a loss of liveness, not of
  safety. T5 states it.
- OI-15 The base OT, the OT extension, the garbling hash and the PRF of
  the WRK port are fixed at S14b (a question then). No sibling source.
- OI-16 The TFHE parameters, the noise bound of a share and the threshold
  key generation are fixed at S13. The key generation is EXTERNAL.
- OI-17 MEASURED in part at S8b (Q-S8b-1 (a)). With the C builtin
  `pairCheckC`, one closed `g16Verify` row takes 0.2 to 0.6 s of CPU time
  under the C99 interpreter. The guest spec of the pairing does not fit
  the arena (OI-38), so its run time is not measured. S9 measures the Wasm
  run of the spec.
- OI-18 The reason for a lost link, a late message or a bad message in
  member mode is fixed at S14c.
- OI-19 From S18 each gate run needs the network and the USER test inputs
  (Q-S1-5 (a)). A sandboxed session needs the facilitator host on its
  network allowlist.
- OI-20 CLOSED at S3. The gate gap from S2 to S3 (Q-S2-2 (a)) is gone:
  `src/front/lower.c`, `src/wasm.c` and `src/ir.h` are back in the build,
  and the gate has the IR rows, 33 Wasm modules and 998 differential cases
  with 0 mismatches (the kit had 13 modules and 813 cases).
- OI-21 `Amount` is a unary family in the kernel (Q-S4-1 (a)). A program
  uses only `amLe`, `amSelect`, `amMax` and `amMin`, so T1 can lower
  `Amount` to a fixed-width word with only these four operations. The T1
  slice fixes the width and the input check.
- OI-22 An open bid list (an input) passes the check at any length; only a
  closed `Bids` value with more than 1024 bids gives `REFUSE_BOUND`
  (Q-S4-6 (a)). The T1 entry bounds the input list (S9).
- OI-23 The kernel signature slot `Outcome` is `DaoOutcome` from S4
  (Q-S4-3 (a)): in `src/front/check.c`, in the DAO prelude of
  `src/front/front.c` and in 23 `.lang` files. A port from ratify-lang or
  permit-lang must apply the same rename.
- OI-24 STATED at S7 (Q-S7-8 (a)). G3 sets no upper law on ta (Q-S5-11
  (a)). A ta above n - f stops the amendments by the honest members: a loss
  of liveness, not of safety. There is no law and no refusal. The row
  `setupOI` in `examples/log.lang` shows it: `authorize` on all n - f
  honest yes ballots gives `inl unit`.
- OI-25 CLOSED at S8a (Q-S8-6 (a)). The `Party` key is a BN254 G1 point
  `Pt1`, and each certificate entry holds a signature (`ctR`, `ctS`).
  `ctHas` runs `schnorrCheck` on the entries of the signer (Q-S5-4 (a),
  Q-S8-6 (a)).
- OI-26 An open committee (an input) passes the check at any size; only a
  closed `Members` value with more than 1024 members gives `REFUSE_BOUND`
  (Q-S5-7 (a)). The T1 entry bounds the open committee (S9).
- OI-27 CLOSED at S6. `Params` has the deadline fields `prSlot`,
  `prStampEnd` and `prRelease` and the proof field `prOrder`, and `release`
  takes an `Opening` with the proof field `opOk` (Q-S6-1 (b), Q-S6-2 (a)).
- OI-28 CLOSED at S8b (Q-S8b-3 (b), Q-S8b-4 (b)). `EligibilityProof` is
  `elgProof (epA : Pt1) (epB : Pt2) (epC : Pt1)`, a Groth16 proof, and
  `elgVrf` runs `g16Verify eligVk` on the root, the handle and the
  commitment. The ideal record of Q-S6-5 (a) is gone.
- OI-29 CLOSED at S7. `replay` takes `Handles`, the slashed set, from the
  Log (Q-S7-4 (a)). `admit` keeps its S6 signature, and `entryVerdict`
  calls it with the handles so far.
- OI-30 CLOSED at S8a (Q-S8-5 (a)). Each message is `hash (hsMsg t x
  y)`, and `hsMsg t x y` is poseidon2(t, poseidon2(x, y)) on Fr. `szPair`
  and `szTri` are out (Q-S6-8 (a)).
- OI-31 `Sealed` is the clear denotation of the FHE ciphertext and the MPC
  shares until S13 and S14 (Q-S6-3 (a)).
- OI-32 The root certificate of a slot is not yet an argument of `admit`
  (design/DESIGN.md line 33).
- OI-33 `slash` has a new `Root` argument (Q-S6-4 (a)). OI-7 stays open for
  the slot argument of `slashUsr`.
- OI-34 CLOSED at S7. `szLe` has the linear form of `szIsZ` and `szDec`
  (Q-S7-9 (a)). The S5 and S6 rows stay green.
- OI-35 A release is outside the Log (Q-S7-1 (a)). The Log has the
  amendments, the slashes and the bid claims. A later slice can add a
  release entry.
- OI-36 The indexed `Log` has no bound: a closed `Log` value of any length
  passes the check. `Verdicts` has no bound too. Only `Ballots` has one
  (Q-S7-10 (a)). The T1 entry bounds the log (S9).
- OI-37 AucID is a private witness of the eligibility circuit (Q-S8b-13
  (c)); the statement is the root, the handle and the commitment. Thus a
  deposit can get a second handle with a second AucID, and the P3 slash
  does not catch it.
- OI-38 A full guest pairing (`pairCheck`) does not fit the 1 GiB arena of
  the evaluator. The gate compares the spec with the C oracle on value
  rows and short chain rows (`test/crypto/pair-spec.lang`), and
  `g16Verify` calls `pairCheckC`. A Wasm run of the spec needs an arena
  reset or a larger limit (S9, OI-17).
- OI-39 The Groth16 keys in `domain/domain.lang` and `test/noir/` are for
  tests only (Q-S8b-10 (a)): the phase-2 entropy is fixed, so the setup
  gives no soundness against a person who knows it. The outcome and share
  circuits are stubs (Q-S8b-7 (b)). The generation script and its tools
  are not in the repo. A deploy needs the real circuits and the keys of a
  real setup.
- OI-40 `sdExpand` gives Words only (Q-S8c-2). There is no sampler of a
  field element or of a bounded number from the stream. A slice that
  needs one must add it.
- OI-41 No host entry gives the OS seed to the guest. A `Seed` is a closed
  test value until S14b and S14c (M3).
- OI-42 `outcomeAtt` needs a signature of each of the n members (Q-S8c-4).
  design/DESIGN.md line 38 says "t attestations", and that text is not
  changed. Thus one absent member stops the attestation of an MPC
  outcome.
- OI-43 No gate row sees the `ccGate` guard. With `wEq w w` in the place
  of `wEq w 0w`, each row value is the same and only the check cost
  changes (2774 check steps against 179066 for
  `test/crypto/chacha.lang`). The gate has no cost rule.
- OI-44 There is no attestation message for `noSale`. `outcomeAtt` takes
  an auction, a winner handle and a price, as `outcomeVrf` does.
- OI-45 The check of a closed term `feToMont frMod (hsMsg ...)` calculates
  the hash, and each row of a kit file checks the full file. The def
  `msgOuA` added 47% to the CPU time of each row of
  `test/crypto/crypto.lang`, so the outcome rows are in
  `test/crypto/att.lang`. A helper def with a binder did not remove the
  cost.

## 14. Questions for the USER (S0)

Each question has a recommendation (rec). S1 ruled each one; section 1
has the rulings. The rulings that are not (a) raised Q-S1-1 to Q-S1-10,
which S1 asked and ruled; section 1 has them too.

- Q-S0-1 RULED (a). Amount arithmetic. (a) rec: compare, select, max and second max
  only. (b) Bounded add and sub with a static bound obligation.
- Q-S0-2 RULED (a). Rejections. (a) rec: `Verdict = valid | invalid Reason` with the
  eight reasons of section 2. (b) `Bool` only, and the host prints the
  reason.
- Q-S0-3 RULED (a). R1 classes. (a) rec: the table of section 3 (`Log` CM, `Machine`
  CC, `List` CM, `Sealed` and `Share` the identity CM at base types,
  `Proof` and `Protocol` base sorts). (b) `Protocol` as a CM too.
- Q-S0-4 RULED (a). The role map. (a) rec: design sec 2. (b) A variant that the USER
  names.
- Q-S0-5 RULED (a). Base types. (a) rec: the list of section 4.1. (b) No `Slot`: one
  root for each auction.
- Q-S0-6 RULED (a). Governance. (a) rec: one `Params` record, t-of-n amendments, the
  amend rule can amend itself, order by log height. (b) The auction rule is
  fixed for each `AucID`.
- Q-S0-7 RULED (a). The price list. (a) rec: a deploy setting, not in `Params`. (b) In
  `Params`.
- Q-S0-8 RULED (a). Hash. (a) rec: abstract `Hash`; Poseidon at T2 and, by Q-S0-13 (b), in the crypto prelude, not in `gaveld`.
  (b) Poseidon fixed in the language.
- Q-S0-9 RULED (a). Proof of the outcome. (a) rec: a T2 proof of the auction outcome
  (the paper has an auction proof, Table 1). (b) t attestations of correct
  evaluation.
- Q-S0-10 RULED (a). T2 format. (a) rec: Noir (the USER ruled Noir for ratify-lang,
  Q-S0-13 (b), 2026-11-03). (b) arkworks R1CS for Groth16 on BN254 (the
  paper's system).
- Q-S0-11 RULED (a). T3 scheme. (a) rec: TFHE (integer compare is native). (b) BFV.
- Q-S0-12 RULED (b). T4. (a) rec: Bristol Fashion only; the MPC protocol and its
  corruption model are EXTERNAL (M1). (b) Pin one MPC protocol in this
  plan.
- Q-S0-13 RULED (b). Crypto location. (a) rec: C in `gaveld` behind a fixed
  host-import ABI, a port of quorum-lang `src/crypto` (ratify-lang Q-S1-1
  (a)). (b) In the module.
- Q-S0-14 RULED (a). Kernel source. (a) rec: the ratify-lang kernel (the permit-lang
  kernel without the affine layer, ratify-lang Q-S0-10 (a)). (b) The
  permit-lang kernel. (c) Extend the kit checker in place.
- Q-S0-15 RULED (a). Wasm engine in `gaveld`. (a) rec: the own C99 interpreter
  (ratify-lang Q-S0-11 (a)); node and wasmtime are oracles in the gate.
  (b) Vendor wasm3. (c) The wasmtime C API.
- Q-S0-16 RULED (a). `langc serve` routes. (a) rec: JSON POST routes for the five
  tools and MCP streamable HTTP at `/mcp`, each behind x402. (b) Only
  `/mcp`.
- Q-S0-17 RULED (a). Price in the 402 response. (a) rec: $0.01 for check, fmt,
  explain, list-base and show-law; $0.05 for build (T1 module); $0.20 for
  the T2 to T4 exports. (b) A flat $0.01. (c) A USER price list. The
  pasted range is $0.01 to $0.20.
- Q-S0-18 RULED (a). Network. (a) rec: USDC on Base mainnet; base-sepolia in tests.
  (b) A network that the USER names.
- Q-S0-19 RULED (a). Registry entry (`server.json`, name
  `io.github.MavenRain/gavel-lang`). (a) rec: a `packages` entry (a bundle
  of the stdio `langc mcp` from a GitHub release) and a `remotes` entry
  (streamable HTTP at the `langc serve` `/mcp` URL, after the USER
  deploys it). (b) `remotes` only. (c) `packages` only.
- Q-S0-20 RULED (a). OpenAPI. (a) rec: `openapi.yaml` 3.1; each route lists its 402
  response with the x402 body schema and the price. (b) 3.0.
- Q-S0-21 RULED (a). Funding files. (a) rec: `.github/FUNDING.yml` (`github:
  [MavenRain]`), `funding.json`, `llms.txt`; no
  `.well-known/ai-donate.json` (no published standard). (b) Ship
  `ai-donate.json` too.
- Q-S0-22 RULED (a). Measurement. (a) rec: `langc serve` writes a local count log;
  nothing leaves the host. (b) None.
- Q-S0-23 RULED (a). AgentFund and the human channels. (a) rec: this plan lists
  candidate milestones only; the USER runs any campaign. (b) None.
- Q-S0-24 RULED (a). Agent framing. (a) rec: a README section "What an agent gets" in
  ROI terms (fewer failed runs, cheaper verification). (b) No section.
- Q-S0-25 RULED (b). Tests. (a) rec: C and gavel-lang tests, a stub facilitator in C,
  the kit node differential stays. (b) Also a test against a public
  test-network facilitator (the network in the gate).
- Q-S0-26 RULED (a). Token comparison. (a) rec: the permit-lang ruling (count_tokens,
  equal outputs). (b) An offline tokenizer.
- Q-S0-27 RULED (a). Law counts. (a) rec: the four buckets of section 6;
  `laws_discharged` counts DISCHARGED rows only. (b) One `laws_total` for
  all rows.
- Q-S0-28 RULED (a). Refusal codes. (a) rec: the eleven codes of section 7. (b) Fewer
  codes: one `REFUSE_PARAMS` for G1 to G3.
