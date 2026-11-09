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

/* The test fixtures (S8a). fxKeyK is K G for the secret key K (1 to 5). A
   message is the tag T (the guest `hsOf T`) or hsMsg (T, X, Y) = poseidon2
   (T, poseidon2 (X, Y)) in Fr, each value not in Montgomery form. Each key in
   KEYS (bit K - 1 for the key K) signs the message with sc_sign. `cryptotool
   fixtures` prints the message words and the domain defs. The rows give
   hsMsg to the guest hsMsg and each signature to the guest schnorrCheck (the
   ck rows of test/crypto/crypto.lang). */
#define FX_KEYS 5u
#define FX_HS_KAT "1ab48afe922b8a4977f312071f685e3384546dd6f6ca941f62f277b4c5c9f677"

typedef struct {
  const char *name;
  int hashed;
  uint64_t t;
  uint64_t x;
  uint64_t y;
  unsigned keys;
} FxMsg;

static const FxMsg FX_MSGS[] = {
    {"Hs2", 0, 2u, 0u, 0u, 0x1fu},
    {"Hs3", 0, 3u, 0u, 0u, 0x04u},
    {"StA", 1, 0u, 1u, 1u, 0x07u},
    {"StL", 1, 0u, 1u, 2u, 0x07u},
    {"OpA", 1, 1u, 1u, 2u, 0x0eu},
    {"OpE", 1, 1u, 1u, 1u, 0x0eu},
    {"AmA", 1, 2u, 0u, 0u, 0x0fu},
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

int main(int argc, char **argv) {
  Rng rng = {0x5338676176656c31ULL};
  const Field fr = {"fr", &BN254_FR};
  const Field fp = {"fp", &BN254_FP};
  unsigned bad = 0u;
  if (argc == 2 && strcmp(argv[1], "poseidon") == 0)
    return poseidon_data();
  if (argc == 2 && strcmp(argv[1], "fixtures") == 0)
    return fixture_data();
  if (argc != 1) {
    fputs("usage: cryptotool [poseidon | fixtures]\n", stderr);
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
  fixture_rows(&rng, &bad);
  if (bad != 0u) {
    fprintf(stderr, "cryptotool: %u rows of the C oracle are wrong\n", bad);
    return 1;
  }
  return 0;
}
