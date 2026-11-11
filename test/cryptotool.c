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
#include "crypto/keccak.h"
#include "crypto/poseidon.h"

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

/* The curve rows (S8a): ptDbl, ptMadd and ptMul of the domain file on BN254
   G1 against src/crypto/bn254.c (ec0_jac_double, ec0_jac_add_affine,
   ec0_mul). A row gives the inputs and then the affine value of the C
   oracle, in canonical form, each coordinate as 8 Words, least significant
   first. A Jacobian input is (x z^2, y z^3, z) for a point (x, y) and a z
   from the generator. */

/* A point k G for a k of 32 bytes from the generator, G = (1, 2). */
static void point_random(G1 *out, Rng *rng) {
  unsigned char be[32];
  U256 k;
  G1 g;
  size_t i;
  for (i = 0u; i < 32u; i++)
    be[i] = (unsigned char)next32(rng);
  u256_from_be(&k, be);
  fe_from_u64(&g.x, 1u);
  fe_from_u64(&g.y, 2u);
  ec0_mul(out, &g, &k, &BN254_FP);
}

/* The Jacobian form (x z^2, y z^3, z) of P. */
static void jac_of(EcJac *out, const G1 *p, const Fe *z) {
  Fe zz;
  fe_mul(&zz, z, z, &BN254_FP);
  fe_mul(&out->x, &p->x, &zz, &BN254_FP);
  fe_mul(&zz, &zz, z, &BN254_FP);
  fe_mul(&out->y, &p->y, &zz, &BN254_FP);
  out->z = *z;
}

static void put_jac(const EcJac *a) {
  put_fe(&a->x);
  put_fe(&a->y);
  put_fe(&a->z);
}

static void put_g1(const G1 *p) {
  put_fe(&p->x);
  put_fe(&p->y);
}

static int same_g1(const G1 *a, const G1 *b) {
  return same(&a->x, &b->x) && same(&a->y, &b->y);
}

/* The row of ptDbl on A. BAD counts the rows where the affine double
   (ec0_double) gives a different point. */
static void dbl_row(const EcJac *a, unsigned *bad) {
  EcJac d;
  G1 out;
  G1 pa;
  G1 want;
  ec0_jac_double(&d, a, &BN254_FP);
  ec0_jac_affine(&out, &d, &BN254_FP);
  ec0_jac_affine(&pa, a, &BN254_FP);
  ec0_double(&want, &pa, &BN254_FP);
  *bad += (unsigned)!same_g1(&out, &want);
  printf("test/crypto/crypto curveDbl");
  put_jac(a);
  put_g1(&out);
  printf(" => 1\n");
}

/* The row of ptMadd on A and B. BAD counts the rows where the affine add
   (ec0_add) gives a different point. */
static void madd_row(const EcJac *a, const G1 *b, unsigned *bad) {
  EcJac d;
  G1 out;
  G1 pa;
  G1 want;
  ec0_jac_add_affine(&d, a, b, &BN254_FP);
  ec0_jac_affine(&out, &d, &BN254_FP);
  ec0_jac_affine(&pa, a, &BN254_FP);
  ec0_add(&want, &pa, b, &BN254_FP);
  *bad += (unsigned)!same_g1(&out, &want);
  printf("test/crypto/crypto curveMadd");
  put_jac(a);
  put_g1(b);
  put_g1(&out);
  printf(" => 1\n");
}

/* The row of ptMul on the scalar N and the point P. BAD counts a 0 P that
   is not (0, 0) and a 1 P that is not P. */
static void mul_row(const G1 *p, uint64_t n, unsigned *bad) {
  U256 k;
  G1 out;
  G1 inf;
  unsigned i;
  memset(&inf, 0, sizeof inf);
  u256_from_u64(&k, n);
  ec0_mul(&out, p, &k, &BN254_FP);
  *bad += (unsigned)(n == 0u && !same_g1(&out, &inf));
  *bad += (unsigned)(n == 1u && !same_g1(&out, p));
  printf("test/crypto/crypto curveMul");
  put_word((uint32_t)n);
  put_word((uint32_t)(n >> 32));
  for (i = 2u; i < BN_LIMBS; i++)
    put_word(0u);
  put_g1(p);
  put_g1(&out);
  printf(" => 1\n");
}

/* 3 double rows (2 from the generator, Z = 0), 6 add rows (2 from the
   generator, b at infinity, a at infinity, a = b, a = -b) and 4 multiply
   rows (0, 1, a 16-bit and a 32-bit scalar). */
static void curve_rows(Rng *rng, unsigned *bad) {
  G1 p;
  G1 q;
  G1 inf;
  G1 neg;
  Fe z;
  EcJac a;
  EcJac zero;
  int k;
  memset(&inf, 0, sizeof inf);
  memset(&zero, 0, sizeof zero);
  for (k = 0; k < 2; k++) {
    point_random(&p, rng);
    fe_random(&z, rng, &BN254_FP);
    jac_of(&a, &p, &z);
    dbl_row(&a, bad);
  }
  dbl_row(&zero, bad);
  for (k = 0; k < 2; k++) {
    point_random(&p, rng);
    point_random(&q, rng);
    fe_random(&z, rng, &BN254_FP);
    jac_of(&a, &p, &z);
    madd_row(&a, &q, bad);
  }
  madd_row(&a, &inf, bad);
  madd_row(&zero, &q, bad);
  madd_row(&a, &p, bad);
  neg.x = p.x;
  fe_neg(&neg.y, &p.y, &BN254_FP);
  madd_row(&a, &neg, bad);
  point_random(&p, rng);
  mul_row(&p, 0u, bad);
  mul_row(&p, 1u, bad);
  mul_row(&p, (uint64_t)((next32(rng) & 0xffffu) | 0x8000u), bad);
  mul_row(&p, (uint64_t)(next32(rng) | 0x80000000u), bad);
}

/* The feMulC rows (S8a): the C builtin feMulC against the guest feMul on the
   same A and B. A row gives 1 when the two products are equal, so the C
   oracle is not used. Each limb of TOP is 0xffffffff: limb 7 of a round
   wraps (feReduce). */
static void mulc_row(const Field *f, const Fe *a, const Fe *b) {
  printf("test/crypto/crypto %sMulC", f->prefix);
  put_fe(a);
  put_fe(b);
  printf(" => 1\n");
}

static void mulc_rows(const Field *f, Rng *rng) {
  Fe zero;
  Fe one;
  Fe m1;
  Fe top;
  Fe a;
  Fe b;
  size_t i;
  memset(&zero, 0, sizeof zero);
  memset(&one, 0, sizeof one);
  memset(&top, 0, sizeof top);
  one.w[0] = 1u;
  for (i = 0u; i < BN_LIMBS; i++)
    top.w[i] = 0xffffffffu;
  fe_sub(&m1, &zero, &one, f->mod);
  fe_random(&a, rng, f->mod);
  fe_random(&b, rng, f->mod);
  mulc_row(f, &a, &b);
  fe_random(&a, rng, f->mod);
  mulc_row(f, &a, &a);
  mulc_row(f, &zero, &b);
  mulc_row(f, &m1, &m1);
  mulc_row(f, &top, &top);
  mulc_row(f, &top, &m1);
}

/* The keccak rows (S8a). A kcOne row gives one padded block of 136 bytes as
   34 Words (the 17 lanes, each the low Word and then the high Word, the bytes
   little-endian) and the digest as 8 Words, least significant first (the
   digest is a big-endian value). A kc160 row gives R.x, R.y, P.x, P.y and m
   (8 Words each) and e = keccak256 of the 160 big-endian bytes, mod r
   (scalar_reduce). The digests come from src/crypto/keccak.c. */
#define KC_EMPTY "c5d2460186f7233c927e7db2dcc703c0e500b653ca82273b7bfad8045d85a470"
#define KC_ABC "4e03657aea45a94fc7d47ba826c8d667c0d1e6e33a64a036ec44f58fa12d6c45"

