/* The C oracle rows (S8, Q-S8-7 (a), Q-S8-9 (a)). Each line is
   `FILE NAME ARGS => RESULT`. FILE is the path of a kit file without `.lang`:
   examples/crypto for the Word ops, test/crypto/crypto for the field ops on
   Fr and Fp. NAME is a def of FILE. ARGS and RESULT are Word literals
   (`0x%08xw`) or a Nat. A field row gives the inputs and then the value of
   the C oracle, each as 8 Words, least significant first. The def gives 1
   when the field op of the domain file gives the same value. test/gate.sh
   evaluates each line with `langc --kit eval`. The inputs come from a fixed
   seed and from known answers. The field values come from src/crypto/bn254.c
   (quorum-lang 8d1a113, canonical form). Exit 1 when the C oracle does not
   give a known answer. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "crypto/bn254.h"

/* The modulus less one and less two, the most significant digit first.
   Source: ratify-lang test/cryptotool.c ORDER_LESS_ONE and PRIME_LESS_TWO.
   field_rows checks each one against BN254_FR and BN254_FP. */
#define FR_LESS_ONE "30644e72e131a029b85045b68181585d2833e84879b9709143e1f593f0000000"
#define FR_LESS_TWO "30644e72e131a029b85045b68181585d2833e84879b9709143e1f593efffffff"
#define FP_LESS_ONE "30644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd46"
#define FP_LESS_TWO "30644e72e131a029b85045b68181585d97816a916871ca8d3c208c16d87cfd45"

/* The Word edge values and the shift counts of the edge rows. */
static const uint32_t EDGE[5] = {0x0u, 0x1u, 0x7fffffffu, 0x80000000u, 0xffffffffu};
static const uint32_t SHIFT[4] = {0u, 31u, 32u, 33u};

/* The state of the fixed-seed generator (splitmix64). */
typedef struct {
  uint64_t s;
} Rng;

static uint64_t next64(Rng *r) {
  uint64_t z;
  r->s += 0x9e3779b97f4a7c15ULL;
  z = r->s;
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
  z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
  return z ^ (z >> 31);
}

static uint32_t next32(Rng *r) {
  return (uint32_t)(next64(r) >> 32);
}

/* The Word ops of examples/crypto.lang. A shift count is mod 32 (the Wasm
   i32 rule). A unary op takes Y and does not use it. */
typedef uint32_t (*Op2)(uint32_t x, uint32_t y);

static uint32_t w_add(uint32_t x, uint32_t y) { return x + y; }
static uint32_t w_sub(uint32_t x, uint32_t y) { return x - y; }
static uint32_t w_mul(uint32_t x, uint32_t y) { return x * y; }
static uint32_t w_mulhi(uint32_t x, uint32_t y) { return (uint32_t)(((uint64_t)x * (uint64_t)y) >> 32); }
static uint32_t w_and(uint32_t x, uint32_t y) { return x & y; }
static uint32_t w_or(uint32_t x, uint32_t y) { return x | y; }
static uint32_t w_xor(uint32_t x, uint32_t y) { return x ^ y; }
static uint32_t w_shl(uint32_t x, uint32_t y) { return x << (y & 31u); }
static uint32_t w_shr(uint32_t x, uint32_t y) { return x >> (y & 31u); }

static uint32_t w_rotl(uint32_t x, uint32_t y) {
  uint32_t n = y & 31u;
  return n == 0u ? x : (x << n) | (x >> (32u - n));
}

static uint32_t w_rotr(uint32_t x, uint32_t y) {
  uint32_t n = y & 31u;
  return n == 0u ? x : (x >> n) | (x << (32u - n));
}

static uint32_t w_lt(uint32_t x, uint32_t y) { return x < y ? 1u : 0u; }
static uint32_t w_eq(uint32_t x, uint32_t y) { return x == y ? 1u : 0u; }

static uint32_t w_not(uint32_t x, uint32_t y) {
  (void)y;
  return ~x;
}

static uint32_t w_lit(uint32_t x, uint32_t y) {
  (void)y;
  return x ^ 0xdeadbeefu;
}

/* The argument grid of an op: two edge values, an edge value and a shift
   count, or one edge value. */
typedef enum { ARGS_TWO, ARGS_SHIFT, ARGS_ONE } Args;

/* The result of an op: a Word literal or a Nat. */
typedef enum { OUT_WORD, OUT_NAT } Out;

typedef struct {
  const char *name;
  Op2 op;
  Args args;
  Out out;
} WordOp;

