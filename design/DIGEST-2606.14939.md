# Digest of arXiv:2606.14939v1 (read 2026-10-20 by WebFetch of the arXiv HTML)

Not the paper text. A fetch of the text into this directory was DENIED at
the permission gate on 2026-10-19 and 2026-10-20; the WebFetch tool read
the HTML in two windows (0-100000, 100000-152834 characters). The USER may
drop a .txt, .md, .html or .pdf copy here; the kickoff used this digest.

Title: "Censorship-Resistant Sealed-Bid Auctions on Blockchains".
Authors: Orestis Alpos, Lioba Heimbach, Kartik Nayak, Sarisht Wadhwa.
cs.CR, 12 Jun 2026.

Sections: 1 Introduction; 2 Model and Notation; 3 Desired Properties
(Hiding, Simultaneous Release, No Free Bid Withdrawal, Auction
Participation Efficiency); 4 Background (FOCIL, Blockchain Protocol); 5
Protocol Description (5.1 Pre-Phase A: Auction Registration; 5.2
Pre-Phase B: Deposit; 5.3 Phase 1: Timestamping; 5.4 Phase 2:
Bid-submission; 5.5 Security Analysis); 6 Benchmarks; 7 Discussions; 8
Related Work; 9 Conclusion and Future Work; A Security Analysis (A.1
Intermediate results; A.2 Proofs for the Entire Construction).

## The eligibility proof pi^m (section 5.3, Algorithm 2)

- Relation (section 5.3 "Zero-Knowledge proof of membership", unnumbered
  display; Algorithm 2 elgPrv line 7): hu = Hash(rho_u || AucID) and
  there is C_u with C_u = Hash(rho_u), (A_u, C_u) in merkle(root_s) and
  cm = Hash(tx || C_u).
- Statement (public inputs): (root_s, hu, cm) (Algorithm 2 calls ZkPrv
  with that statement).
- Witnesses: rho_u, A_u, the Merkle path, tx (Algorithm 2). The section
  5.3 text also lists AucID as a witness; the algorithm does not. C_u is
  existentially quantified (computed in the circuit).
- root_s is public. elgVrf (Algorithm 2) recomputes it for slot s from
  the deposits Merkle tree in the contract.
- Section 5.3 says the proof shows the deposit is active; the displayed
  relation has no activity term.
- AucID is hidden inside hu; the indistinguishability proof treats it as
  hidden through pre-image resistance (Appendix A).

## Tree, leaves, hash, proof system

- Depth: the benchmarks vary the depth for 2^8, 2^16, 2^24 and 2^32
  bidders (section 6, Table 1). No leaf encoding or node hash is given.
- Leaves: the deposit records (A_u, C_u) (Algorithm 1 registerDeposit);
  the section 5.2 text says (C_u, D).
- Hash in the circuit: Poseidon (section 6: Poseidon "for all hash
  computations"). On chain: not specified; the paper uses a generic Hash
  and never names keccak.
- Proof system: Groth16 over BN254, Rust, arkworks (abstract, section 6).
  No circom.
- Table 1 (ms, prove / verify): eligibility 2^8 bidders 46.7 +- 3.3 /
  0.9 +- 0.0; 2^32 bidders 159.4 +- 43.2 / 1.0 +- 0.2; auction proof 12.9
  +- 1.6 / 0.9 +- 0.1.
- No constraint counts, no Poseidon parameters, no field encoding of hu,
  cm or root anywhere (sections 6, A).

## Definitions

- C_u <- Hash(rho_u) (Algorithm 1, offline deposit; section 5.2).
- hu = Hash(rho_u || AucID) (section 5.3; Algorithm 2 elgPrv).
- cm = Hash(tx || C_u) (section 5.3; Algorithm 2 elgPrv).

## slashUsr (Algorithm 1)

- slashUsr(hu, (cm, pi^m_u), (cm', pi^m_u')): elgVrf on both (hu, cm,
  pi^m_u) and (hu, cm', pi^m_u'); both must verify; adds hu to the
  slashed set. It does not identify the user ("the true identity of the
  user is still unknown").
- The timestamper's signCom passes a slot s with the evidence; the
  slashUsr signature has no slot argument.
- slash(A_u, hu, Pi) removes the deposit once the handle is used again.

## Appendix A

- Lemma 14 (uniqueness of handle per deposit) assumes "Hash() is a
  collision-resistant hash function". Lemma 12 covers completeness. No
  knowledge-soundness assumption is stated.