static uint32_t le_word(const unsigned char *b) {
  return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static uint32_t be_word(const unsigned char *b) {
  return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | (uint32_t)b[3];
}

/* Print 32 big-endian bytes as 8 Words, least significant first. */
static void put_be(const unsigned char *b) {
  size_t i;
  for (i = 0u; i < 8u; i++)
    put_word(be_word(b + 28u - 4u * i));
}

static int digest_is(const unsigned char *d, const char *hex) {
  char s[65];
  size_t i;
  for (i = 0u; i < 32u; i++)
    snprintf(s + 2u * i, 3u, "%02x", (unsigned)d[i]);
  return strcmp(s, hex) == 0;
}

/* The row of the message MSG of N bytes, N less than 136. WANT is the known
   digest, or NULL. */
static void kc_one_row(const unsigned char *msg, size_t n, const char *want, unsigned *bad) {
  unsigned char blk[136];
  unsigned char d[32];
  size_t i;
  memset(blk, 0, sizeof blk);
  memcpy(blk, msg, n);
  blk[n] = (unsigned char)(blk[n] ^ 0x01u);
  blk[135] = (unsigned char)(blk[135] ^ 0x80u);
  keccak256(msg, n, d);
  if (want != NULL && !digest_is(d, want))
    (*bad)++;
  printf("test/crypto/crypto kcOne");
  for (i = 0u; i < 136u; i += 4u)
    put_word(le_word(blk + i));
  put_be(d);
  printf(" => 1\n");
}

static void kc160_row(const unsigned char *in) {
  unsigned char d[32];
  unsigned char eb[32];
  U256 x;
  U256 e;
  size_t i;
  keccak256(in, 160u, d);
  u256_from_be(&x, d);
  scalar_reduce(&e, &x);
  u256_to_be(eb, &e);
  printf("test/crypto/crypto kc160");
  for (i = 0u; i < 5u; i++)
    put_be(in + 32u * i);
  put_be(eb);
  printf(" => 1\n");
}

/* The known answers "" and "abc", 55 and 135 bytes (the pad bytes 0x01 and
   0x80 then share byte 135), then 4 inputs of 160 bytes. */
static void keccak_rows(Rng *rng, unsigned *bad) {
  unsigned char msg[135];
  unsigned char in[160];
  size_t i;
  for (i = 0u; i < sizeof msg; i++)
    msg[i] = (unsigned char)(i * 7u + 1u);
  kc_one_row(msg, 0u, KC_EMPTY, bad);
  kc_one_row((const unsigned char *)"abc", 3u, KC_ABC, bad);
  kc_one_row(msg, 55u, NULL, bad);
  kc_one_row(msg, 135u, NULL, bad);
  memset(in, 0, sizeof in);
  kc160_row(in);
  for (i = 0u; i < sizeof in; i++)
    in[i] = (unsigned char)next32(rng);
  kc160_row(in);
  for (i = 0u; i < sizeof in; i++)
    in[i] = (unsigned char)next32(rng);
  kc160_row(in);
  memset(in, 0xff, sizeof in);
  kc160_row(in);
}

/* The Poseidon data and rows (S8a). `cryptotool poseidon` prints the 195
   round constants ("rc I") and the 9 MDS values ("mds I J") of
   poseidon_params on BN254_FR in Montgomery form (x R mod r, R = 2^256), each
   as 8 Words, least significant first. The data psRcsFr and psMdsFr of
   domain/domain.lang is this print. A psPerm row gives 3 values and the 3
   values of poseidon_perm. A psHash row gives x, y and element 0 of the
   permutation of (0, x, y). The values of the rows are not in Montgomery
   form. */
#define PS_KAT "115cc0f5e7d690413df64c6b9662e9cf2a3617f2743245519e19607a4417189a"

/* OUT = A R mod r, with R mod r from 256 doublings of 1. */
static void ps_mont(Fe *out, const Fe *a) {
  Fe rm;
  Fe t;
  size_t i;
  memset(&rm, 0, sizeof rm);
  rm.w[0] = 1u;
  for (i = 0u; i < 256u; i++) {
    t = rm;
    fe_add(&rm, &t, &t, &BN254_FR);
  }
  fe_mul(out, a, &rm, &BN254_FR);
}

static int poseidon_data(void) {
  PoseidonParams p;
  Fe m;
  size_t i;
  size_t j;
  poseidon_params(&p, &BN254_FR);
  for (i = 0u; i < POSEIDON_CONSTANTS; i++) {
    ps_mont(&m, &p.rc[i]);
    printf("rc %u", (unsigned)i);
    put_fe(&m);
    printf("\n");
  }
  for (i = 0u; i < POSEIDON_WIDTH; i++)
    for (j = 0u; j < POSEIDON_WIDTH; j++) {
      ps_mont(&m, &p.mds[i][j]);
      printf("mds %u %u", (unsigned)i, (unsigned)j);
      put_fe(&m);
      printf("\n");
    }
  return 0;
}

/* 1 when A is the big-endian hex value HEX (64 digits). */
static int fe_is_hex(const Fe *a, const char *hex) {
  char s[65];
  size_t i;
  for (i = 0u; i < BN_LIMBS; i++)
    snprintf(s + 8u * i, 9u, "%08x", (unsigned)a->w[BN_LIMBS - 1u - i]);
  return strcmp(s, hex) == 0;
}

static void ps_perm_row(const PoseidonParams *p, const Fe in[POSEIDON_WIDTH], Fe out[POSEIDON_WIDTH]) {
  size_t i;
  for (i = 0u; i < POSEIDON_WIDTH; i++)
    out[i] = in[i];
  poseidon_perm(out, p);
  printf("test/crypto/crypto psPerm");
  for (i = 0u; i < POSEIDON_WIDTH; i++)
    put_fe(&in[i]);
  for (i = 0u; i < POSEIDON_WIDTH; i++)
    put_fe(&out[i]);
  printf(" => 1\n");
}

static void ps_hash_row(const PoseidonParams *p, const Fe *x, const Fe *y, Fe *h) {
  Fe s[POSEIDON_WIDTH];
  memset(&s[0], 0, sizeof s[0]);
  s[1] = *x;
  s[2] = *y;
  poseidon_perm(s, p);
  *h = s[0];
  printf("test/crypto/crypto psHash");
  put_fe(x);
  put_fe(y);
  put_fe(h);
  printf(" => 1\n");
}

/* The known answer perm(0, 1, 2) (element 0 = circomlib poseidon(1, 2)) as a
   psPerm and a psHash row, then psPerm on random values, on r - 1 and on 0,
   then psHash on random values. */
static void poseidon_rows(Rng *rng, unsigned *bad) {
  PoseidonParams p;
  Fe in[POSEIDON_WIDTH];
  Fe out[POSEIDON_WIDTH];
  Fe zero;
  Fe one;
  Fe h;
  size_t i;
  poseidon_params(&p, &BN254_FR);
  memset(in, 0, sizeof in);
  in[1].w[0] = 1u;
  in[2].w[0] = 2u;
  ps_perm_row(&p, in, out);
  if (!fe_is_hex(&out[0], PS_KAT)) {
    fputs("cryptotool: poseidon perm(0, 1, 2) is not the known answer\n", stderr);
    (*bad)++;
  }
  ps_hash_row(&p, &in[1], &in[2], &h);
  if (!fe_is_hex(&h, PS_KAT)) {
    fputs("cryptotool: poseidon hash(1, 2) is not the known answer\n", stderr);
    (*bad)++;
  }
  for (i = 0u; i < POSEIDON_WIDTH; i++)
    fe_random(&in[i], rng, &BN254_FR);
  ps_perm_row(&p, in, out);
  memset(&zero, 0, sizeof zero);
  memset(&one, 0, sizeof one);
  one.w[0] = 1u;
  for (i = 0u; i < POSEIDON_WIDTH; i++)
    fe_sub(&in[i], &zero, &one, &BN254_FR);
  ps_perm_row(&p, in, out);
  memset(in, 0, sizeof in);
  ps_perm_row(&p, in, out);
  fe_random(&in[1], rng, &BN254_FR);
  fe_random(&in[2], rng, &BN254_FR);
  ps_hash_row(&p, &in[1], &in[2], &h);
}

/* The Schnorr rows (S8a), as src/crypto/schnorr.c of ratify-lang: the key
   P = x G for a secret x, the nonce k = keccak256(x || m) mod r, R = k G,
   e = keccak256(R.x || R.y || P.x || P.y || m) mod r and s = k + e x mod r.
   Each value is 32 bytes, big-endian. A row scCheck gives P, R, s and m (not
   in Montgomery form), then 1 when the signature is valid, else 0. */
static void sc_be(unsigned char *out, const Fe *a) {
  size_t i;
  for (i = 0u; i < BN_LIMBS; i++) {
    uint32_t w = a->w[BN_LIMBS - 1u - i];
    out[4u * i] = (unsigned char)(w >> 24);
    out[4u * i + 1u] = (unsigned char)(w >> 16);
    out[4u * i + 2u] = (unsigned char)(w >> 8);
    out[4u * i + 3u] = (unsigned char)w;
  }
}
/* The digest of the N bytes at DATA, mod r. */
static void sc_hash(Fe *out, const unsigned char *data, size_t n) {
  unsigned char d[32];
  U256 u;
  U256 v;
  keccak256(data, n, d);
  u256_from_be(&u, d);
  scalar_reduce(&v, &u);
  (void)fe_from_u256(out, &v, &BN254_FR);
}
static void sc_gen(G1 *g) {
  memset(g, 0, sizeof *g);
  g->x.w[0] = 1u;
  g->y.w[0] = 2u;
}
static void sc_mul(G1 *out, const G1 *p, const Fe *k) {
  U256 u;
  fe_to_u256(&u, k);
  ec0_mul(out, p, &u, &BN254_FP);
}
static void sc_challenge(Fe *e, const G1 *pk, const G1 *r, const Fe *m) {
  unsigned char b[160];
  sc_be(b, &r->x);
  sc_be(b + 32u, &r->y);
  sc_be(b + 64u, &pk->x);
  sc_be(b + 96u, &pk->y);
  sc_be(b + 128u, m);
  sc_hash(e, b, sizeof b);
}
static void sc_sign(G1 *pk, G1 *r, Fe *s, uint64_t sk, const Fe *m) {
  unsigned char b[64];
  G1 g;
  Fe x;
  Fe k;
  Fe e;
  Fe t;
  sc_gen(&g);
  fe_from_u64(&x, sk);
  sc_mul(pk, &g, &x);
  sc_be(b, &x);
  sc_be(b + 32u, m);
  sc_hash(&k, b, sizeof b);
  sc_mul(r, &g, &k);
  sc_challenge(&e, pk, r, m);
  fe_mul(&t, &e, &x, &BN254_FR);
  fe_add(s, &k, &t, &BN254_FR);
}
/* 1 when P is a point of the curve (not the point at infinity) with each
   coordinate less than p. */
static int sc_valid(const G1 *p) {
  Fe zero;
  memset(&zero, 0, sizeof zero);
  return below(&p->x, &BN254_FP) && below(&p->y, &BN254_FP) && !(same(&p->x, &zero) && same(&p->y, &zero)) && g1_on_curve(p);
}
/* 1 when (R, s) is a valid signature on M by PK: PK and R valid, s less than
   r and s G = R + e P. */
static int sc_verify(const G1 *pk, const G1 *r, const Fe *s, const Fe *m) {
  G1 g;
  G1 lhs;
  G1 ep;
  G1 rhs;
  Fe e;
  if (!sc_valid(pk) || !sc_valid(r) || !below(s, &BN254_FR))
    return 0;
  sc_gen(&g);
  sc_challenge(&e, pk, r, m);
  sc_mul(&lhs, &g, s);
  sc_mul(&ep, pk, &e);
  ec0_add(&rhs, r, &ep, &BN254_FP);
  return same_g1(&lhs, &rhs);
}
static void sc_row(const G1 *pk, const G1 *r, const Fe *s, const Fe *m, int want, unsigned *bad) {
  if (sc_verify(pk, r, s, m) != want) {
    fputs("cryptotool: a schnorr row is not the C verdict\n", stderr);
    (*bad)++;
  }
  printf("test/crypto/crypto scCheck");
  put_g1(pk);
  put_g1(r);
  put_fe(s);
  put_fe(m);
  printf(" => %d\n", want);
}
/* 2 valid rows (sk 7 and sk 8, each on a random m), then 5 invalid rows from
   the sk 7 row: s + 1, m + 1, the sk 8 key, s + r (not reduced) and -R. */
static void schnorr_rows(Rng *rng, unsigned *bad) {
  G1 pk7;
  G1 r7;
  G1 pk8;
  G1 r8;
  G1 nr;
  Fe s7;
  Fe s8;
  Fe m7;
  Fe m8;
  Fe one;
  Fe t;
  uint64_t c = 0u;
  size_t i;
  fe_random(&m7, rng, &BN254_FR);
  fe_random(&m8, rng, &BN254_FR);
  sc_sign(&pk7, &r7, &s7, 7u, &m7);
  sc_sign(&pk8, &r8, &s8, 8u, &m8);
  sc_row(&pk7, &r7, &s7, &m7, 1, bad);
  sc_row(&pk8, &r8, &s8, &m8, 1, bad);
  memset(&one, 0, sizeof one);
  one.w[0] = 1u;
  fe_add(&t, &s7, &one, &BN254_FR);
  sc_row(&pk7, &r7, &t, &m7, 0, bad);
  fe_add(&t, &m7, &one, &BN254_FR);
  sc_row(&pk7, &r7, &s7, &t, 0, bad);
  sc_row(&pk8, &r7, &s7, &m7, 0, bad);
  for (i = 0u; i < BN_LIMBS; i++) {
    c += (uint64_t)s7.w[i] + (uint64_t)BN254_FR.w[i];
    t.w[i] = (uint32_t)c;
    c >>= 32;
  }
  sc_row(&pk7, &r7, &t, &m7, 0, bad);
  nr = r7;
  fe_neg(&nr.y, &r7.y, &BN254_FP);
  sc_row(&pk7, &nr, &s7, &m7, 0, bad);
}
/* The ptMulC rows (S8a, USER ruling Q-S8a-7 (a)): the C builtin ptMulC
   against the guest ptMul on the same K and P. A row gives 1 when the two
   points are equal, so the C oracle is not used. The scalars are 0, 1, a
   16-bit, a 32-bit and a 254-bit value (less than r), and 2^256 - 1 (not
   reduced). One 32-bit row takes the point at infinity. */
static void ptmulc_row(const Fe *k, const G1 *p) {
  printf("test/crypto/crypto curveMulC");
  put_fe(k);
  put_g1(p);
  printf(" => 1\n");
}

static void ptmulc_rows(Rng *rng) {
  G1 p;
  G1 inf;
  Fe k;
  size_t i;
  memset(&inf, 0, sizeof inf);
  memset(&k, 0, sizeof k);
  point_random(&p, rng);
  ptmulc_row(&k, &p);
  k.w[0] = 1u;
  ptmulc_row(&k, &p);
  k.w[0] = (next32(rng) & 0xffffu) | 0x8000u;
  ptmulc_row(&k, &p);
  k.w[0] = next32(rng) | 0x80000000u;
  ptmulc_row(&k, &p);
  ptmulc_row(&k, &inf);
  for (i = 0u; i < BN_LIMBS; i++)
    k.w[i] = next32(rng);
  k.w[BN_LIMBS - 1u] = (k.w[BN_LIMBS - 1u] & 0x0fffffffu) | 0x20000000u;
  ptmulc_row(&k, &p);
  for (i = 0u; i < BN_LIMBS; i++)
    k.w[i] = 0xffffffffu;
  ptmulc_row(&k, &p);
}

/* The BN254 pairing oracle (S8b 4c, USER ruling Q-S8b-1 (a)). This is a
   copy of the pairing text of src/front/eval.c (the C builtin pairCheckC):
   the Montgomery Fp words, the tower Fp2, Fp6 and Fp12, the Miller loop of
   the optimal ate pairing, the final exponentiation and the point checks.
   build/cryptotool does not link src/front, thus the text is copied. The
   constants are in Montgomery form (R = 2^256), except the *_RAW exponents.
   pairo_rows checks the copy: the tower laws, the final exponentiation, the
   bilinearity, the point checks and the EIP-197 known answers. A wrong check adds 1 to bad. */
static const uint32_t BN_P[8] = {0xd87cfd47u, 0x3c208c16u, 0x6871ca8du, 0x97816a91u, 0x8181585du, 0xb85045b6u, 0xe131a029u, 0x30644e72u};
static const uint32_t BN_N0 = 0xe4866389u;
static const uint32_t BN_ONE[8] = {0xc58f0d9du, 0xd35d438du, 0xf5c70b3du, 0x0a78eb28u, 0x7879462cu, 0x666ea36fu, 0x9a07df2fu, 0x0e0a77c1u};
static const uint32_t BN_THREE[8] = {0x50ad28d7u, 0x7a17caa9u, 0xe15521b9u, 0x1f6ac17au, 0x696bd284u, 0x334bea4eu, 0xce179d8eu, 0x2a1f6744u};
static const uint32_t BN_HALF[8] = {0x4f060572u, 0x87bee7d2u, 0x2f1c6ae5u, 0xd0fd2addu, 0xfcfd4f44u, 0x8f5f7492u, 0x3d9cbfacu, 0x1f37631au};
static const uint32_t BN_PM2_RAW[8] = {0xd87cfd45u, 0x3c208c16u, 0x6871ca8du, 0x97816a91u, 0x8181585du, 0xb85045b6u, 0xe131a029u, 0x30644e72u};
static const uint32_t BN_R_RAW[8] = {0xf0000001u, 0x43e1f593u, 0x79b97091u, 0x2833e848u, 0x8181585du, 0xb85045b6u, 0xe131a029u, 0x30644e72u};
static const uint32_t BN_ATE_RAW[8] = {0xbe763ba8u, 0x9d797039u, 0x00000001u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u};
#define BN_ATE_BITS 65u
static const uint32_t BN_X_RAW[8] = {0x4a6909f1u, 0x44e992b4u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u};
#define BN_X_BITS 63u
#define BN_H_BITS 761u
static const uint32_t BN_E12_RAW[88] = {0xca86f120u, 0x86964b64u, 0xe54523a4u, 0x40a4efb7u, 0x96e84abbu, 0x837fa978u, 0xb9b2b918u, 0x361102b6u, 0xf35692dau, 0xc0de81deu, 0xa6c3c760u, 0xbe04c7e8u, 0xd570bb7fu, 0xd766f9c9u, 0x83561841u, 0xc230974du, 0xc3be69a3u, 0x5bba1668u, 0x10526294u, 0x7f3811c4u, 0xdadda71cu, 0x29baee7du, 0x145da900u, 0xbf813b8du, 0x423f9a2cu, 0x641bbadfu, 0x44eacc5eu, 0xa80bb4eau, 0x14fde37cu, 0xcd656648u, 0x580291d2u, 0x4a0364b9u, 0x0826f0ddu, 0xee93dfb1u, 0xc5514724u, 0x6b42db8du, 0x0b0f3785u, 0xbb10cf43u, 0x6f804216u, 0x40494e40u, 0xacf3aafbu, 0x55cfe107u, 0xe0ebae87u, 0x2088ec80u, 0x11a337a0u, 0x846a3ed0u, 0x1e3a5195u, 0x48a45a4au, 0xdfc50e16u, 0xe5664568u, 0x4c0cc4ebu, 0xab6a4129u, 0xd268c7dau, 0x82d0d602u, 0xed3cc48au, 0x6668449au, 0xb2015dfcu, 0x5062cd0fu, 0xb1ddb3d1u, 0x7f2940a8u, 0x2a226448u, 0x77f5b63au, 0x61e443aeu, 0xfef07813u, 0x88d5c6c8u, 0xf977870eu, 0x1f676baau, 0x790364a6u, 0xceaddea3u, 0x5887e72eu, 0xa09a1b70u, 0x1377e563u, 0x1bd8c3b2u, 0x0c54efeeu, 0xd524d8f7u, 0x3ec3d15au, 0xb2383a5du, 0xdaf15466u, 0xbb94fec0u, 0xe1e30a73u, 0x5f3f7be2u, 0x6a1c7101u, 0x6369b1ffu, 0x842d43bfu, 0x107d20bcu, 0x20fddadfu, 0x4b6dc970u, 0x0000002fu};
#define BN_E12_BITS 2790u
static const uint32_t BN_BT[2][8] = {{0x77b802a8u, 0x3bf938e3u, 0x3633535du, 0x020b1b27u, 0x49755260u, 0x26b7edf0u, 0x4384a86du, 0x2514c632u}, {0xd1dcff67u, 0x38e7ecccu, 0x93ce0d3eu, 0x65f0b37du, 0x22ac00aau, 0xd749d0ddu, 0x4a688d4du, 0x0141b9ceu}};
static const uint32_t BN_G1F[6][2][8] = {{{0xc58f0d9du, 0xd35d438du, 0xf5c70b3du, 0x0a78eb28u, 0x7879462cu, 0x666ea36fu, 0x9a07df2fu, 0x0e0a77c1u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}}, {{0x33144907u, 0xaf9ba696u, 0x87afb78au, 0xca6b1d73u, 0xf08a2087u, 0x11bded5eu, 0x1a1f3a7cu, 0x02f34d75u}, {0x4c492d72u, 0xa222ae23u, 0x565de15bu, 0xd00f02a4u, 0x53dfc926u, 0xdc2ff3a2u, 0xb3899551u, 0x10a75716u}}, {{0x4563ab30u, 0xb5773b10u, 0xa9aa6454u, 0x347f91c8u, 0x242e0991u, 0x7a007127u, 0x118214ecu, 0x1956bcd8u}, {0xa0aa4757u, 0x6e849f1eu, 0x89f89141u, 0xaa1c7b6du, 0xfae0ca3au, 0xb6e713cdu, 0x4e82ebc3u, 0x26694fbbu}}, {{0x2936b629u, 0xe4bbdd0cu, 0xe133bacbu, 0xbb30f162u, 0xf9645366u, 0x31a9d1b6u, 0xa500f8ddu, 0x253570beu}, {0x5ffe77c7u, 0xa1d77ce4u, 0x7826d1dbu, 0x07affd11u, 0xbb7edc6bu, 0x6d16bd27u, 0x85defeccu, 0x2c872002u}}, {{0x843abe92u, 0x7361d77fu, 0x273411fbu, 0xa5bb2bd3u, 0x4b3e2399u, 0x9c941f31u, 0xbb9fd3ecu, 0x15df9cddu}, {0x4bd8c949u, 0x5dddfd15u, 0xa4445b60u, 0x62cb29a5u, 0x0c7dd2b9u, 0x37bc870au, 0x3171f0fdu, 0x24830a9du}}, {{0x41690fe7u, 0xc970692fu, 0x27694b0bu, 0xe2403421u, 0x83c459e8u, 0x32bee66bu, 0x0ab08841u, 0x12aabcedu}, {0x40aebfa9u, 0x0d485d23u, 0xab2fcc57u, 0x05193418u, 0x8a4910f5u, 0xd3b0a40bu, 0x35d2925au, 0x2f21ebb5u}}};
static const uint32_t BN_G2F[6][8] = {{0xc58f0d9du, 0xd35d438du, 0xf5c70b3du, 0x0a78eb28u, 0x7879462cu, 0x666ea36fu, 0x9a07df2fu, 0x0e0a77c1u}, {0x00fa1bf2u, 0xca8d8005u, 0x68b39769u, 0xf0c5d614u, 0xad0d4418u, 0x0e201271u, 0xbad856e6u, 0x04290f65u}, {0x13e80b9cu, 0x3350c88eu, 0xdb5e56b9u, 0x7dce557cu, 0xb615564au, 0x6001b4b8u, 0x020217e0u, 0x2682e617u}, {0x12edefaau, 0x68c34889u, 0x72aabf4fu, 0x8d087f68u, 0x09081231u, 0x51e1a247u, 0x4729c0fau, 0x2259d6b1u}, {0xd782e155u, 0x71930c11u, 0xffbe3323u, 0xa6bb947cu, 0xd4741444u, 0xaa303344u, 0x26594943u, 0x2c3b3f0du}, {0xc494f1abu, 0x08cfc388u, 0x8d1373d4u, 0x19b31514u, 0xcb6c0213u, 0x584e90fdu, 0xdf2f8849u, 0x09e1685bu}};
static const uint32_t BN_GEN1[2][8] = {{0xc58f0d9du, 0xd35d438du, 0xf5c70b3du, 0x0a78eb28u, 0x7879462cu, 0x666ea36fu, 0x9a07df2fu, 0x0e0a77c1u}, {0x8b1e1b3au, 0xa6ba871bu, 0xeb8e167bu, 0x14f1d651u, 0xf0f28c58u, 0xccdd46deu, 0x340fbe5eu, 0x1c14ef83u}};
static const uint32_t BN_GEN2[2][2][8] = {{{0x02bc2026u, 0x8e83b5d1u, 0x497b0172u, 0xdceb1935u, 0x97811adfu, 0xfbb82647u, 0xaf96503bu, 0x19573841u}, {0xa84c6140u, 0xafb4737du, 0x5802d8c4u, 0x6043dd5au, 0x52a02f86u, 0x09e950fcu, 0x3aea7b6bu, 0x14fef083u}}, {{0x886be9f6u, 0x619dfa9du, 0xf59e9b78u, 0xfe7fd297u, 0x231b7dfeu, 0xff9e1a62u, 0xae9e4206u, 0x28fd7eebu}, {0xc71856eeu, 0x64095b56u, 0x327d3cbbu, 0xdc57f922u, 0x33351076u, 0x55f935beu, 0x93fd6482u, 0x0da4a0e6u}}};
static const uint32_t PR_AP[2][8] = {{0xa41f4b72u, 0xfcda3ec4u, 0x62ac7a78u, 0xa5ad2adfu, 0xe9a35115u, 0x46bb7691u, 0xd4acd190u, 0x0e928de9u}, {0xc2e11d2au, 0x20474aa4u, 0x59be6b21u, 0x45d73b21u, 0x16dfd9f6u, 0x9fa4833du, 0x87fd5e1cu, 0x03ce0908u}};
static const uint32_t PR_BQ[2][2][8] = {{{0xa022da58u, 0x387e4129u, 0x348910a2u, 0xe5aba09cu, 0x17b3d2e1u, 0xb5c2e584u, 0xc1e5f6c7u, 0x174de684u}, {0x1a183e4fu, 0x0fe6db8cu, 0xf0ac1fe6u, 0xd6135f1eu, 0x7c6a37b9u, 0xf66747bau, 0xb6f18adcu, 0x1e2ac018u}}, {{0xfce404aeu, 0x84a5f06cu, 0xb7c68680u, 0x88470685u, 0xbbd81ed4u, 0xd38397feu, 0x7d66cab2u, 0x01a96b99u}, {0x93da2dd4u, 0xe5d43b9bu, 0x83ec0aefu, 0xb7241b04u, 0x31e8ab23u, 0xa0ec477du, 0xf04d0e81u, 0x16e3db86u}}};
static const uint32_t PR_NABP[2][8] = {{0x73524960u, 0x5c59c9c1u, 0x74046ca6u, 0x9f8719afu, 0x58feda1eu, 0x80a4298au, 0x308d8b9au, 0x038a5205u}, {0x96ae2452u, 0x33e471a6u, 0x473b5707u, 0x6fe4cf5eu, 0x2c706004u, 0x6b819761u, 0x0144bd3fu, 0x110633c5u}};
static const uint32_t PR_NAB1P[2][8] = {{0x28c0236cu, 0x15c3962fu, 0x737ffb22u, 0x5a25e217u, 0xca2a5b6fu, 0x30362e7bu, 0xf98d91ecu, 0x1d47aea0u}, {0x678f6c4au, 0x70f5c994u, 0x11248013u, 0xaf55bd26u, 0x01d977aeu, 0x23b687bau, 0x42269424u, 0x2d0beb7cu}};
/* EIP-197 known answers: the pairs of the bn254 pairing test of
   revm-precompile 32.1.0 (test_bn254_pair). E1 is a valid 2-pair input,
   E2 has words off the curve, E3 has G1 at infinity, E4 has G2 at
   infinity. PR_EAP = a G1 and PR_EAQ = a G2 for the Fp12 bilinearity
   rows, a = 0x9e3779b97f4a7c15 (PR_EA_RAW). */
static const uint32_t PR_E1P[2][2][8] = {{{0x3de3f75fu, 0x5a42f6adu, 0x2b8a6babu, 0x490f2995u, 0x8f05ec38u, 0x113420dfu, 0xb842db3bu, 0x0fccfdc0u}, {0x68cc13a8u, 0x0c5bdb12u, 0xc96f8df4u, 0x894a6491u, 0xfddde147u, 0xa584078eu, 0x849c4a3du, 0x0bf53754u}}, {{0xa257af87u, 0x6864bf1fu, 0x568a5b38u, 0xfa447393u, 0x156e4202u, 0xd922522au, 0x92dac9deu, 0x25b5e7bbu}, {0xeba0804au, 0xd6031bf7u, 0xd0269d01u, 0xad855d60u, 0xa6fce03bu, 0x038e02c0u, 0x85d20025u, 0x2bbb38e9u}}};
static const uint32_t PR_E1Q[2][2][2][8] = {{{{0x60395307u, 0x80fb170bu, 0xc8ce0ccfu, 0x2f3b6cefu, 0xaa118aedu, 0x3dc50249u, 0x0f175688u, 0x1b52ddfau}, {0xf645c385u, 0xd6f006a5u, 0x127fb2e3u, 0xa949aa5fu, 0xb33d9defu, 0xd38926ffu, 0x2e447a34u, 0x17ffeae9u}}, {{0xe378a531u, 0x3b169eeeu, 0x9951cacbu, 0xfbe2b27cu, 0xce04f149u, 0xced11805u, 0x61eded2bu, 0x020613e2u}, {0xfaa124dcu, 0x3c118dc1u, 0x88570534u, 0x6a6feeb5u, 0xae97ba11u, 0x9c45b90cu, 0xcc2efc16u, 0x23cc1832u}}}, {{{0x02bc2026u, 0x8e83b5d1u, 0x497b0172u, 0xdceb1935u, 0x97811adfu, 0xfbb82647u, 0xaf96503bu, 0x19573841u}, {0xa84c6140u, 0xafb4737du, 0x5802d8c4u, 0x6043dd5au, 0x52a02f86u, 0x09e950fcu, 0x3aea7b6bu, 0x14fef083u}}, {{0x886be9f6u, 0x619dfa9du, 0xf59e9b78u, 0xfe7fd297u, 0x231b7dfeu, 0xff9e1a62u, 0xae9e4206u, 0x28fd7eebu}, {0xc71856eeu, 0x64095b56u, 0x327d3cbbu, 0xdc57f922u, 0x33351076u, 0x55f935beu, 0x93fd6482u, 0x0da4a0e6u}}}};
static const uint32_t PR_E2P[1][2][8] = {{{0x7e301e52u, 0x070a179au, 0x5f3f6719u, 0xbaf799b9u, 0xa4b94542u, 0x76c66b6fu, 0xb34b23bfu, 0x2cafa132u}, {0x7e301e52u, 0x070a179au, 0x5f3f6719u, 0xbaf799b9u, 0xa4b94542u, 0x76c66b6fu, 0xb34b23bfu, 0x2cafa132u}}};
static const uint32_t PR_E2Q[1][2][2][8] = {{{{0x7e301e52u, 0x070a179au, 0x5f3f6719u, 0xbaf799b9u, 0xa4b94542u, 0x76c66b6fu, 0xb34b23bfu, 0x2cafa132u}, {0x7e301e52u, 0x070a179au, 0x5f3f6719u, 0xbaf799b9u, 0xa4b94542u, 0x76c66b6fu, 0xb34b23bfu, 0x2cafa132u}}, {{0x7e301e52u, 0x070a179au, 0x5f3f6719u, 0xbaf799b9u, 0xa4b94542u, 0x76c66b6fu, 0xb34b23bfu, 0x2cafa132u}, {0x7e301e52u, 0x070a179au, 0x5f3f6719u, 0xbaf799b9u, 0xa4b94542u, 0x76c66b6fu, 0xb34b23bfu, 0x2cafa132u}}}};
static const uint32_t PR_E3P[1][2][8] = {{{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}}};
static const uint32_t PR_E3Q[1][2][2][8] = {{{{0x60395307u, 0x80fb170bu, 0xc8ce0ccfu, 0x2f3b6cefu, 0xaa118aedu, 0x3dc50249u, 0x0f175688u, 0x1b52ddfau}, {0xf645c385u, 0xd6f006a5u, 0x127fb2e3u, 0xa949aa5fu, 0xb33d9defu, 0xd38926ffu, 0x2e447a34u, 0x17ffeae9u}}, {{0xe378a531u, 0x3b169eeeu, 0x9951cacbu, 0xfbe2b27cu, 0xce04f149u, 0xced11805u, 0x61eded2bu, 0x020613e2u}, {0xfaa124dcu, 0x3c118dc1u, 0x88570534u, 0x6a6feeb5u, 0xae97ba11u, 0x9c45b90cu, 0xcc2efc16u, 0x23cc1832u}}}};
static const uint32_t PR_E4P[1][2][8] = {{{0x3de3f75fu, 0x5a42f6adu, 0x2b8a6babu, 0x490f2995u, 0x8f05ec38u, 0x113420dfu, 0xb842db3bu, 0x0fccfdc0u}, {0x68cc13a8u, 0x0c5bdb12u, 0xc96f8df4u, 0x894a6491u, 0xfddde147u, 0xa584078eu, 0x849c4a3du, 0x0bf53754u}}};
static const uint32_t PR_E4Q[1][2][2][8] = {{{{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}}, {{0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}, {0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u}}}};
static const uint32_t PR_EAP[2][8] = {{0x5b43f9edu, 0x4e189335u, 0x238a2575u, 0x198e11dcu, 0xa2b1644fu, 0xa969d0b8u, 0x8540889au, 0x112b23cau}, {0xe2d34a7au, 0x711e8efeu, 0xd4675833u, 0x5c95d57au, 0x696ce4bdu, 0x89b46994u, 0xa434f26cu, 0x094d34e5u}};
static const uint32_t PR_EAQ[2][2][8] = {{{0x1d73de98u, 0xb3a78641u, 0xf37432ccu, 0x0ab438f0u, 0x44c51a15u, 0x1aa64f4au, 0xccd8c211u, 0x225bc9c0u}, {0x88694fadu, 0xd97fc933u, 0xf2704395u, 0xaac6ed19u, 0x9f455ee8u, 0x5d1874b1u, 0x66a79ee9u, 0x1c237927u}}, {{0x9792516fu, 0xabffd623u, 0x912fa455u, 0xabcbf66bu, 0x1fbd57a2u, 0xb5ea4877u, 0x051db1a8u, 0x10d7cc2fu}, {0x2e73a197u, 0x28b42448u, 0x87dd6c33u, 0x55288d18u, 0x4478701du, 0x297f9ba8u, 0x4a673ba1u, 0x131a71aau}}};
static const uint32_t PR_EA_RAW[8] = {0x7f4a7c15u, 0x9e3779b9u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u, 0x00000000u};
#define PR_EA_BITS 64u

typedef struct {
  uint32_t m[8];
  uint32_t n0;
} PtMod;

static int fe_below(const uint32_t *x, const uint32_t *y) {
  uint32_t i = 8u;
  while (i > 0u && x[i - 1u] == y[i - 1u])
    i--;
  return i > 0u && x[i - 1u] < y[i - 1u];
}

static void fe_mont(const uint32_t *mw, uint32_t n0, const uint32_t *aw, const uint32_t *bw, uint32_t *t) {
  uint32_t u[8];
  uint32_t r[8] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
  uint32_t hi;
  uint32_t q;
  uint64_t s;
  uint32_t i;
  uint32_t j;
  for (i = 0u; i < 8u; i++) {
    s = 0u;
    for (j = 0u; j < 8u; j++) {
      s = (uint64_t)r[j] + (uint64_t)aw[j] * bw[i] + (s >> 32);
      u[j] = (uint32_t)s;
    }
    hi = (uint32_t)(s >> 32);
    q = u[0] * n0;
    s = 0u;
    for (j = 0u; j < 8u; j++) {
      s = (uint64_t)u[j] + (uint64_t)q * mw[j] + (s >> 32);
      if (j > 0u)
        r[j - 1u] = (uint32_t)s;
    }
    r[7] = hi + (uint32_t)(s >> 32);
  }
  if (!fe_below(r, mw)) {
    s = 0u;
    for (j = 0u; j < 8u; j++) {
      s = (uint64_t)r[j] - mw[j] - (s >> 63);
      r[j] = (uint32_t)s;
    }
  }
  for (j = 0u; j < 8u; j++)
    t[j] = r[j];
}

static int fe_zero(const uint32_t *x) {
  uint32_t i;
  uint32_t o = 0u;
  for (i = 0u; i < 8u; i++)
    o |= x[i];
  return o == 0u;
}

static void fe_copy(uint32_t *t, const uint32_t *x) {
  uint32_t i;
  for (i = 0u; i < 8u; i++)
    t[i] = x[i];
}

static void fe_add_raw(const uint32_t *x, const uint32_t *y, uint32_t *t) {
  uint64_t s = 0u;
  uint32_t j;
  for (j = 0u; j < 8u; j++) {
    s = (uint64_t)x[j] + y[j] + (s >> 32);
    t[j] = (uint32_t)s;
  }
}

static void fe_sub_raw(const uint32_t *x, const uint32_t *y, uint32_t *t) {
  uint64_t s = 0u;
  uint32_t j;
  for (j = 0u; j < 8u; j++) {
    s = (uint64_t)x[j] - y[j] - (s >> 63);
    t[j] = (uint32_t)s;
  }
}

static void pt_fm(const PtMod *d, const uint32_t *x, const uint32_t *y, uint32_t *t) {
  fe_mont(d->m, d->n0, x, y, t);
}

static void pt_add(const PtMod *d, const uint32_t *x, const uint32_t *y, uint32_t *t) {
  uint32_t r[8];
  fe_add_raw(x, y, r);
  if (fe_below(r, d->m)) {
    fe_copy(t, r);
    return;
  }
  fe_sub_raw(r, d->m, t);
}

static void pt_sub(const PtMod *d, const uint32_t *x, const uint32_t *y, uint32_t *t) {
  int lt = fe_below(x, y);
  fe_sub_raw(x, y, t);
  if (lt)
    fe_add_raw(t, d->m, t);
}


/* Fp2 = Fp[u] / (u^2 + 1): a + b u. */
typedef struct {
  uint32_t a[8];
  uint32_t b[8];
} F2;

/* Fp6 = Fp2[v] / (v^3 - xi), xi = 9 + u: a + b v + c v^2. */
typedef struct {
  F2 a;
  F2 b;
  F2 c;
} F6;

/* Fp12 = Fp6[w] / (w^2 - v): a + b w. */
typedef struct {
  F6 a;
  F6 b;
} F12;

/* A point (X, Y, Z) of the twist y^2 = x^3 + b' over Fp2. G2H: homogeneous
   form (X / Z, Y / Z), the Miller loop. G2J: Jacobian form (X / Z^2,
   Y / Z^3), the subgroup check. */
typedef struct {
  F2 x;
  F2 y;
  F2 z;
} G2P3;

/* An affine G2 point. (0, 0) is the point at infinity. */
typedef struct {
  F2 x;
  F2 y;
} G2A;

static const uint32_t FE_NIL[8] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};