static const WordOp WORD_OPS[] = {
    {"wadd", w_add, ARGS_TWO, OUT_WORD},     {"wsub", w_sub, ARGS_TWO, OUT_WORD},
    {"wmul", w_mul, ARGS_TWO, OUT_WORD},     {"wmulhi", w_mulhi, ARGS_TWO, OUT_WORD},
    {"wand", w_and, ARGS_TWO, OUT_WORD},     {"wor", w_or, ARGS_TWO, OUT_WORD},
    {"wxor", w_xor, ARGS_TWO, OUT_WORD},     {"wshl", w_shl, ARGS_SHIFT, OUT_WORD},
    {"wshr", w_shr, ARGS_SHIFT, OUT_WORD},   {"wrotl", w_rotl, ARGS_SHIFT, OUT_WORD},
    {"wrotr", w_rotr, ARGS_SHIFT, OUT_WORD}, {"wnot", w_not, ARGS_ONE, OUT_WORD},
    {"wlit", w_lit, ARGS_ONE, OUT_WORD},     {"wlt", w_lt, ARGS_TWO, OUT_NAT},
    {"weq", w_eq, ARGS_TWO, OUT_NAT},
};

static void put_word(uint32_t x) {
  printf(" 0x%08xw", (unsigned)x);
}

/* Print the row of the op W on X and Y. */
static void word_row(const WordOp *w, uint32_t x, uint32_t y) {
  uint32_t r = w->op(x, y);
  printf("examples/crypto %s", w->name);
  put_word(x);
  if (w->args != ARGS_ONE)
    put_word(y);
  switch (w->out) {
  case OUT_WORD:
    printf(" =>");
    put_word(r);
    printf("\n");
    break;
  case OUT_NAT:
    printf(" => %u\n", (unsigned)r);
    break;
  }
}

/* The edge rows of each op, then 3 rows from the generator. */
static void word_rows(Rng *rng) {
  size_t i;
  size_t j;
  size_t k;
  for (i = 0u; i < sizeof WORD_OPS / sizeof WORD_OPS[0]; i++) {
    const WordOp *w = &WORD_OPS[i];
    switch (w->args) {
    case ARGS_TWO:
      for (j = 0u; j < 5u; j++)
        for (k = 0u; k < 5u; k++)
          word_row(w, EDGE[j], EDGE[k]);
      break;
    case ARGS_SHIFT:
      for (j = 0u; j < 5u; j++)
        for (k = 0u; k < 4u; k++)
          word_row(w, EDGE[j], SHIFT[k]);
      break;
    case ARGS_ONE:
      for (j = 0u; j < 5u; j++)
        word_row(w, EDGE[j], 0u);
      break;
    }
    for (j = 0u; j < 3u; j++) {
      uint32_t x = next32(rng);
      uint32_t y = next32(rng);
      word_row(w, x, y);
    }
  }
}

/* The field element of the 64 hex digits HEX, the most significant digit
   first. */
static void fe_hex(Fe *out, const char *hex) {
  size_t i;
  memset(out, 0, sizeof *out);
  for (i = 0u; i < 64u; i++) {
    char c = hex[i];
    uint32_t d = c >= 'a' ? (uint32_t)(c - 'a' + 10) : (uint32_t)(c - '0');
    size_t limb = (63u - i) / 8u;
    out->w[limb] = (out->w[limb] << 4) | d;
  }
}

static int same(const Fe *a, const Fe *b) {
  return memcmp(a->w, b->w, sizeof a->w) == 0;
}

/* 1 when A is less than the modulus F. */
static int below(const Fe *a, const Modulus *f) {
  size_t i = BN_LIMBS;
  while (i > 0u) {
    i--;
    if (a->w[i] != f->w[i])
      return a->w[i] < f->w[i];
  }
  return 0;
}

/* A canonical element from the generator: 8 limbs, the top limb less than
   2^30, again while the value is F or more. */
static void fe_random(Fe *out, Rng *rng, const Modulus *f) {
  do {
    size_t i;
    for (i = 0u; i < BN_LIMBS; i++)
      out->w[i] = next32(rng);
    out->w[BN_LIMBS - 1u] &= 0x3fffffffu;
  } while (!below(out, f));
}

typedef enum { FIELD_ADD, FIELD_SUB, FIELD_MUL, FIELD_INV } FieldOp;

typedef struct {
  const char *prefix;
  const Modulus *mod;
} Field;

static void put_fe(const Fe *a) {
  size_t i;
  for (i = 0u; i < BN_LIMBS; i++)
    put_word(a->w[i]);
}

/* Print the row of OP on A and B (FIELD_INV does not use B). OUT gets the
   value of the C oracle. */
static void field_row(const Field *f, FieldOp op, const Fe *a, const Fe *b, Fe *out) {
  switch (op) {
  case FIELD_ADD:
    fe_add(out, a, b, f->mod);
    printf("test/crypto/crypto %sAdd", f->prefix);
    break;
  case FIELD_SUB:
    fe_sub(out, a, b, f->mod);
    printf("test/crypto/crypto %sSub", f->prefix);
    break;
  case FIELD_MUL:
    fe_mul(out, a, b, f->mod);
    printf("test/crypto/crypto %sMul", f->prefix);
    break;
  case FIELD_INV:
    fe_inv(out, a, f->mod);
    printf("test/crypto/crypto %sInv", f->prefix);
    break;
  }
  put_fe(a);
  if (op != FIELD_INV)
    put_fe(b);
  put_fe(out);
  printf(" => 1\n");
}

