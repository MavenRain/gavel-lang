// gavel-lang S8b: the eligibility relation of Alg 2 (Q-S8b-7 (b)).
// Statement (root_s, hu, cm). Witnesses rho_u, auc_id (Q-S8b-13 (c)), a_u,
// the Merkle path with its index bits, tx. C_u is computed here.
// Every hash is hs_msg (t, x, y) = hash_2 ([t, hash_2 ([x, y])]), the domain
// hsMsg (Q-S8b-11 (a)). The tags 0, 1 and 2 are the domain message tags.
use poseidon::poseidon::bn254::hash_2;

global TAG_LEAF: Field = 3;
global TAG_NODE: Field = 4;
global TAG_HANDLE: Field = 5;
global TAG_COMMIT: Field = 6;
global TAG_SECRET: Field = 7;
global DEPTH: u32 = 8;

fn hs_msg(t: Field, x: Field, y: Field) -> Field {
    hash_2([t, hash_2([x, y])])
}

// bits[i] is the index bit at level i (level 0 is the leaf). Bit 0: the
// current node is the left child. Bit 1: the current node is the right child.
fn merkle_root(leaf: Field, path: [Field; DEPTH], bits: [Field; DEPTH]) -> Field {
    let mut cur = leaf;
    for i in 0..DEPTH {
        let b = bits[i];
        assert(b * (1 - b) == 0);
        let sib = path[i];
        let left = cur + b * (sib - cur);
        let right = sib + b * (cur - sib);
        cur = hs_msg(TAG_NODE, left, right);
    }
    cur
}

fn main(
    root_s: pub Field,
    hu: pub Field,
    cm: pub Field,
    rho_u: Field,
    auc_id: Field,
    a_u: Field,
    path: [Field; DEPTH],
    bits: [Field; DEPTH],
    tx: Field,
) {
    let c_u = hs_msg(TAG_SECRET, rho_u, 0);
    let leaf = hs_msg(TAG_LEAF, a_u, c_u);
    assert(merkle_root(leaf, path, bits) == root_s);
    assert(hs_msg(TAG_HANDLE, rho_u, auc_id) == hu);
    assert(hs_msg(TAG_COMMIT, tx, c_u) == cm);
}

// The domain KAT (test/cryptotool.c PS_KAT): circomlib Poseidon([1, 2]).
#[test]
fn test_poseidon_kat() {
    assert(hash_2([1, 2]) == 0x115cc0f5e7d690413df64c6b9662e9cf2a3617f2743245519e19607a4417189a);
}