static int bit_at(const uint32_t *e, uint32_t i) {
  return ((e[i / 32u] >> (i % 32u)) & 1u) != 0u;
}

static void fp_neg(const PtMod *d, const uint32_t *x, uint32_t *t) {
  pt_sub(d, FE_NIL, x, t);
}

/* x^e over the BITS low bits of E, the top bit first, from 1. T can be X. */
static void fp_pow(const PtMod *d, const uint32_t *x, const uint32_t *e, uint32_t bits, uint32_t *t) {
  uint32_t r[8];
  uint32_t b[8];
  uint32_t i;
  fe_copy(b, x);
  fe_copy(r, BN_ONE);
  for (i = bits; i > 0u; i--) {
    pt_fm(d, r, r, r);
    if (bit_at(e, i - 1u))
      pt_fm(d, r, b, r);
  }
  fe_copy(t, r);
}

static int fe_same(const uint32_t *x, const uint32_t *y) {
  uint32_t i;
  uint32_t o = 0u;
  for (i = 0u; i < 8u; i++)
    o |= x[i] ^ y[i];
  return o == 0u;
}

static void f2_set(F2 *t, const uint32_t (*g)[8]) {
  fe_copy(t->a, g[0]);
  fe_copy(t->b, g[1]);
}

static void f2_zero(F2 *t) {
  fe_copy(t->a, FE_NIL);
  fe_copy(t->b, FE_NIL);
}

