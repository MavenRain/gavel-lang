# Reads a `langc ir` dump and prints one `NAME [ARGS...]` line for each case of
# the differential grid. A Flag parameter takes 0 and 1. A Nat parameter takes
# small values, and, when the entry has no loop, also the values at the 2^32
# and 2^64 limits (the ADD_CARRY cases). An entry with a loop gets
# only small values, because a large count runs past the evaluator fuel.
# A Word parameter (S8) takes words at the 32-bit limits, in the eval form
# (31w, 0xffffffffw); test/run-wasm.mjs drops the w.
# An Amount parameter (S9a) takes small values only: `langc eval` builds the
# unary value of each argument.
BEGIN {
  small = "0 1 2 3 5 7"
  big = small " 4294967295 4294967296 4294967297 18446744073709551615"
  words = "0w 1w 7w 31w 32w 33w 0x80000000w 0xffffffffw"
  amounts = "0 1 2 5"
  n = 0
}
/^func / {
  n++
  name[n] = $2
  sig = $0
  sub(/^func [^ ]+ \(/, "", sig)
  sub(/\).*$/, "", sig)
  gsub(/,/, "", sig)
  types[n] = sig
  # A host-form entry (S9a) has `-> out N` in its head: each of its cases
  # gets the last word out:N. The gate drops that word for `langc eval`.
  suffix[n] = match($0, /-> out [0-9]+/) ? " out:" substr($0, RSTART + 7, RLENGTH - 7) : ""
  loop[n] = 0
  next
}
/^ +(repeat|while) / { loop[n] = 1 }
END {
  for (i = 1; i <= n; i++) {
    sfx = suffix[i]
    cases(name[i], types[i], loop[i] ? small : big)
  }
}
function cases(nm, ty, nats,    t, nt, k, c, nc, j, v, nv, m, out, nl) {
  nc = 1
  c[1] = nm
  nt = split(ty, t, " ")
  for (k = 1; k <= nt; k++) {
    nv = split(t[k] == "flag" ? "0 1" : t[k] == "word" ? words : t[k] == "amount" ? amounts : t[k] == "list" ? lists(++nl) : nats, v, " ")
    m = 0
    for (j = 1; j <= nc; j++) {
      for (i2 = 1; i2 <= nv; i2++) out[++m] = c[j] " " v[i2]
    }
    nc = m
    for (j = 1; j <= nc; j++) c[j] = out[j]
  }
  for (j = 1; j <= nc; j++) print c[j] sfx
}
# S9a (D41): the 5 values of the list J of a head, at the byte A of the host
# region: the empty list, three short lists (one with a tie) and the count
# 1025, which is above the bound (the status 1). test/run-wasm.mjs writes the
# words at A; the gate gives `langc eval` the count, then the elements.
function lists(j,    a) {
  a = "at:" (64 + 8200 * (j - 1)) ":"
  return a "0 " a "1,5 " a "2,5,2 " a "3,2,5,5 " a "1025"
}
