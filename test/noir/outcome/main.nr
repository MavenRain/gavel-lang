// gavel-lang S8b: the auction outcome proof stub (Q-S8b-7 (b), Q-S8b-12 (a)).
// Public (auc_id, winner, price): the auction, the winner handle, the price.
// One constraint uses each public input. The real relation is T3.
fn main(auc_id: pub Field, winner: pub Field, price: pub Field, w: Field) {
    assert(w == auc_id + 2 * winner + 3 * price);
}