static void f2_one(F2 *t) {
  fe_copy(t->a, BN_ONE);
  fe_copy(t->b, FE_NIL);
}

static int f2_is_zero(const F2 *x) {
  return fe_zero(x->a) && fe_zero(x->b);
}

static int f2_same(const F2 *x, const F2 *y) {
  return fe_same(x->a, y->a) && fe_same(x->b, y->b);
}

/* f2Add, f2Sub and the other F2 ops: T can be X or Y. */
static void f2_add(const PtMod *d, const F2 *x, const F2 *y, F2 *t) {
  pt_add(d, x->a, y->a, t->a);
  pt_add(d, x->b, y->b, t->b);
}

static void f2_sub(const PtMod *d, const F2 *x, const F2 *y, F2 *t) {
  pt_sub(d, x->a, y->a, t->a);
  pt_sub(d, x->b, y->b, t->b);
}

static void f2_neg(const PtMod *d, const F2 *x, F2 *t) {
  fp_neg(d, x->a, t->a);
  fp_neg(d, x->b, t->b);
}

static void f2_conj(const PtMod *d, const F2 *x, F2 *t) {
  fe_copy(t->a, x->a);
  fp_neg(d, x->b, t->b);
}

/* f2Mul (Karatsuba, 3 Montgomery products): v0 = a0 b0, v1 = a1 b1,
   (v0 - v1) + ((a0 + a1) (b0 + b1) - v0 - v1) u. */
static void f2_mul(const PtMod *d, const F2 *x, const F2 *y, F2 *t) {
  uint32_t v0[8];
  uint32_t v1[8];
  uint32_t s[8];
  uint32_t u[8];
  pt_fm(d, x->a, y->a, v0);
  pt_fm(d, x->b, y->b, v1);
  pt_add(d, x->a, x->b, s);
  pt_add(d, y->a, y->b, u);
  pt_fm(d, s, u, s);
  pt_sub(d, v0, v1, t->a);
  pt_sub(d, s, v0, s);
  pt_sub(d, s, v1, t->b);
}

/* f2Scale: x s for an Fp value S. */
static void f2_scale(const PtMod *d, const F2 *x, const uint32_t *s, F2 *t) {
  pt_fm(d, x->a, s, t->a);
  pt_fm(d, x->b, s, t->b);
}

/* fe9: 9 x as x2 = x + x, x4 = x2 + x2, x4 + x4 + x. */
static void fp_nine(const PtMod *d, const uint32_t *x, uint32_t *t) {
  uint32_t x2[8];
  uint32_t x4[8];
  pt_add(d, x, x, x2);
  pt_add(d, x2, x2, x4);
  pt_add(d, x4, x4, x2);
  pt_add(d, x2, x, t);
}

/* f2Xi: (9 + u) (a + b u) = (9 a - b) + (9 b + a) u. */
static void f2_xi(const PtMod *d, const F2 *x, F2 *t) {
  uint32_t a[8];
  uint32_t b[8];
  fp_nine(d, x->a, a);
  fp_nine(d, x->b, b);
  pt_sub(d, a, x->b, a);
  pt_add(d, b, x->a, t->b);
  fe_copy(t->a, a);
}

/* f2Inv: (a - b u) / (a^2 + b^2). 0 gives 0. */
static void f2_inv(const PtMod *d, const F2 *x, F2 *t) {
  uint32_t n[8];
  uint32_t s[8];
  pt_fm(d, x->a, x->a, n);
  pt_fm(d, x->b, x->b, s);
  pt_add(d, n, s, n);
  fp_pow(d, n, BN_PM2_RAW, 256u, n);
  pt_fm(d, x->a, n, t->a);
  pt_fm(d, x->b, n, s);
  fp_neg(d, s, t->b);
}

static void f6_zero(F6 *t) {
  f2_zero(&t->a);
  f2_zero(&t->b);
  f2_zero(&t->c);
}

static void f6_add(const PtMod *d, const F6 *x, const F6 *y, F6 *t) {
  f2_add(d, &x->a, &y->a, &t->a);
  f2_add(d, &x->b, &y->b, &t->b);
  f2_add(d, &x->c, &y->c, &t->c);
}

static void f6_sub(const PtMod *d, const F6 *x, const F6 *y, F6 *t) {
  f2_sub(d, &x->a, &y->a, &t->a);
  f2_sub(d, &x->b, &y->b, &t->b);
  f2_sub(d, &x->c, &y->c, &t->c);
}

static void f6_neg(const PtMod *d, const F6 *x, F6 *t) {
  f2_neg(d, &x->a, &t->a);
  f2_neg(d, &x->b, &t->b);
  f2_neg(d, &x->c, &t->c);
}

/* f6MulV: (a + b v + c v^2) v = xi c + a v + b v^2. */
static void f6_mul_v(const PtMod *d, const F6 *x, F6 *t) {
  F2 c;
  f2_xi(d, &x->c, &c);
  t->c = x->b;
  t->b = x->a;
  t->a = c;
}

/* f6Mul (Karatsuba, 6 f2Mul): v0 = a0 b0, v1 = a1 b1, v2 = a2 b2,
   c0 = v0 + xi ((a1 + a2) (b1 + b2) - v1 - v2),
   c1 = (a0 + a1) (b0 + b1) - v0 - v1 + xi v2,
   c2 = (a0 + a2) (b0 + b2) - v0 - v2 + v1. */
static void f6_mul(const PtMod *d, const F6 *x, const F6 *y, F6 *t) {
  F2 v0;
  F2 v1;
  F2 v2;
  F2 s;
  F2 u;
  F2 c0;
  F2 c1;
  f2_mul(d, &x->a, &y->a, &v0);
  f2_mul(d, &x->b, &y->b, &v1);
  f2_mul(d, &x->c, &y->c, &v2);
  f2_add(d, &x->b, &x->c, &s);
  f2_add(d, &y->b, &y->c, &u);
  f2_mul(d, &s, &u, &s);
  f2_sub(d, &s, &v1, &s);
  f2_sub(d, &s, &v2, &s);
  f2_xi(d, &s, &s);
  f2_add(d, &v0, &s, &c0);
  f2_add(d, &x->a, &x->b, &s);
  f2_add(d, &y->a, &y->b, &u);
  f2_mul(d, &s, &u, &s);
  f2_sub(d, &s, &v0, &s);
  f2_sub(d, &s, &v1, &s);
  f2_xi(d, &v2, &u);
  f2_add(d, &s, &u, &c1);
  f2_add(d, &x->a, &x->c, &s);
  f2_add(d, &y->a, &y->c, &u);
  f2_mul(d, &s, &u, &s);
  f2_sub(d, &s, &v0, &s);
  f2_sub(d, &s, &v2, &s);
  f2_add(d, &s, &v1, &t->c);
  t->a = c0;
  t->b = c1;
}

/* f6Inv: t0 = a^2 - xi b c, t1 = xi c^2 - a b, t2 = b^2 - a c,
   n = a t0 + xi (c t1 + b t2), then (t0, t1, t2) / n. */
static void f6_inv(const PtMod *d, const F6 *x, F6 *t) {
  F2 t0;
  F2 t1;
  F2 t2;
  F2 s;
  F2 n;
  f2_mul(d, &x->a, &x->a, &t0);
  f2_mul(d, &x->b, &x->c, &s);
  f2_xi(d, &s, &s);
  f2_sub(d, &t0, &s, &t0);
  f2_mul(d, &x->c, &x->c, &t1);
  f2_xi(d, &t1, &t1);
  f2_mul(d, &x->a, &x->b, &s);
  f2_sub(d, &t1, &s, &t1);
  f2_mul(d, &x->b, &x->b, &t2);
  f2_mul(d, &x->a, &x->c, &s);
  f2_sub(d, &t2, &s, &t2);
  f2_mul(d, &x->c, &t1, &n);
  f2_mul(d, &x->b, &t2, &s);
  f2_add(d, &n, &s, &n);
  f2_xi(d, &n, &n);
  f2_mul(d, &x->a, &t0, &s);
  f2_add(d, &s, &n, &n);
  f2_inv(d, &n, &n);
  f2_mul(d, &t0, &n, &t->a);
  f2_mul(d, &t1, &n, &t->b);
  f2_mul(d, &t2, &n, &t->c);
}

static void f12_one(F12 *t) {
  f6_zero(&t->a);
  f6_zero(&t->b);
  f2_one(&t->a.a);
}

static int f12_is_one(const F12 *x) {
  F12 one;
  f12_one(&one);
  return f2_same(&x->a.a, &one.a.a) && f2_is_zero(&x->a.b) && f2_is_zero(&x->a.c) && f2_is_zero(&x->b.a) &&
         f2_is_zero(&x->b.b) && f2_is_zero(&x->b.c);
}

/* f12Mul (Karatsuba, 3 f6Mul): v0 = a0 b0, v1 = a1 b1,
   (v0 + v v1) + ((a0 + a1) (b0 + b1) - v0 - v1) w. T can be X or Y. */
