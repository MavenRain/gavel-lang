// gavel-lang S8b: the TFHE share proof stub (Q-S8b-7 (b), Q-S8b-12 (a)).
// Public (auc_id, idx, share_cm): the auction, the share index, the share
// commitment. One constraint uses each public input. The real relation is
// T3 (Q-S1-8 (a)).
fn main(auc_id: pub Field, idx: pub Field, share_cm: pub Field, w: Field) {
    assert(w == auc_id + 2 * idx + 3 * share_cm);
}