/* A known-answer row: BAD counts the rows where the C oracle does not give
   WANT. */
static void known(const Field *f, FieldOp op, const Fe *a, const Fe *b, const Fe *want, unsigned *bad) {
  Fe out;
  field_row(f, op, a, b, &out);
  *bad += (unsigned)!same(&out, want);
}

/* The rows of the field F. BAD counts the wrong known answers and the rows
   where the inverse op does not give the input back. */
static void field_rows(const Field *f, const char *less_one, const char *less_two, Rng *rng, unsigned *bad) {
  Fe zero;
  Fe one;
  Fe two;
  Fe m1;
  Fe m2;
  Fe a;
  Fe b;
  Fe out;
  Fe back;
  Fe inv;
  int k;
  fe_from_u64(&zero, 0u);
  fe_from_u64(&one, 1u);
  fe_from_u64(&two, 2u);
  fe_hex(&m1, less_one);
  fe_hex(&m2, less_two);
  back = m1;
  back.w[0] += 1u;
  *bad += (unsigned)(memcmp(back.w, f->mod->w, sizeof back.w) != 0);
  back = m2;
  back.w[0] += 2u;
  *bad += (unsigned)(memcmp(back.w, f->mod->w, sizeof back.w) != 0);
  known(f, FIELD_ADD, &zero, &zero, &zero, bad);
  known(f, FIELD_ADD, &zero, &one, &one, bad);
  known(f, FIELD_ADD, &m1, &one, &zero, bad);
  known(f, FIELD_ADD, &one, &m1, &zero, bad);
  known(f, FIELD_ADD, &m1, &m1, &m2, bad);
  known(f, FIELD_SUB, &zero, &zero, &zero, bad);
  known(f, FIELD_SUB, &one, &zero, &one, bad);
  known(f, FIELD_SUB, &zero, &one, &m1, bad);
  known(f, FIELD_SUB, &one, &one, &zero, bad);
  known(f, FIELD_SUB, &m1, &m1, &zero, bad);
  known(f, FIELD_MUL, &zero, &m1, &zero, bad);
  known(f, FIELD_MUL, &one, &m1, &m1, bad);
  known(f, FIELD_MUL, &two, &m1, &m2, bad);
  known(f, FIELD_MUL, &m1, &m1, &one, bad);
  known(f, FIELD_INV, &one, &zero, &one, bad);
  known(f, FIELD_INV, &m1, &zero, &m1, bad);
  field_row(f, FIELD_INV, &two, &zero, &out);
  fe_mul(&back, &two, &out, f->mod);
  *bad += (unsigned)!same(&back, &one);
  for (k = 0; k < 4; k++) {
    fe_random(&a, rng, f->mod);
    fe_random(&b, rng, f->mod);
    field_row(f, FIELD_ADD, &a, &b, &out);
    fe_sub(&back, &out, &b, f->mod);
    *bad += (unsigned)!same(&back, &a);
  }
  for (k = 0; k < 4; k++) {
    fe_random(&a, rng, f->mod);
    fe_random(&b, rng, f->mod);
    field_row(f, FIELD_SUB, &a, &b, &out);
    fe_add(&back, &out, &b, f->mod);
    *bad += (unsigned)!same(&back, &a);
  }
  for (k = 0; k < 4; k++) {
    fe_random(&a, rng, f->mod);
    fe_random(&b, rng, f->mod);
    field_row(f, FIELD_MUL, &a, &b, &out);
    fe_inv(&inv, &b, f->mod);
    fe_mul(&back, &out, &inv, f->mod);
    *bad += (unsigned)!same(&back, &a);
  }
  for (k = 0; k < 3; k++) {
    fe_random(&a, rng, f->mod);
    field_row(f, FIELD_INV, &a, &zero, &out);
    fe_mul(&back, &a, &out, f->mod);
    *bad += (unsigned)!same(&back, &one);
  }
}

int main(int argc, char **argv) {
  Rng rng = {0x5338676176656c31ULL};
  const Field fr = {"fr", &BN254_FR};
  const Field fp = {"fp", &BN254_FP};
  unsigned bad = 0u;
  (void)argv;
  if (argc != 1) {
    fputs("usage: cryptotool\n", stderr);
    return 2;
  }
  word_rows(&rng);
  field_rows(&fr, FR_LESS_ONE, FR_LESS_TWO, &rng, &bad);
  field_rows(&fp, FP_LESS_ONE, FP_LESS_TWO, &rng, &bad);
  if (bad != 0u) {
    fprintf(stderr, "cryptotool: %u rows of the C oracle are wrong\n", bad);
    return 1;
  }
  return 0;
}