static void f12_mul(const PtMod *d, const F12 *x, const F12 *y, F12 *t) {
  F6 v0;
  F6 v1;
  F6 s;
  F6 u;
  f6_mul(d, &x->a, &y->a, &v0);
  f6_mul(d, &x->b, &y->b, &v1);
  f6_add(d, &x->a, &x->b, &s);
  f6_add(d, &y->a, &y->b, &u);
  f6_mul(d, &s, &u, &s);
  f6_sub(d, &s, &v0, &s);
  f6_sub(d, &s, &v1, &t->b);
  f6_mul_v(d, &v1, &v1);
  f6_add(d, &v0, &v1, &t->a);
}

/* f12Conj: a - b w, the p^6 power. */
static void f12_conj(const PtMod *d, const F12 *x, F12 *t) {
  t->a = x->a;
  f6_neg(d, &x->b, &t->b);
}

/* f12Inv: (a - b w) / (a^2 - v b^2). */
static void f12_inv(const PtMod *d, const F12 *x, F12 *t) {
  F6 s;
  F6 u;
  f6_mul(d, &x->a, &x->a, &s);
  f6_mul(d, &x->b, &x->b, &u);
  f6_mul_v(d, &u, &u);
  f6_sub(d, &s, &u, &s);
  f6_inv(d, &s, &s);
  f6_mul(d, &x->a, &s, &t->a);
  f6_mul(d, &x->b, &s, &u);
  f6_neg(d, &u, &t->b);
}

/* The coefficient of w^k of x = sum c_k w^k: k 0, 2, 4 are a.a, a.b, a.c and
   k 1, 3, 5 are b.a, b.b, b.c. */
static F2 *f12_at(F12 *x, uint32_t k) {
  F6 *h = k % 2u == 0u ? &x->a : &x->b;
  return k / 2u == 0u ? &h->a : k / 2u == 1u ? &h->b : &h->c;
}

/* f12Frob: the p power, c_k -> conj(c_k) xi^(k (p - 1) / 6). */
static void f12_frob(const PtMod *d, const F12 *x, F12 *t) {
  F12 r = *x;
  F2 g;
  uint32_t k;
  for (k = 0u; k < 6u; k++) {
    f2_conj(d, f12_at(&r, k), f12_at(&r, k));
    if (k > 0u) {
      f2_set(&g, BN_G1F[k]);
      f2_mul(d, f12_at(&r, k), &g, f12_at(&r, k));
    }
  }
  *t = r;
}

/* f12Frob2: the p^2 power, c_k -> c_k xi^(k (p^2 - 1) / 6) (an Fp value). */
static void f12_frob2(const PtMod *d, const F12 *x, F12 *t) {
  F12 r = *x;
  uint32_t k;
  for (k = 1u; k < 6u; k++)
    f2_scale(d, f12_at(&r, k), BN_G2F[k], f12_at(&r, k));
  *t = r;
}

/* x^e over the BITS low bits of E, the top bit first, from 1. */
static void f12_pow(const PtMod *d, const F12 *x, const uint32_t *e, uint32_t bits, F12 *t) {
  F12 r;
  F12 b = *x;
  uint32_t i;
  f12_one(&r);
  for (i = bits; i > 0u; i--) {
    f12_mul(d, &r, &r, &r);
    if (bit_at(e, i - 1u))
      f12_mul(d, &r, &b, &r);
  }
  *t = r;
}

/* The final exponentiation f^((p^12 - 1) / r): the easy part f^((p^6 - 1)
   (p^2 + 1)), then the hard part (p^4 - p^2 + 1) / r with three powers of
   x (Beuchat et al. 2010, Algorithm 1 steps 13 to 15). */
static void final_exp(const PtMod *d, const F12 *in, F12 *out) {
  F12 t0;
  F12 t1;
  F12 t2;
  F12 fp;
  F12 fp2;
  F12 fp3;
  F12 fu;
  F12 fu2;
  F12 fu3;
  F12 fu2p;
  F12 fu3p;
  F12 y0;
  F12 y1;
  F12 y2;
  F12 y3;
  F12 y4;
  F12 y5;
  F12 y6;
  f12_conj(d, in, &t1);
  f12_inv(d, in, &t2);
  f12_mul(d, &t1, &t2, &t1);
  f12_frob2(d, &t1, &t2);
  f12_mul(d, &t1, &t2, &t1);
  f12_frob(d, &t1, &fp);
  f12_frob2(d, &t1, &fp2);
  f12_frob(d, &fp2, &fp3);
  f12_pow(d, &t1, BN_X_RAW, BN_X_BITS, &fu);
  f12_pow(d, &fu, BN_X_RAW, BN_X_BITS, &fu2);
  f12_pow(d, &fu2, BN_X_RAW, BN_X_BITS, &fu3);
  f12_frob(d, &fu, &y3);
  f12_frob(d, &fu2, &fu2p);
  f12_frob(d, &fu3, &fu3p);
  f12_frob2(d, &fu2, &y2);
  f12_mul(d, &fp, &fp2, &y0);
  f12_mul(d, &y0, &fp3, &y0);
  f12_conj(d, &t1, &y1);
  f12_conj(d, &fu2, &y5);
  f12_conj(d, &y3, &y3);
  f12_mul(d, &fu, &fu2p, &y4);
  f12_conj(d, &y4, &y4);
  f12_mul(d, &fu3, &fu3p, &y6);
  f12_conj(d, &y6, &y6);
  f12_mul(d, &y6, &y6, &t0);
  f12_mul(d, &t0, &y4, &t0);
  f12_mul(d, &t0, &y5, &t0);
  f12_mul(d, &y3, &y5, &t1);
  f12_mul(d, &t1, &t0, &t1);
  f12_mul(d, &t0, &y2, &t0);
  f12_mul(d, &t1, &t1, &t1);
  f12_mul(d, &t1, &t0, &t1);
  f12_mul(d, &t1, &t1, &t1);
  f12_mul(d, &t1, &y1, &t0);
  f12_mul(d, &t1, &y0, &t1);
  f12_mul(d, &t0, &t0, &t0);
  f12_mul(d, &t0, &t1, out);
}

/* mDbl: T = 2 T and f = f^2 l, l the tangent line at T at the point
   (px, py) of G1 (Aranha et al. 2011, homogeneous form, D-type twist):
   A = X Y / 2, B = Y^2, C = Z^2, E = 3 b' C, F = 3 E, G = (B + F) / 2,
   H = (Y + Z)^2 - (B + C), J = X^2; T = (A (B - F), G^2 - 3 E^2, B H);
   l = -H py + 3 J px w + (E - B) v w. */
static void miller_dbl(const PtMod *d, G2P3 *t, const uint32_t *px, const uint32_t *py, F12 *f) {
  F2 a;
  F2 b;
  F2 c;
  F2 e;
  F2 ff;
  F2 g;
  F2 h;
  F2 j;
  F2 s;
  F2 bt;
  F12 l;
  f2_set(&bt, BN_BT);
  f2_mul(d, &t->x, &t->y, &a);
  f2_scale(d, &a, BN_HALF, &a);
  f2_mul(d, &t->y, &t->y, &b);
  f2_mul(d, &t->z, &t->z, &c);
  f2_add(d, &c, &c, &s);
  f2_add(d, &s, &c, &s);
  f2_mul(d, &bt, &s, &e);
  f2_add(d, &e, &e, &ff);
  f2_add(d, &ff, &e, &ff);
  f2_add(d, &b, &ff, &g);
  f2_scale(d, &g, BN_HALF, &g);
  f2_add(d, &t->y, &t->z, &h);
  f2_mul(d, &h, &h, &h);
  f2_add(d, &b, &c, &s);
  f2_sub(d, &h, &s, &h);
  f2_mul(d, &t->x, &t->x, &j);
  f6_zero(&l.a);
  f6_zero(&l.b);
  f2_neg(d, &h, &s);
  f2_scale(d, &s, py, &l.a.a);
  f2_add(d, &j, &j, &s);
  f2_add(d, &s, &j, &s);
  f2_scale(d, &s, px, &l.b.a);
  f2_sub(d, &e, &b, &l.b.b);
  f2_sub(d, &b, &ff, &s);
  f2_mul(d, &a, &s, &t->x);
  f2_mul(d, &e, &e, &s);
  f2_add(d, &s, &s, &c);
  f2_add(d, &c, &s, &s);
  f2_mul(d, &g, &g, &t->y);
  f2_sub(d, &t->y, &s, &t->y);
  f2_mul(d, &b, &h, &t->z);
  f12_mul(d, f, f, f);
  f12_mul(d, f, &l, f);
}

/* mAdd: T = T + Q and f = f l, l the line through T and Q (affine) at the
   point (px, py): theta = Y - qy Z, lambda = X - qx Z, C = theta^2,
   D = lambda^2, E = lambda D, F = Z C, G = X D, H = E + F - 2 G;
   T = (lambda H, theta (G - H) - Y E, Z E);
   l = lambda py - theta px w + (theta qx - lambda qy) v w. */
static void miller_add(const PtMod *d, G2P3 *t, const G2A *q, const uint32_t *px, const uint32_t *py, F12 *f) {
  F2 th;
  F2 la;
  F2 c;
  F2 dd;
  F2 e;
  F2 ff;
  F2 g;
  F2 h;
  F2 s;
  F12 l;
  f2_mul(d, &q->y, &t->z, &s);
  f2_sub(d, &t->y, &s, &th);
  f2_mul(d, &q->x, &t->z, &s);
  f2_sub(d, &t->x, &s, &la);
  f2_mul(d, &th, &th, &c);
  f2_mul(d, &la, &la, &dd);
  f2_mul(d, &la, &dd, &e);
  f2_mul(d, &t->z, &c, &ff);
  f2_mul(d, &t->x, &dd, &g);
  f2_add(d, &e, &ff, &h);
  f2_sub(d, &h, &g, &h);
  f2_sub(d, &h, &g, &h);
  f6_zero(&l.a);
  f6_zero(&l.b);
  f2_scale(d, &la, py, &l.a.a);
  f2_neg(d, &th, &s);
  f2_scale(d, &s, px, &l.b.a);
  f2_mul(d, &th, &q->x, &l.b.b);
  f2_mul(d, &la, &q->y, &s);
  f2_sub(d, &l.b.b, &s, &l.b.b);
  f2_mul(d, &la, &h, &t->x);
  f2_sub(d, &g, &h, &s);
  f2_mul(d, &th, &s, &s);
  f2_mul(d, &t->y, &e, &c);
  f2_sub(d, &s, &c, &t->y);
  f2_mul(d, &t->z, &e, &t->z);
  f12_mul(d, f, &l, f);
}

/* The Miller loop of the optimal ate pairing over the bits of 6 x + 2 (the
   top bit first), then the lines to pi(Q) and -pi^2(Q):
   pi(Q) = (conj(qx) xi^((p - 1) / 3), conj(qy) xi^((p - 1) / 2)),
   -pi^2(Q) = (qx xi^((p^2 - 1) / 3), qy). */
static void miller(const PtMod *d, const uint32_t *px, const uint32_t *py, const G2A *q, F12 *f) {
  G2P3 t;
  G2A q1;
  F2 g;
  uint32_t i;
  t.x = q->x;
  t.y = q->y;
  f2_one(&t.z);
  f12_one(f);
  for (i = BN_ATE_BITS - 1u; i > 0u; i--) {
    miller_dbl(d, &t, px, py, f);
    if (bit_at(BN_ATE_RAW, i - 1u))
      miller_add(d, &t, q, px, py, f);
  }
  f2_conj(d, &q->x, &q1.x);
  f2_set(&g, BN_G1F[2]);
  f2_mul(d, &q1.x, &g, &q1.x);
  f2_conj(d, &q->y, &q1.y);
  f2_set(&g, BN_G1F[3]);
  f2_mul(d, &q1.y, &g, &q1.y);
  miller_add(d, &t, &q1, px, py, f);
  f2_scale(d, &q->x, BN_G2F[2], &q1.x);
  q1.y = q->y;
  miller_add(d, &t, &q1, px, py, f);
}

/* g2Dbl: ptDbl over Fp2, the same field ops in the same order. O can be A. */
static void g2_dbl(const PtMod *d, const G2P3 *a, G2P3 *o) {
  F2 yy;
  F2 s;
  F2 xx;
  F2 mm;
  F2 x3;
  F2 y8;
  F2 yz;
  F2 t;
  if (f2_is_zero(&a->z) || f2_is_zero(&a->y)) {
    f2_zero(&o->x);
    f2_zero(&o->y);
    f2_zero(&o->z);
    return;
  }
  f2_mul(d, &a->y, &a->y, &yy);
  f2_mul(d, &a->x, &yy, &s);
  f2_add(d, &s, &s, &s);
  f2_add(d, &s, &s, &s);
  f2_mul(d, &a->x, &a->x, &xx);
  f2_add(d, &xx, &xx, &mm);
  f2_add(d, &mm, &xx, &mm);
  f2_mul(d, &mm, &mm, &t);
  f2_sub(d, &t, &s, &t);
  f2_sub(d, &t, &s, &x3);
  f2_mul(d, &yy, &yy, &y8);
  f2_add(d, &y8, &y8, &y8);
  f2_add(d, &y8, &y8, &y8);
  f2_add(d, &y8, &y8, &y8);
  f2_mul(d, &a->y, &a->z, &yz);
  f2_sub(d, &s, &x3, &t);
  f2_mul(d, &mm, &t, &t);
  f2_sub(d, &t, &y8, &o->y);
  f2_add(d, &yz, &yz, &o->z);
  o->x = x3;
}

/* g2Madd: ptMadd over Fp2 (Jacobian a plus affine b). O can be A. */
static void g2_madd(const PtMod *d, const G2P3 *a, const G2A *b, G2P3 *o) {
  F2 zz;
  F2 h;
  F2 r;
  F2 hh;
  F2 hhh;
  F2 v;
  F2 x3;
  F2 t;
  F2 u;
  if (f2_is_zero(&b->x) && f2_is_zero(&b->y)) {
    *o = *a;
    return;
  }
  if (f2_is_zero(&a->z)) {
    o->x = b->x;
    o->y = b->y;
    f2_one(&o->z);
    return;
  }
  f2_mul(d, &a->z, &a->z, &zz);
  f2_mul(d, &b->x, &zz, &h);
  f2_sub(d, &h, &a->x, &h);
  f2_mul(d, &b->y, &zz, &r);
  f2_mul(d, &r, &a->z, &r);
  f2_sub(d, &r, &a->y, &r);
  if (f2_is_zero(&h)) {
    if (f2_is_zero(&r)) {
      g2_dbl(d, a, o);
      return;
    }
    f2_zero(&o->x);
    f2_zero(&o->y);
    f2_zero(&o->z);
    return;
  }
  f2_mul(d, &h, &h, &hh);
  f2_mul(d, &h, &hh, &hhh);
  f2_mul(d, &a->x, &hh, &v);
  f2_mul(d, &r, &r, &t);
  f2_sub(d, &t, &hhh, &t);
  f2_sub(d, &t, &v, &t);
  f2_sub(d, &t, &v, &x3);
  f2_sub(d, &v, &x3, &t);
  f2_mul(d, &r, &t, &t);
  f2_mul(d, &a->y, &hhh, &u);
  f2_sub(d, &t, &u, &o->y);
  f2_mul(d, &a->z, &h, &o->z);
  o->x = x3;
}

/* 1 when the G1 point (x, y) is (0, 0), or its coordinates are below p and
   y^2 = x^3 + 3. */
static int g1_valid(const PtMod *d, const uint32_t *x, const uint32_t *y) {
  uint32_t l[8];
  uint32_t r[8];
  if (fe_zero(x) && fe_zero(y))
    return 1;
  if (!fe_below(x, d->m) || !fe_below(y, d->m))
    return 0;
  pt_fm(d, y, y, l);
  pt_fm(d, x, x, r);
  pt_fm(d, r, x, r);
  pt_add(d, r, BN_THREE, r);
  return fe_same(l, r);
}

/* 1 when the G2 point q is (0, 0), or its coordinates are below p,
   y^2 = x^3 + b' and r q is the point at infinity (USER ruling Q-S8b-8 (a)). */
static int g2_valid(const PtMod *d, const G2A *q) {
  F2 l;
  F2 r;
  G2P3 acc;
  uint32_t i;
  if (f2_is_zero(&q->x) && f2_is_zero(&q->y))
    return 1;
  if (!fe_below(q->x.a, d->m) || !fe_below(q->x.b, d->m) || !fe_below(q->y.a, d->m) || !fe_below(q->y.b, d->m))
    return 0;
  f2_mul(d, &q->y, &q->y, &l);
  f2_mul(d, &q->x, &q->x, &r);
  f2_mul(d, &r, &q->x, &r);
  f2_set(&acc.x, BN_BT);
  f2_add(d, &r, &acc.x, &r);
  if (!f2_same(&l, &r))
    return 0;
  f2_zero(&acc.x);
  f2_zero(&acc.y);
  f2_zero(&acc.z);
  for (i = 256u; i > 0u; i--) {
    g2_dbl(d, &acc, &acc);
    if (bit_at(BN_R_RAW, i - 1u))
      g2_madd(d, &acc, q, &acc);
  }
  return f2_is_zero(&acc.z);
}

/* pairCheck: 1 when each point is valid and the product of the pairings
   e(P_i, Q_i) is 1. A pair with a point at infinity gives 1. */
static int pair_check(const PtMod *d, uint32_t n, const uint32_t (*p)[2][8], const G2A *q) {
  F12 acc;
  F12 f;
  uint32_t i;
  f12_one(&acc);
  for (i = 0u; i < n; i++) {
    if (!g1_valid(d, p[i][0], p[i][1]) || !g2_valid(d, &q[i]))
      return 0;
    if ((fe_zero(p[i][0]) && fe_zero(p[i][1])) || (f2_is_zero(&q[i].x) && f2_is_zero(&q[i].y)))
      continue;
    miller(d, p[i][0], p[i][1], &q[i], &f);
    f12_mul(d, &acc, &f, &acc);
  }
  final_exp(d, &acc, &f);
  return f12_is_one(&f);
}

static void pair_row(unsigned *bad, const char *name, int got, int want) {
  if (got != want) {
    fprintf(stderr, "cryptotool: pairing oracle row wrong: %s\n", name);
    *bad += 1u;
  }
}

static void g2a_set(G2A *q, const uint32_t (*g)[2][8]) {
  f2_set(&q->x, g[0]);
  f2_set(&q->y, g[1]);
}

static void g1_set(uint32_t (*t)[8], const uint32_t (*g)[8]) {
  fe_copy(t[0], g[0]);
  fe_copy(t[1], g[1]);
}

/* A fixed Fp12 test value with each limb set from K. */
static void f12_fill(const PtMod *d, F12 *x, uint32_t k) {
  uint32_t c;
  uint32_t v[8];
  for (c = 0u; c < 12u; c++) {
    uint32_t i;
    F2 *z = f12_at(x, c / 2u);
    for (i = 0u; i < 8u; i++)
      v[i] = (k * 2654435761u) ^ (c * 40503u + i * 97u);
    v[7] &= 0x0fffffffu;
    pt_fm(d, v, BN_ONE, c % 2u == 0u ? z->a : z->b);
  }
}

static int f12_same(const F12 *x, const F12 *y) {
  uint32_t k;
  F12 a = *x;
  F12 b = *y;
  for (k = 0u; k < 6u; k++)
    if (!f2_same(f12_at(&a, k), f12_at(&b, k)))
      return 0;
  return 1;
}

static void pairo_rows(unsigned *bad) {
  PtMod d;
  F12 x;
  F12 y;
  F12 z;
  F12 m;
  uint32_t pp[4][2][8];
  G2A qq[4];
  fe_copy(d.m, BN_P);
  d.n0 = BN_N0;
  f12_fill(&d, &x, 7u);
  f12_inv(&d, &x, &y);
  f12_mul(&d, &x, &y, &z);
  pair_row(bad, "f12_inv x * x^-1 = 1", f12_is_one(&z), 1);
  f12_frob(&d, &x, &y);
  f12_pow(&d, &x, BN_P, 254u, &z);
  pair_row(bad, "f12_frob x = x^p", f12_same(&y, &z), 1);
  f12_frob(&d, &y, &y);
  f12_frob2(&d, &x, &z);
  pair_row(bad, "f12_frob2 x = frob (frob x)", f12_same(&y, &z), 1);
  g2a_set(&qq[0], BN_GEN2);
  miller(&d, BN_GEN1[0], BN_GEN1[1], &qq[0], &m);
  final_exp(&d, &m, &y);
  f12_pow(&d, &m, BN_E12_RAW, BN_E12_BITS, &z);
  pair_row(bad, "final_exp f = f^((p^12 - 1) / r)", f12_same(&y, &z), 1);
  pair_row(bad, "e(G1, G2) is not 1", f12_is_one(&y), 0);
  f12_pow(&d, &y, BN_R_RAW, 254u, &z);
  pair_row(bad, "e(G1, G2)^r = 1", f12_is_one(&z), 1);
  pair_row(bad, "G2 generator valid", g2_valid(&d, &qq[0]), 1);
  g1_set(pp[0], BN_GEN1);
  g1_set(pp[1], BN_GEN1);
  fp_neg(&d, pp[1][1], pp[1][1]);
  qq[1] = qq[0];
  pair_row(bad, "pairCheck [(G1, G2), (-G1, G2)]", pair_check(&d, 2u, (const uint32_t(*)[2][8])pp, qq), 1);
  pair_row(bad, "pairCheck [(G1, G2)]", pair_check(&d, 1u, (const uint32_t(*)[2][8])pp, qq), 0);
  pair_row(bad, "pairCheck []", pair_check(&d, 0u, (const uint32_t(*)[2][8])pp, qq), 1);
  g1_set(pp[0], PR_AP);
  g2a_set(&qq[0], PR_BQ);
  g1_set(pp[1], PR_NABP);
  g2a_set(&qq[1], BN_GEN2);
  pair_row(bad, "bilinear [(a G1, b G2), (-(a b) G1, G2)]", pair_check(&d, 2u, (const uint32_t(*)[2][8])pp, qq), 1);
  g1_set(pp[1], PR_NAB1P);
  pair_row(bad, "bilinear [(a G1, b G2), (-(a b + 1) G1, G2)]", pair_check(&d, 2u, (const uint32_t(*)[2][8])pp, qq), 0);
  g1_set(pp[1], PR_NABP);
  g1_set(pp[2], BN_GEN1);
  g2a_set(&qq[2], BN_GEN2);
  fp_neg(&d, pp[2][1], pp[3][1]);
  fe_copy(pp[3][0], pp[2][0]);
  qq[3] = qq[2];
  pair_row(bad, "pairCheck 4 pairs", pair_check(&d, 4u, (const uint32_t(*)[2][8])pp, qq), 1);
  pt_add(&d, qq[2].y.a, BN_ONE, qq[2].y.a);
  pair_row(bad, "G2 off the curve", g2_valid(&d, &qq[2]), 0);
  qq[2] = qq[3];
  pp[2][1][0] ^= 1u;
  pair_row(bad, "G1 off the curve", g1_valid(&d, pp[2][0], pp[2][1]), 0);
  f2_zero(&qq[0].x);
  f2_zero(&qq[0].y);
  g1_set(pp[0], BN_GEN1);
  pair_row(bad, "pairCheck [(G1, inf)]", pair_check(&d, 1u, (const uint32_t(*)[2][8])pp, qq), 1);
  /* EIP-197 known answers (revm-precompile 32.1.0, test_bn254_pair). */
  g2a_set(&qq[0], PR_E1Q[0]);
  g2a_set(&qq[1], PR_E1Q[1]);
  pair_row(bad, "EIP-197 E1 2 pairs", pair_check(&d, 2u, PR_E1P, qq), 1);
  pair_row(bad, "EIP-197 E1 first pair only", pair_check(&d, 1u, PR_E1P, qq), 0);
  g2a_set(&qq[0], PR_E1Q[1]);
  g2a_set(&qq[1], PR_E1Q[0]);
  pair_row(bad, "EIP-197 E1 G2 points swapped", pair_check(&d, 2u, PR_E1P, qq), 0);
  g2a_set(&qq[0], PR_E2Q[0]);
  pair_row(bad, "EIP-197 E2 off the curve", pair_check(&d, 1u, PR_E2P, qq), 0);
  g2a_set(&qq[0], PR_E3Q[0]);
  pair_row(bad, "EIP-197 E3 (inf, Q)", pair_check(&d, 1u, PR_E3P, qq), 1);
  g2a_set(&qq[0], PR_E4Q[0]);
  pair_row(bad, "EIP-197 E4 (P, inf)", pair_check(&d, 1u, PR_E4P, qq), 1);
  /* Bilinearity in Fp12: e(a G1, G2) = e(G1, a G2) = e(G1, G2)^a. */
  g2a_set(&qq[0], BN_GEN2);
  miller(&d, BN_GEN1[0], BN_GEN1[1], &qq[0], &m);
  final_exp(&d, &m, &x);
  f12_pow(&d, &x, PR_EA_RAW, PR_EA_BITS, &z);
  miller(&d, PR_EAP[0], PR_EAP[1], &qq[0], &m);
  final_exp(&d, &m, &y);
  pair_row(bad, "e(a G1, G2) = e(G1, G2)^a", f12_same(&y, &z), 1);
  g2a_set(&qq[1], PR_EAQ);
  pair_row(bad, "a G2 valid", g2_valid(&d, &qq[1]), 1);
  miller(&d, BN_GEN1[0], BN_GEN1[1], &qq[1], &m);
  final_exp(&d, &m, &y);
  pair_row(bad, "e(G1, a G2) = e(G1, G2)^a", f12_same(&y, &z), 1);
}

/* The pairCheckC rows (S8b, USER ruling Q-S8b-1 (a)): the C builtin
   pairCheckC on the generators (pcYes and pcNo of test/crypto/crypto.lang).
   Then the rows of the guest pairing spec (S8b 4b, test/crypto/pair-spec.lang),
   which check the spec against itself. Each row gives 1. */
/* The digest of a tower value (S8b 4c): h = rotl (h, 5) xor w for each word
   w, limb 0 first, F2 a before b, F6 a, b, c, F12 a before b. For a Miller
   step, the digest of T (x, y, z) continues with f. The guest dgF12 and dgMs
   of test/crypto/pair-spec.lang are the same. Each value row prints the
   digest of the C value, and the guest row must give the same word. */
static uint32_t f2_dig(uint32_t h, const F2 *z) {
  uint32_t i;
  for (i = 0u; i < 8u; i++)
    h = w_rotl(h, 5u) ^ z->a[i];
  for (i = 0u; i < 8u; i++)
    h = w_rotl(h, 5u) ^ z->b[i];
  return h;
}

static uint32_t f6_dig(uint32_t h, const F6 *x) {
  return f2_dig(f2_dig(f2_dig(h, &x->a), &x->b), &x->c);
}

static uint32_t f12_dig(uint32_t h, const F12 *x) {
  return f6_dig(f6_dig(h, &x->a), &x->b);
}

static uint32_t ms_dig(const G2P3 *t, const F12 *f) {
  return f12_dig(f2_dig(f2_dig(f2_dig(0u, &t->x), &t->y), &t->z), f);
}

/* The short chain rows of test/crypto/pair-spec.lang (S8b 4b): g2MulBits,
   f12PowFrom and millerBits walk a few bits (the top bit first), and each
   row prints the digest of the result. */
static uint32_t g2_dig(uint32_t h, const G2P3 *a) {
  return f2_dig(f2_dig(f2_dig(h, &a->x), &a->y), &a->z);
}

static void g2_walk(const PtMod *d, const G2A *q, uint32_t e, uint32_t bits, G2P3 *t) {
  G2P3 u;
  uint32_t i;
  f2_zero(&t->x);
  f2_zero(&t->y);
  f2_zero(&t->z);
  for (i = bits; i > 0u; i--) {
    g2_dbl(d, t, &u);
    *t = u;
    if (bit_at(&e, i - 1u)) {
      g2_madd(d, t, q, &u);
      *t = u;
    }
  }
}

static void chain_rows(const PtMod *d, const G2A *q, const uint32_t *px, const uint32_t *py, const F12 *a, const F12 *b) {
  G2P3 t;
  F12 u;
  F12 w;
  F12 f;
  uint32_t e;
  uint32_t i;
  g2_walk(d, q, 0x0u, 3u, &t);
  printf("test/crypto/pair-spec spZeroD => 0x%08xw\n", (unsigned)g2_dig(0u, &t));
  g2_walk(d, q, 0x0bu, 5u, &t);
  printf("test/crypto/pair-spec spG2MulD => 0x%08xw\n", (unsigned)g2_dig(0u, &t));
  /* f12PowFrom: b^(2^5) a^19 for the 5 bits 10011. */
  e = 0x13u;
  f12_pow(d, a, &e, 5u, &u);
  w = *b;
  for (i = 0u; i < 5u; i++)
    f12_mul(d, &w, &w, &w);
  f12_mul(d, &w, &u, &f);
  printf("test/crypto/pair-spec spPowD => 0x%08xw\n", (unsigned)f12_dig(0u, &f));
  /* millerBits on the 3 bits 101 from T = (G2, 1) and f = 1 at P = G1. */
  e = 0x5u;
  t.x = q->x;
  t.y = q->y;
  f2_one(&t.z);
  f12_one(&f);
  for (i = 3u; i > 0u; i--) {
    miller_dbl(d, &t, px, py, &f);
    if (bit_at(&e, i - 1u))
      miller_add(d, &t, q, px, py, &f);
  }
  printf("test/crypto/pair-spec spMillD => 0x%08xw\n", (unsigned)ms_dig(&t, &f));
}

static void pairc_rows(void) {
  static const char *const SPEC_ROWS[] = {"spInv2", "spInv12", "spFrob2", "spFrob6", "spG1V", "spG1N", "spG2N", "spMStep"};
  PtMod d;
  G2A q;
  G2P3 t;
  F2 one;
  F12 x;
  F12 y;
  F12 z;
  uint32_t p[2][8];
  size_t i;
  printf("test/crypto/crypto pcYes => 1\n");
  printf("test/crypto/crypto pcNo => 1\n");
  for (i = 0u; i < sizeof SPEC_ROWS / sizeof SPEC_ROWS[0]; i++)
    printf("test/crypto/pair-spec %s => 1\n", SPEC_ROWS[i]);
  /* spA12 and spB12 of test/crypto/pair-spec.lang, from G2 and 1. */
  fe_copy(d.m, BN_P);
  d.n0 = BN_N0;
  g2a_set(&q, BN_GEN2);
  g1_set(p, BN_GEN1);
  f2_one(&one);
  x.a.a = q.x;
  x.a.b = q.y;
  x.a.c = one;
  x.b.a = q.y;
  x.b.b = q.x;
  f2_add(&d, &q.x, &q.y, &x.b.c);
  y.a.a = q.y;
  y.a.b = one;
  y.a.c = q.x;
  f2_add(&d, &q.x, &one, &y.b.a);
  y.b.b = q.y;
  y.b.c = q.x;
  f12_mul(&d, &x, &y, &z);
  printf("test/crypto/pair-spec spMulD => 0x%08xw\n", (unsigned)f12_dig(0u, &z));
  f12_frob(&d, &x, &z);
  printf("test/crypto/pair-spec spFrobD => 0x%08xw\n", (unsigned)f12_dig(0u, &z));
  /* One Miller doubling step from T = (G2, 1) and f = 1 at P = G1, then one
     Miller addition step of G2. */
  t.x = q.x;
  t.y = q.y;
  t.z = one;
  f12_one(&z);
  miller_dbl(&d, &t, p[0], p[1], &z);
  printf("test/crypto/pair-spec spDblD => 0x%08xw\n", (unsigned)ms_dig(&t, &z));
  miller_add(&d, &t, &q, p[0], p[1], &z);
  printf("test/crypto/pair-spec spAddD => 0x%08xw\n", (unsigned)ms_dig(&t, &z));
  chain_rows(&d, &q, p[0], p[1], &x, &y);
}

/* The test fixtures (S8a). fxKeyK is K G for the secret key K (1 to 5). A
   message is the tag T (the guest `hsOf T`) or hsMsg (T, X, Y) = poseidon2
   (T, poseidon2 (X, Y)) in Fr, each value not in Montgomery form. X is the 64 hex digits XHEX
   when XHEX is not NULL: StA and StL sign the cm FX_CM of the golden elig
   statement (test/noir/elig), the cm of the examples (S8b). Each key in
   KEYS (bit K - 1 for the key K) signs the message with sc_sign. `cryptotool
   fixtures` prints the message words and the domain defs. The rows give
   hsMsg to the guest hsMsg and each signature to the guest schnorrCheck (the
   ck rows of test/crypto/crypto.lang). */
#define FX_KEYS 5u
#define FX_HS_KAT "1ab48afe922b8a4977f312071f685e3384546dd6f6ca941f62f277b4c5c9f677"
/* The cm of the golden elig statement (test/noir/elig/public.json). */
#define FX_CM "12f8384c816e12687e32287a0243fee448a8be3d5c759e15761babe975aef4d9"

typedef struct {
  const char *name;
  int hashed;
  uint64_t t;
  uint64_t x;
  uint64_t y;
  unsigned keys;
  const char *xhex;
} FxMsg;

static const FxMsg FX_MSGS[] = {
    {"Hs2", 0, 2u, 0u, 0u, 0x1fu, NULL},
    {"Hs3", 0, 3u, 0u, 0u, 0x04u, NULL},
    {"StA", 1, 0u, 0u, 1u, 0x07u, FX_CM},
    {"StL", 1, 0u, 0u, 2u, 0x07u, FX_CM},
    {"OpA", 1, 1u, 1u, 2u, 0x0eu, NULL},
    {"OpE", 1, 1u, 1u, 1u, 0x0eu, NULL},
    {"AmA", 1, 2u, 0u, 0u, 0x0fu, NULL},
};
#define FX_MSG_COUNT (sizeof FX_MSGS / sizeof FX_MSGS[0])

/* H = element 0 of the permutation of (0, X, Y). */
static void hs_pair(const PoseidonParams *p, Fe *h, const Fe *x, const Fe *y) {
  Fe s[POSEIDON_WIDTH];
  memset(s, 0, sizeof s);
  s[1] = *x;
  s[2] = *y;
  poseidon_perm(s, p);
  *h = s[0];
}
/* OUT = hsMsg (T, X, Y) = poseidon2 (T, poseidon2 (X, Y)), as the guest. */
static void hs_msg(const PoseidonParams *p, Fe *out, const Fe *t, const Fe *x, const Fe *y) {
  Fe inner;
  hs_pair(p, &inner, x, y);
  hs_pair(p, out, t, &inner);
}
static void fx_msg(const PoseidonParams *p, Fe *m, const FxMsg *g) {
  Fe t;
  Fe x;
  Fe y;
  fe_from_u64(&t, g->t);
  fe_from_u64(&x, g->x);
  fe_from_u64(&y, g->y);
  if (g->xhex != NULL)
    fe_hex(&x, g->xhex);
  *m = t;
  if (g->hashed)
    hs_msg(p, m, &t, &x, &y);
}
static void fx_point(const char *name, const G1 *p) {
  printf("def %s : Pt1 := pt1 (fe", name);
  put_fe(&p->x);
  printf(") (fe");
  put_fe(&p->y);
  printf(")\n");
}
static int fixture_data(void) {
  PoseidonParams p;
  G1 g;
  G1 pk;
  G1 r;
  Fe x;
  Fe s;
  Fe m;
  char name[32];
  size_t i;
  unsigned k;
  poseidon_params(&p, &BN254_FR);
  sc_gen(&g);
  for (i = 0u; i < FX_MSG_COUNT; i++) {
    fx_msg(&p, &m, &FX_MSGS[i]);
    printf("msg %s", FX_MSGS[i].name);
    put_fe(&m);
    printf("\n");
  }
  for (k = 1u; k <= FX_KEYS; k++) {
    fe_from_u64(&x, k);
    sc_mul(&pk, &g, &x);
    snprintf(name, sizeof name, "fxKey%u", k);
    fx_point(name, &pk);
  }
  for (i = 0u; i < FX_MSG_COUNT; i++) {
    fx_msg(&p, &m, &FX_MSGS[i]);
    for (k = 1u; k <= FX_KEYS; k++)
      if (((FX_MSGS[i].keys >> (k - 1u)) & 1u) != 0u) {
        sc_sign(&pk, &r, &s, k, &m);
        snprintf(name, sizeof name, "fx%sK%uR", FX_MSGS[i].name, k);
        fx_point(name, &r);
        printf("def fx%sK%uS : Fe := fe", FX_MSGS[i].name, k);
        put_fe(&s);
        printf("\n");
      }
  }
  return 0;
}
static void hs_row(const PoseidonParams *p, const Fe *t, const Fe *x, const Fe *y, Fe *h) {
  hs_msg(p, h, t, x, y);
  printf("test/crypto/crypto hsMsgRow");
  put_fe(t);
  put_fe(x);
  put_fe(y);
  put_fe(h);
  printf(" => 1\n");
}
/* hsMsg (0, 1, 2) (the known answer of the guest) and hsMsg on random
   values, then one row for each fixture signature, each checked first by the
   C verifier. */
static void fixture_rows(Rng *rng, unsigned *bad) {
  PoseidonParams p;
  G1 pk;
  G1 r;
  Fe s;
  Fe m;
  Fe t;
  Fe x;
  Fe y;
  Fe h;
  size_t i;
  unsigned k;
  poseidon_params(&p, &BN254_FR);
  fe_from_u64(&t, 0u);
  fe_from_u64(&x, 1u);
  fe_from_u64(&y, 2u);
  hs_row(&p, &t, &x, &y, &h);
  if (!fe_is_hex(&h, FX_HS_KAT)) {
    fputs("cryptotool: hsMsg (0, 1, 2) is not the guest value\n", stderr);
    (*bad)++;
  }
  fe_random(&t, rng, &BN254_FR);
  fe_random(&x, rng, &BN254_FR);
  fe_random(&y, rng, &BN254_FR);
  hs_row(&p, &t, &x, &y, &h);
  for (i = 0u; i < FX_MSG_COUNT; i++) {
    fx_msg(&p, &m, &FX_MSGS[i]);
    for (k = 1u; k <= FX_KEYS; k++)
      if (((FX_MSGS[i].keys >> (k - 1u)) & 1u) != 0u) {
        sc_sign(&pk, &r, &s, k, &m);
        if (!sc_verify(&pk, &r, &s, &m)) {
          fputs("cryptotool: a fixture signature is not valid\n", stderr);
          (*bad)++;
        }
        printf("test/crypto/crypto ck%sK%u => inr unit\n", FX_MSGS[i].name, k);
      }
  }
}

/* The Noir golden files (S8b, USER rulings Q-S8b-6 (a) and Q-S8b-12 (a)).
   `cryptotool noir NAME DIR` reads the snarkjs files verification_key.json,
   proof.json and public.json of DIR. Each value is a decimal string: a
   coordinate is below p, a public input is below r. snarkjs writes an Fp2
   value as [re, im] (F2 a, b) and a point with its z (1, or [1, 0] for G2).
   The tool checks the Groth16 equation e(-A, B) e(alpha, beta) e(vk_x,
   gamma) e(C, delta) = 1 with vk_x = IC_0 + x_1 IC_1 + ... + x_n IC_n
   (pair_check also checks the points: G1 on the curve, G2 on the twist and
   in the subgroup). Then it prints the domain data defs of NAME, each value
   not in Montgomery form. A file or check failure gives rc 1. */
#define NR_TEXT_MAX 16384u
#define NR_PUB_MAX 8u

typedef struct {
  char text[NR_TEXT_MAX];
  size_t size;
  size_t at;
} NrFile;

typedef struct {
  uint32_t alpha[2][8];
  uint32_t beta[2][2][8];
  uint32_t gamma[2][2][8];
  uint32_t delta[2][2][8];
  uint32_t ic[NR_PUB_MAX + 1u][2][8];
  uint32_t a[2][8];
  uint32_t b[2][2][8];
  uint32_t c[2][8];
  uint32_t pub[NR_PUB_MAX][8];
  unsigned n;
} NrSet;

/* Reads DIR/FILE into F. 0 when the file is not there, is too large or has
   a 0 byte. */
static int nr_read(NrFile *f, const char *dir, const char *file) {
  char path[1024];
  FILE *h;
  int more;
  if (snprintf(path, sizeof path, "%s/%s", dir, file) >= (int)sizeof path)
    return 0;
  h = fopen(path, "rb");
  if (h == NULL)
    return 0;
  f->size = fread(f->text, 1u, NR_TEXT_MAX - 1u, h);
  f->text[f->size] = '\0';
  f->at = 0u;
  more = fgetc(h) != EOF || ferror(h) != 0;
  fclose(h);
  return !more && memchr(f->text, '\0', f->size) == NULL;
}

/* Moves F->at past the key KEY. 0 when KEY is not in F one time. */
static int nr_key(NrFile *f, const char *key) {
  char q[32];
  const char *s;
  if (snprintf(q, sizeof q, "\"%s\"", key) >= (int)sizeof q)
    return 0;
  s = strstr(f->text, q);
  if (s == NULL || strstr(s + 1, q) != NULL)
    return 0;
  f->at = (size_t)(s - f->text) + strlen(q);
  return 1;
}

/* Reads the next string of F as a decimal value below the modulus M into W
   (word 0 lowest). 0 when the string is not such a value. */
static int nr_dec(NrFile *f, const Modulus *m, uint32_t *w) {
  const char *s = strchr(f->text + f->at, '"');
  const char *e;
  const char *i;
  Fe x;
  unsigned k;
  uint64_t c;
  if (s == NULL)
    return 0;
  e = strchr(s + 1, '"');
  if (e == NULL || e == s + 1 || e - s > 79)
    return 0;
  memset(w, 0, 8u * sizeof *w);
  for (i = s + 1; i < e; i++) {
    if (*i < '0' || *i > '9')
      return 0;
    c = (uint64_t)(*i - '0');
    for (k = 0u; k < 8u; k++) {
      c += (uint64_t)w[k] * 10u;
      w[k] = (uint32_t)c;
      c >>= 32;
    }
    if (c != 0u)
      return 0;
  }
  f->at = (size_t)(e + 1 - f->text);
  memcpy(x.w, w, sizeof x.w);
  return below(&x, m);
}

/* Reads the next value of F. 0 when it is not V. */
static int nr_small(NrFile *f, uint32_t v) {
  uint32_t w[8];
  if (!nr_dec(f, &BN254_FP, w) || w[0] != v)
    return 0;
  w[0] = 0u;
  return fe_zero(w);
}

/* A G1 point [x, y, 1] into P. */
static int nr_g1(NrFile *f, uint32_t (*p)[8]) {
  return nr_dec(f, &BN254_FP, p[0]) && nr_dec(f, &BN254_FP, p[1]) && nr_small(f, 1u);
}

/* A G2 point [[x a, x b], [y a, y b], [1, 0]] into Q. */
static int nr_g2(NrFile *f, uint32_t (*q)[2][8]) {
  return nr_dec(f, &BN254_FP, q[0][0]) && nr_dec(f, &BN254_FP, q[0][1]) && nr_dec(f, &BN254_FP, q[1][0]) &&
         nr_dec(f, &BN254_FP, q[1][1]) && nr_small(f, 1u) && nr_small(f, 0u);
}

/* Moves F->at past N closing brackets, each after white space. */
static int nr_close(NrFile *f, unsigned n) {
  unsigned k;
  for (k = 0u; k < n; k++) {
    f->at += strspn(f->text + f->at, " \t\r\n");
    if (f->text[f->at] != ']')
      return 0;
    f->at++;
  }
  return 1;
}

/* The bare count after the key nPublic: 1 to NR_PUB_MAX. */
static int nr_count(NrFile *f, unsigned *n) {
  const char *s = f->text + f->at;
  s += strspn(s, " \t\r\n:");
  *n = 0u;
  while (*s >= '0' && *s <= '9' && *n <= NR_PUB_MAX) {
    *n = *n * 10u + (unsigned)(*s - '0');
    s++;
  }
  return *n >= 1u && *n <= NR_PUB_MAX;
}

/* Reads the 3 snarkjs files of DIR into S. IC has nPublic + 1 points and
   public.json has nPublic values. */
static int nr_load(NrSet *s, const char *dir) {
  static NrFile f;
  unsigned i;
  int ok;
  ok = nr_read(&f, dir, "verification_key.json") && strstr(f.text, "\"groth16\"") != NULL &&
       strstr(f.text, "\"bn128\"") != NULL && nr_key(&f, "nPublic") && nr_count(&f, &s->n) &&
       nr_key(&f, "vk_alpha_1") && nr_g1(&f, s->alpha) && nr_key(&f, "vk_beta_2") && nr_g2(&f, s->beta) &&
       nr_key(&f, "vk_gamma_2") && nr_g2(&f, s->gamma) && nr_key(&f, "vk_delta_2") && nr_g2(&f, s->delta) &&
       nr_key(&f, "IC");
  for (i = 0u; ok && i <= s->n; i++)
    ok = nr_g1(&f, s->ic[i]);
  ok = ok && nr_close(&f, 2u) && nr_read(&f, dir, "proof.json") && strstr(f.text, "\"groth16\"") != NULL &&
       strstr(f.text, "\"bn128\"") != NULL && nr_key(&f, "pi_a") && nr_g1(&f, s->a) && nr_key(&f, "pi_b") &&
       nr_g2(&f, s->b) && nr_key(&f, "pi_c") && nr_g1(&f, s->c) && nr_read(&f, dir, "public.json");
  for (i = 0u; ok && i < s->n; i++)
    ok = nr_dec(&f, &BN254_FR, s->pub[i]);
  return ok && nr_close(&f, 1u);
}

/* T = X R mod p, the Montgomery form of X (below p). R^2 mod p comes from
   the Montgomery one R mod p by 256 doublings. */
static void nr_mont(const PtMod *d, const uint32_t *x, uint32_t *t) {
  uint32_t r2[8];
  uint32_t u[8];
  unsigned k;
  fe_copy(r2, BN_ONE);
  for (k = 0u; k < 256u; k++) {
    pt_add(d, r2, r2, u);
    fe_copy(r2, u);
  }
  pt_fm(d, x, r2, t);
}

static void nr_u256(U256 *u, const uint32_t *w) {
  unsigned k;
  for (k = 0u; k < 4u; k++)
    u->limb[k] = (uint64_t)w[2u * k] | ((uint64_t)w[2u * k + 1u] << 32);
}

static void nr_words(uint32_t *w, const U256 *u) {
  unsigned k;
  for (k = 0u; k < 4u; k++) {
    w[2u * k] = (uint32_t)u->limb[k];
    w[2u * k + 1u] = (uint32_t)(u->limb[k] >> 32);
  }
}

static int nr_g1_of(G1 *g, const uint32_t (*p)[8]) {
  U256 x;
  U256 y;
  nr_u256(&x, p[0]);
  nr_u256(&y, p[1]);
  return g1_from_words(g, &x, &y);
}

static void nr_g2m(const PtMod *d, const uint32_t (*q)[2][8], G2A *t) {
  nr_mont(d, q[0][0], t->x.a);
  nr_mont(d, q[0][1], t->x.b);
  nr_mont(d, q[1][0], t->y.a);
  nr_mont(d, q[1][1], t->y.b);
}

/* 1 when e(-A, B) e(alpha, beta) e(vk_x, gamma) e(C, delta) = 1. Also checks
   that the Montgomery form of 1 is BN_ONE. */
static int nr_groth16(const NrSet *s) {
  PtMod d;
  G1 x;
  G1 t;
  G1 ic;
  U256 k;
  U256 u;
  U256 v;
  uint32_t w[8];
  uint32_t pp[4][2][8];
  G2A qq[4];
  unsigned i;
  int ok;
  fe_copy(d.m, BN_P);
  d.n0 = BN_N0;
  memset(w, 0, sizeof w);
  w[0] = 1u;
  nr_mont(&d, w, pp[0][0]);
  ok = fe_same(pp[0][0], BN_ONE) && nr_g1_of(&x, s->ic[0]);
  for (i = 0u; ok && i < s->n; i++) {
    nr_u256(&k, s->pub[i]);
    ok = nr_g1_of(&ic, s->ic[i + 1u]);
    if (ok) {
      g1_mul(&t, &ic, &k);
      g1_add(&x, &x, &t);
    }
  }
  if (!ok)
    return 0;
  g1_to_words(&u, &v, &x);
  nr_words(w, &u);
  nr_mont(&d, w, pp[2][0]);
  nr_words(w, &v);
  nr_mont(&d, w, pp[2][1]);
  nr_mont(&d, s->a[0], pp[0][0]);
  nr_mont(&d, s->a[1], w);
  fp_neg(&d, w, pp[0][1]);
  nr_mont(&d, s->alpha[0], pp[1][0]);
  nr_mont(&d, s->alpha[1], pp[1][1]);
  nr_mont(&d, s->c[0], pp[3][0]);
  nr_mont(&d, s->c[1], pp[3][1]);
  nr_g2m(&d, s->b, &qq[0]);
  nr_g2m(&d, s->beta, &qq[1]);
  nr_g2m(&d, s->gamma, &qq[2]);
  nr_g2m(&d, s->delta, &qq[3]);
  return pair_check(&d, 4u, (const uint32_t (*)[2][8])pp, qq);
}

static void nr_fe(const uint32_t *w) {
  unsigned k;
  printf("(fe");
  for (k = 0u; k < 8u; k++)
    put_word(w[k]);
  printf(")");
}

static void nr_pt1(const char *name, const char *part, const uint32_t (*p)[8]) {
  printf("def %s%s : Pt1 := pt1 ", name, part);
  nr_fe(p[0]);
  printf(" ");
  nr_fe(p[1]);
  printf("\n");
}

static void nr_pt2(const char *name, const char *part, const uint32_t (*q)[2][8]) {
  printf("def %s%s : Pt2 := pt2 (fp2 ", name, part);
  nr_fe(q[0][0]);
  printf(" ");
  nr_fe(q[0][1]);
  printf(") (fp2 ");
  nr_fe(q[1][0]);
  printf(" ");
  nr_fe(q[1][1]);
  printf(")\n");
}

static int noir_data(const char *name, const char *dir) {
  static NrSet set;
  const NrSet *s = &set;
  char part[16];
  unsigned i;
  unsigned k;
  if (name[0] < 'a' || name[0] > 'z' || strlen(name) > 24u ||
      strspn(name, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") != strlen(name)) {
    fputs("cryptotool: noir NAME: a lower case letter, then letters and digits\n", stderr);
    return 2;
  }
  if (!nr_load(&set, dir)) {
    fprintf(stderr, "cryptotool: %s: the snarkjs files are not valid\n", dir);
    return 1;
  }
  if (!nr_groth16(s)) {
    fprintf(stderr, "cryptotool: %s: the Groth16 check fails\n", dir);
    return 1;
  }
  nr_pt1(name, "VkAlpha", s->alpha);
  nr_pt2(name, "VkBeta", s->beta);
  nr_pt2(name, "VkGamma", s->gamma);
  nr_pt2(name, "VkDelta", s->delta);
  for (i = 0u; i <= s->n; i++) {
    snprintf(part, sizeof part, "VkIc%u", i);
    nr_pt1(name, part, s->ic[i]);
  }
  nr_pt1(name, "PrfA", s->a);
  nr_pt2(name, "PrfB", s->b);
  nr_pt1(name, "PrfC", s->c);
  for (i = 0u; i < s->n; i++) {
    printf("def %sPub%u : Fe := fe", name, i);
    for (k = 0u; k < 8u; k++)
      put_word(s->pub[i][k]);
    printf("\n");
  }
  return 0;
}

/* The elig statement of test/noir/elig (Prover.toml, Q-S8b-13 (c) and
   Q-S8b-14 (a)): C_u = hsMsg (7, rho_u, 0), the leaf hsMsg (3, a_u, C_u), the
   root over the Merkle path (tag 4; index bit 1: the node is the right
   child), hu = hsMsg (5, rho_u, AucID) and cm = hsMsg (6, tx, C_u) must be
   the 3 public inputs of test/noir/elig/public.json. */
#define EL_ROOT "29a72ffee5744ff770dc0c386fccee794d23537dbe07f625c6f142c201c257fa"
#define EL_HU "2c3462023699dcbef712fcd5bf6edbc43513b74ccec4260407a32114aa984ef2"
#define EL_CM FX_CM
#define EL_RHO 11u
#define EL_AUC 1u
#define EL_A 2u
#define EL_TX 13u
#define EL_BITS 0x4du
static const uint64_t EL_PATH[8] = {101u, 102u, 103u, 104u, 105u, 106u, 107u, 108u};

/* The second elig statement of test/noir/elig2 (Q-S8b-17 (a)): the same
   deposit and handle as test/noir/elig and a second bid. Only tx and cm
   change, thus root_s and hu are the EL_ values. */
#define EL2_CM "018c6232ff2c55441f25f371fc2c5f7de743492ab55d5cc1ed2e6bd82a433cec"
#define EL2_TX 14u

static void el_msg(const PoseidonParams *p, Fe *out, uint64_t t, const Fe *x, const Fe *y) {
  Fe f;
  fe_from_u64(&f, t);
  hs_msg(p, out, &f, x, y);
}

static void elig_rows(unsigned *bad) {
  PoseidonParams p;
  Fe x;
  Fe y;
  Fe cu;
  Fe cur;
  Fe sib;
  Fe h;
  unsigned i;
  poseidon_params(&p, &BN254_FR);
  fe_from_u64(&x, EL_RHO);
  fe_from_u64(&y, 0u);
  el_msg(&p, &cu, 7u, &x, &y);
  fe_from_u64(&x, EL_A);
  el_msg(&p, &cur, 3u, &x, &cu);
  for (i = 0u; i < 8u; i++) {
    fe_from_u64(&sib, EL_PATH[i]);
    if (((EL_BITS >> i) & 1u) != 0u)
      el_msg(&p, &h, 4u, &sib, &cur);
    else
      el_msg(&p, &h, 4u, &cur, &sib);
    cur = h;
  }
  pair_row(bad, "elig root_s", fe_is_hex(&cur, EL_ROOT), 1);
  fe_from_u64(&x, EL_RHO);
  fe_from_u64(&y, EL_AUC);
  el_msg(&p, &h, 5u, &x, &y);
  pair_row(bad, "elig hu", fe_is_hex(&h, EL_HU), 1);
  fe_from_u64(&x, EL_TX);
  el_msg(&p, &h, 6u, &x, &cu);
  pair_row(bad, "elig cm", fe_is_hex(&h, EL_CM), 1);
  fe_from_u64(&x, EL2_TX);
  el_msg(&p, &h, 6u, &x, &cu);
  pair_row(bad, "elig2 cm", fe_is_hex(&h, EL2_CM), 1);
}

/* The Groth16 rows of test/crypto/g16.lang (S8b 4d). `cryptotool noir NAME
   DIR` checks the golden proofs of test/noir in C. These rows check that the
   guest verifier gives the same result: inr unit for a valid proof, inl unit
   for a changed public, key or proof. */
typedef struct {
  const char *name;
  int valid;
} G16Row;

static void g16_rows(void) {
  static const G16Row ROWS[] = {
    {"g16Elig", 1},
    {"g16Outcome", 1},
    {"g16Share", 1},
    {"g16PubOne", 0},
    {"g16PubAlias", 0},
    {"g16Short", 0},
    {"g16SwapAC", 0},
    {"g16Cross", 0},
    {"g16Elig2", 1},
    {"g16Elig2Cm", 0},
    {"g16NonCanon", 0},
    {"vrfElig", 1},
    {"vrfElig2", 1},
    {"vrfEligCm", 0},
    {"vrfOutcome", 1},
    {"vrfOutPrice", 0},
    {"vrfShare", 1},
    {"vrfShareIdx", 0},
  };
  size_t i;
  for (i = 0u; i < sizeof ROWS / sizeof ROWS[0]; i++)
    printf("test/crypto/g16 %s => %s unit\n", ROWS[i].name, ROWS[i].valid ? "inr" : "inl");
}

int main(int argc, char **argv) {
  Rng rng = {0x5338676176656c31ULL};
  const Field fr = {"fr", &BN254_FR};
  const Field fp = {"fp", &BN254_FP};
  unsigned bad = 0u;
  if (argc == 2 && strcmp(argv[1], "poseidon") == 0)
    return poseidon_data();
  if (argc == 2 && strcmp(argv[1], "fixtures") == 0)
    return fixture_data();
  if (argc == 4 && strcmp(argv[1], "noir") == 0)
    return noir_data(argv[2], argv[3]);
  if (argc != 1) {
    fputs("usage: cryptotool [poseidon | fixtures | noir NAME DIR]\n", stderr);
    return 2;
  }
  word_rows(&rng);
  field_rows(&fr, FR_LESS_ONE, FR_LESS_TWO, &rng, &bad);
  field_rows(&fp, FP_LESS_ONE, FP_LESS_TWO, &rng, &bad);
  curve_rows(&rng, &bad);
  mulc_rows(&fr, &rng);
  mulc_rows(&fp, &rng);
  keccak_rows(&rng, &bad);
  poseidon_rows(&rng, &bad);
  schnorr_rows(&rng, &bad);
  ptmulc_rows(&rng);
  pairo_rows(&bad);
  pairc_rows();
  fixture_rows(&rng, &bad);
  elig_rows(&bad);
  g16_rows();
  if (bad != 0u) {
    fprintf(stderr, "cryptotool: %u rows of the C oracle are wrong\n", bad);
    return 1;
  }
  return 0;
}
