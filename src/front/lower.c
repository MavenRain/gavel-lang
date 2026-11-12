/* The lowering from checked values to the first-order IR. SHARED by the tcc
   kits.

   Each entry is evaluated on variables for its parameters. The normal form
   is the residual program, and this file writes it as IR statements. Each
   value becomes an Operand: one word (see src/ir.h). An eliminator on a heap
   value loads the tag and the fields of the block. A fold over a list or a
   linear family walks the spine into a reversed chain, then folds the chain
   from its last node. The chain is a list: a block [1, word, rest]. */
#include "front/lower.h"

#include <string.h>

#include "front/check.h"

#define IR_PRINT_DEPTH_MAX 4000u

/* A value at run time: its word, a constant or a local. */
typedef struct {
  const IrExpr *w;
} Operand;

/* The local of a result. */
typedef struct {
  uint32_t w;
} Slot;

typedef struct Buf Buf;

/* The operand of a value lowered earlier: one lowering per Value pointer
   (flag 51). A block finds the entries of its own memo and of the blocks it
   is placed in (flag 52), so a local set in a sibling arm is never read. */
typedef struct {
  const Value *key;
  Operand op;
} Memo;

struct Buf {
  const IrStmt **items;
  size_t count;
  size_t cap;
  const Buf *parent; /* the block this one is placed in, or NULL */
  Memo *memo;        /* open addressing on the Value pointer */
  size_t memo_count;
  size_t memo_cap; /* 0 or a power of two */
};

/* S9a (D26): the largest depth of the run-time elims that test their `ih`
   variable at one time. */
#define LOW_WATCH_MAX 8u

typedef struct {
  Machine *m;
  uint32_t local_count;
  Operand *levels; /* the operand of each de Bruijn level */
  uint32_t level_count;
  uint32_t level_cap;
  uint32_t watch[LOW_WATCH_MAX]; /* the levels of the `ih` variables under test */
  uint32_t watch_count;
  uint32_t watch_hit; /* bit I: the lowering read the level watch[I] */
  uint32_t host; /* S9a (D4): 1 in a host-form entry */
} Low;

static int oom(Low *l) {
  return diag_fail(l->m->diag, "OOM", l->m->def, "out of memory");
}

static int internal(Low *l, const char *what) {
  return diag_fail(l->m->diag, "INTERNAL", l->m->def, "the lowering found %s", what);
}

/* An operation that the Wasm host does not lower (S3). */
static int refuse(Low *l, const char *what) {
  return diag_fail(l->m->diag, "REFUSE_LOWER", l->m->def, "the Wasm host does not lower %s", what);
}

static IrExpr *expr_alloc(Low *l, IrExprKind kind) {
  IrExpr *e = arena_alloc(l->m->arena, sizeof *e);
  if (e == NULL)
    oom(l);
  else
    e->kind = kind;
  return e;
}

static const IrExpr *ex_const(Low *l, uint64_t value) {
  IrExpr *e = expr_alloc(l, IR_EXPR_CONST);
  if (e != NULL)
    e->value = value;
  return e;
}

static const IrExpr *ex_local(Low *l, uint32_t local) {
  IrExpr *e = expr_alloc(l, IR_EXPR_LOCAL);
  if (e != NULL)
    e->local = local;
  return e;
}

static const IrExpr *ex_bin(Low *l, IrOp op, const IrExpr *a, const IrExpr *b) {
  IrExpr *e = a == NULL || b == NULL ? NULL : expr_alloc(l, IR_EXPR_BINARY);
  if (e == NULL)
    return NULL;
  e->op = op;
  e->left = a;
  e->right = b;
  return e;
}

/* Word WORD of the block at ADDR. */
static const IrExpr *ex_load(Low *l, const IrExpr *base, uint32_t word) {
  IrExpr *e = base == NULL ? NULL : expr_alloc(l, IR_EXPR_LOAD);
  if (e == NULL)
    return NULL;
  e->left = base;
  e->field = word;
  return e;
}

static int is_const(const IrExpr *e, uint64_t value) {
  return e != NULL && e->kind == IR_EXPR_CONST && e->value == value;
}

static Operand operand(const IrExpr *w) {
  Operand o;
  o.w = w;
  return o;
}

static int give(Low *l, Operand *out, uint64_t w) {
  *out = operand(ex_const(l, w));
  return out->w != NULL;
}

static uint32_t local_new(Low *l) {
  return l->local_count++;
}

static Slot slot_new(Low *l) {
  Slot s;
  s.w = local_new(l);
  return s;
}

static Operand slot_get(Low *l, Slot s) {
  return operand(ex_local(l, s.w));
}

/* A new block, placed in PARENT (NULL for the body of an entry). */
static Buf buf_under(const Buf *parent) {
  Buf b;
  memset(&b, 0, sizeof b);
  b.parent = parent;
  return b;
}

/* The slot of KEY in MEMO (CAP entries): its entry, or the free slot
   where it goes. */
static size_t memo_slot(const Memo *memo, size_t cap, const Value *key) {
  size_t i = (((size_t)(uintptr_t)key) >> 4) * 2654435761u & (cap - 1u);
  while (memo[i].key != NULL && memo[i].key != key)
    i = (i + 1u) & (cap - 1u);
  return i;
}

/* 1 and OUT when V was lowered into B or into a block B is placed in. */
static int memo_find(const Buf *b, const Value *v, Operand *out) {
  size_t i;
  for (; b != NULL; b = b->parent) {
    if (b->memo_cap == 0)
      continue;
    i = memo_slot(b->memo, b->memo_cap, v);
    if (b->memo[i].key == v) {
      *out = b->memo[i].op;
      return 1;
    }
  }
  return 0;
}

static int memo_grow(Low *l, Buf *b) {
  size_t cap = b->memo_cap == 0 ? 16u : b->memo_cap * 2u;
  Memo *grown = arena_alloc(l->m->arena, cap * sizeof *grown);
  size_t i;
  if (grown == NULL)
    return oom(l);
  memset(grown, 0, cap * sizeof *grown);
  for (i = 0; i < b->memo_cap; i++)
    if (b->memo[i].key != NULL)
      grown[memo_slot(grown, cap, b->memo[i].key)] = b->memo[i];
  b->memo = grown;
  b->memo_cap = cap;
  return 1;
}

/* Records V as O in the memo of B. The table stays at most half full. */
static int memo_put(Low *l, Buf *b, const Value *v, Operand o) {
  size_t i;
  if (2u * (b->memo_count + 1u) > b->memo_cap && !memo_grow(l, b))
    return 0;
  i = memo_slot(b->memo, b->memo_cap, v);
  if (b->memo[i].key == NULL)
    b->memo_count++;
  b->memo[i].key = v;
  b->memo[i].op = o;
  return 1;
}

/* A value with a lowering worth one memo entry: an operation or a stuck
   operation (flag 54). A number, a type, a variable and a closure are
   constants, a level or a diagnostic. */
static int memo_kind(const Value *v) {
  return v->kind == VAL_OP || v->kind == VAL_STUCK;
}

static int buf_grow(Low *l, Buf *b) {
  size_t cap = b->cap == 0 ? 8u : b->cap * 2u;
  const IrStmt **grown = arena_alloc(l->m->arena, cap * sizeof *grown);
  size_t i;
  if (grown == NULL)
    return oom(l);
  for (i = 0; i < b->count; i++)
    grown[i] = b->items[i];
  b->items = grown;
  b->cap = cap;
  return 1;
}

static int emit(Low *l, Buf *b, const IrStmt *s) {
  if (s == NULL || (b->count == b->cap && !buf_grow(l, b)))
    return 0;
  b->items[b->count++] = s;
  return 1;
}

static int splice(Low *l, Buf *b, const Buf *in) {
  size_t i;
  for (i = 0; i < in->count; i++)
    if (!emit(l, b, in->items[i]))
      return 0;
  return 1;
}

static IrBlock block_of(const Buf *b) {
  IrBlock out;
  out.items = b->items;
  out.count = b->count;
  return out;
}

static IrStmt *stmt_new(Low *l, IrStmtKind kind) {
  IrStmt *s = arena_alloc(l->m->arena, sizeof *s);
  if (s == NULL)
    oom(l);
  else
    s->kind = kind;
  return s;
}

/* A statement that reads the expression E. */
static IrStmt *stmt_expr(Low *l, IrStmtKind kind, const IrExpr *e) {
  IrStmt *s = e == NULL ? NULL : stmt_new(l, kind);
  if (s != NULL)
    s->expr = e;
  return s;
}

static int set(Low *l, Buf *b, uint32_t local, const IrExpr *e) {
  IrStmt *s = stmt_expr(l, IR_STMT_SET, e);
  if (s != NULL)
    s->local = local;
  return emit(l, b, s);
}

/* DST := SRC. */
static int store(Low *l, Buf *b, Slot dst, Operand src) {
  return src.w != NULL && set(l, b, dst.w, src.w);
}

/* E as a constant or a local. */
static const IrExpr *atom(Low *l, Buf *b, const IrExpr *e) {
  uint32_t t;
  if (e == NULL || e->kind == IR_EXPR_CONST || e->kind == IR_EXPR_LOCAL)
    return e;
  t = local_new(l);
  return set(l, b, t, e) ? ex_local(l, t) : NULL;
}

static int st_if(Low *l, Buf *b, const IrExpr *c, const Buf *yes, const Buf *no) {
  IrStmt *s = stmt_expr(l, IR_STMT_IF, c);
  if (s == NULL)
    return 0;
  s->body = block_of(yes);
  s->otherwise = block_of(no);
  return emit(l, b, s);
}

/* IF F {YES} ELSE {NO}. A constant F keeps one block. */
static int if_flag(Low *l, Buf *b, const IrExpr *f, const Buf *yes, const Buf *no) {
  if (f == NULL)
    return 0;
  if (is_const(f, 0) || is_const(f, 1))
    return splice(l, b, is_const(f, 1) ? yes : no);
  return st_if(l, b, f, yes, no);
}

static int st_switch(Low *l, Buf *b, const IrExpr *e, const IrBlock *arms, size_t n) {
  IrStmt *s = stmt_expr(l, IR_STMT_SWITCH, e);
  if (s == NULL)
    return 0;
  s->arms = arms;
  s->arm_count = n;
  return emit(l, b, s);
}

/* REPEAT COUNT {BODY}, with a new counter local. */
static int st_repeat(Low *l, Buf *b, const IrExpr *count, const Buf *body) {
  IrStmt *s = stmt_expr(l, IR_STMT_REPEAT, count);
  if (s == NULL)
    return 0;
  s->counter = local_new(l);
  s->body = block_of(body);
  return emit(l, b, s);
}

/* WHILE COUNT, LOCAL {BODY}, with a new counter local. */
static int st_while(Low *l, Buf *b, const IrExpr *count, uint32_t local, const Buf *body) {
  IrStmt *s = stmt_expr(l, IR_STMT_WHILE, count);
  if (s == NULL)
    return 0;
  s->counter = local_new(l);
  s->local = local;
  s->body = block_of(body);
  return emit(l, b, s);
}

/* A new block [TAG, the word of each of the N operands]. */
static int block_ops(Low *l, Buf *b, uint64_t tag, const Operand *ops, uint32_t n, Operand *out) {
  const IrExpr **fields = arena_alloc(l->m->arena, ((size_t)n + 1u) * sizeof *fields);
  IrStmt *s = stmt_new(l, IR_STMT_ALLOC);
  uint32_t i;
  if (fields == NULL || s == NULL)
    return fields == NULL ? oom(l) : 0;
  fields[0] = ex_const(l, tag);
  for (i = 0; i < n; i++)
    fields[1u + i] = ops[i].w;
  s->local = local_new(l);
  s->fields = fields;
  s->field_count = (size_t)n + 1u;
  *out = operand(ex_local(l, s->local));
  return fields[0] != NULL && emit(l, b, s);
}

/* The operand of field K of the block at ADDR. */
static Operand field_op(Low *l, const IrExpr *base, uint32_t k) {
  return operand(ex_load(l, base, 1u + k));
}

/* Field K of the block at ADDR, copied into a new local. */
static int field_at(Low *l, Buf *b, const IrExpr *base, uint32_t k, Operand *out) {
  Slot s = slot_new(l);
  *out = slot_get(l, s);
  return store(l, b, s, field_op(l, base, k));
}

static int levels_grow(Low *l) {
  uint32_t cap = l->level_cap == 0 ? 64u : l->level_cap * 2u;
  Operand *grown = arena_alloc(l->m->arena, (size_t)cap * sizeof *grown);
  uint32_t i;
  if (grown == NULL)
    return oom(l);
  for (i = 0; i < l->level_count; i++)
    grown[i] = l->levels[i];
  l->levels = grown;
  l->level_cap = cap;
  return 1;
}

/* A new variable that stands for the operand O. Levels never repeat. */
static const Value *fresh(Low *l, Operand o) {
  if (o.w == NULL || (l->level_count == l->level_cap && !levels_grow(l)))
    return NULL;
  l->levels[l->level_count] = o;
  return val_var(l->m, l->level_count++);
}

static const char *op_word(IrOp op) {
  switch (op) {
  case IR_OP_ADD:
    return "add";
  case IR_OP_SUB:
    return "sub";
  case IR_OP_MUL:
    return "mul";
  case IR_OP_EQ:
    return "eq";
  case IR_OP_LE:
    return "le";
  case IR_OP_ADD_CARRY:
    return "add_carry";
  case IR_OP_DIV:
    return "div";
  case IR_OP_MOD:
    return "mod";
  case IR_OP_LT:
    return "lt";
  case IR_OP_W_ADD:
    return "w_add";
  case IR_OP_W_SUB:
    return "w_sub";
  case IR_OP_W_MUL:
    return "w_mul";
  case IR_OP_W_MULHI:
    return "w_mulhi";
  case IR_OP_W_AND:
    return "w_and";
  case IR_OP_W_OR:
    return "w_or";
  case IR_OP_W_XOR:
    return "w_xor";
  case IR_OP_W_SHL:
    return "w_shl";
  case IR_OP_W_SHR:
    return "w_shr";
  case IR_OP_W_ROTL:
    return "w_rotl";
  case IR_OP_W_ROTR:
    return "w_rotr";
  }
  return "?";
}

static void expr_print(const IrExpr *e, FILE *out, unsigned depth) {
  if (depth > IR_PRINT_DEPTH_MAX) {
    fputs("...", out);
    return;
  }
  switch (e->kind) {
  case IR_EXPR_CONST:
    fprintf(out, "%llu", (unsigned long long)e->value);
    return;
  case IR_EXPR_LOCAL:
    fprintf(out, "l%u", (unsigned)e->local);
    return;
  case IR_EXPR_BINARY:
    fprintf(out, "%s(", op_word(e->op));
    expr_print(e->left, out, depth + 1u);
    fputs(", ", out);
    expr_print(e->right, out, depth + 1u);
    fputs(")", out);
    return;
  case IR_EXPR_LOAD:
    fputs("load(", out);
    expr_print(e->left, out, depth + 1u);
    fprintf(out, ", %u)", (unsigned)e->field);
    return;
  }
}

static void block_print(const IrBlock *b, FILE *out, unsigned depth);

static void indent(FILE *out, unsigned depth) {
  fprintf(out, "%*s", (int)(2u * depth), "");
}

static void close_print(FILE *out, unsigned depth) {
  indent(out, depth);
  fputs("}\n", out);
}

static void stmt_print(const IrStmt *s, FILE *out, unsigned depth) {
  size_t i;
  indent(out, depth);
  switch (s->kind) {
  case IR_STMT_SET:
    fprintf(out, "l%u := ", (unsigned)s->local);
    expr_print(s->expr, out, 0);
    fputs("\n", out);
    return;
  case IR_STMT_ALLOC:
    fprintf(out, "l%u := alloc [", (unsigned)s->local);
    for (i = 0; i < s->field_count; i++) {
      fputs(i == 0 ? "" : ", ", out);
      expr_print(s->fields[i], out, 0);
    }
    fputs("]\n", out);
    return;
  case IR_STMT_IF:
    fputs("if ", out);
    expr_print(s->expr, out, 0);
    fputs(" {\n", out);
    block_print(&s->body, out, depth + 1u);
    indent(out, depth);
    fputs("} else {\n", out);
    block_print(&s->otherwise, out, depth + 1u);
    close_print(out, depth);
    return;
  case IR_STMT_SWITCH:
    fputs("switch ", out);
    expr_print(s->expr, out, 0);
    fputs(" {\n", out);
    for (i = 0; i < s->arm_count; i++) {
      indent(out, depth);
      fprintf(out, "arm %zu:\n", i);
      block_print(&s->arms[i], out, depth + 1u);
    }
    close_print(out, depth);
    return;
  case IR_STMT_REPEAT:
    fprintf(out, "repeat l%u := ", (unsigned)s->counter);
    expr_print(s->expr, out, 0);
    fputs(" {\n", out);
    block_print(&s->body, out, depth + 1u);
    close_print(out, depth);
    return;
  case IR_STMT_WHILE:
    fprintf(out, "while l%u := ", (unsigned)s->counter);
    expr_print(s->expr, out, 0);
    fprintf(out, ", l%u {\n", (unsigned)s->local);
    block_print(&s->body, out, depth + 1u);
    close_print(out, depth);
    return;
  case IR_STMT_STORE:
    fprintf(out, "out %u := ", (unsigned)s->local);
    expr_print(s->expr, out, 0);
    fputs("\n", out);
    return;
  case IR_STMT_RETURN:
    fputs("return ", out);
    expr_print(s->expr, out, 0);
    fputs("\n", out);
    return;
  }
}

static void block_print(const IrBlock *b, FILE *out, unsigned depth) {
  size_t i;
  if (depth > IR_PRINT_DEPTH_MAX) {
    fputs("...\n", out);
    return;
  }
  for (i = 0; i < b->count; i++)
    stmt_print(b->items[i], out, depth);
}

static const char *scalar_word(IrScalar s) {
  switch (s) {
  case IR_SCALAR_NAT:
    return "nat";
  case IR_SCALAR_FLAG:
    return "flag";
  case IR_SCALAR_WORD:
    return "word";
  case IR_SCALAR_AMOUNT:
    return "amount";
  case IR_SCALAR_LIST:
    return "list";
  }
  return "?";
}

void ir_print(const IrProgram *prog, FILE *out) {
  size_t i;
  size_t k;
  for (i = 0; i < prog->func_count; i++) {
    const IrFunc *fn = &prog->funcs[i];
    fprintf(out, "func %s (", fn->name);
    for (k = 0; k < fn->param_count; k++)
      fprintf(out, "%s%s", k == 0 ? "" : ", ", scalar_word(fn->params[k]));
    fputs(") -> ", out);
    if (fn->out_words != 0u)
      fprintf(out, "out %u", (unsigned)fn->out_words);
    else
      fputs(scalar_word(fn->result), out);
    fprintf(out, ", %u locals, %u heap words\n", (unsigned)fn->local_count, (unsigned)fn->heap_words);
    block_print(&fn->body, out, 1u);
  }
}

static int lower(Low *l, Buf *b, const Value *v, Operand *out);

static int closure(Low *l) {
  return diag_fail(l->m->diag, "REFUSE_RUNTIME_CLOSURE", l->m->def, "a function value must exist at run time (HOST-LIMIT)");
}

/* 1 when the evaluator gave a value, else 0 after a diagnostic. */
static int got(Low *l, const Value *v) {
  return v != NULL ? 1 : (l->m->diag->set ? 0 : internal(l, "a missing value"));
}

/* RES := the lowering of V. */
static int lower_into(Low *l, Buf *b, const Value *v, Slot res) {
  Operand o;
  return lower(l, b, v, &o) && store(l, b, res, o);
}

/* RES := FN applied to N new variables, one for each operand of ARGS. */
static int apply_into(Low *l, Buf *b, const Value *fn, const Operand *args, uint32_t n, Slot res) {
  const Value *v = fn;
  const Value *x;
  uint32_t i;
  for (i = 0; i < n && v != NULL; i++) {
    x = fresh(l, args[i]);
    v = x == NULL ? NULL : apply_value(l->m, v, x);
  }
  return got(l, v) && lower_into(l, b, v, res);
}

/* A C builtin (S8a, S8b): feMulC md a b (USER ruling Q-S8a-6 (a)), ptMulC md
   k p (Q-S8a-7 (a)) or pairCheckC md ps (Q-S8b-1 (a)), as OP tells. Wasm
   runs the guest spec: the value of the domain def feMul, ptMul or
   pairCheck, applied to the arguments, is lowered in place of the C builtin
   (the lowering inlines each domain def). */
static int builtin_spec(Low *l, Buf *b, const Value *v, Op op, Operand *out) {
  const char *spec = op == OP_PAIR_CHECK ? "pairCheck" : op == OP_PT_MUL ? "ptMul" : "feMul";
  uint32_t arity = op == OP_PAIR_CHECK ? 2u : 3u;
  const Value *f = NULL;
  uint32_t i;
  for (i = 0u; i < l->m->def_count && f == NULL; i++)
    if (strcmp(l->m->defs[i].name, spec) == 0)
      f = def_value(l->m, i);
  if (f == NULL && !l->m->diag->set)
    return refuse(l, op == OP_PAIR_CHECK ? "pairCheckC without the domain def pairCheck"
                     : op == OP_PT_MUL ? "ptMulC without the domain def ptMul"
                                       : "feMulC without the domain def feMul");
  for (i = 0u; i < arity && f != NULL; i++)
    f = apply_value(l->m, f, v->args[i]);
  return got(l, f) && lower(l, b, f, out);
}

/* Q-S8a-9 (a): a sealed def on open arguments. The lowering inlines each
   domain def, thus it unfolds the def here. */
static int sealed_spec(Low *l, Buf *b, const Value *v, Operand *out) {
  const Value *f = seal_unfold(l->m, v);
  return got(l, f) && lower(l, b, f, out);
}

/* A new block [TAG, the lowering of each of the N values of ARGS]. */
static int ctor_block(Low *l, Buf *b, uint64_t tag, const Value *const *args, uint32_t n, Operand *out) {
  Operand *ops = arena_alloc(l->m->arena, ((size_t)n + 1u) * sizeof *ops);
  uint32_t i;
  if (ops == NULL)
    return oom(l);
  for (i = 0; i < n; i++)
    if (!lower(l, b, args[i], &ops[i]))
      return 0;
  return block_ops(l, b, tag, ops, n, out);
}

/* A constructor with fewer arguments than its arity is a function. */
static int ctor_full(Low *l, Buf *b, const Value *v, uint32_t skip, uint32_t n, uint64_t tag, Operand *out) {
  return v->argc < skip + n ? closure(l) : ctor_block(l, b, tag, v->args + skip, n, out);
}

/* S9a (Q-S9-2): a constructor of a word family. A closed chain is its
   number. A Size chain that ends in a run-time value adds its length to that
   word. An Amount chain does not: the sum can wrap at 2^64, and the unary
   Amount has no wrap. */
static int word_ctor(Low *l, Buf *b, const Value *v, uint32_t family, Operand *out) {
  uint64_t n;
  const Value *rest;
  Operand x;
  if (!word_chain(l->m, family, v, &n, &rest))
    return closure(l);
  if (rest == NULL)
    return give(l, out, n);
  if (word_family(l->m, family) == WORD_FAMILY_AMOUNT)
    return refuse(l, "amSucc on a run-time Amount (an i64 word wraps at 2^64)");
  if (!lower(l, b, rest, &x))
    return 0;
  *out = operand(atom(l, b, ex_bin(l, IR_OP_ADD, x.w, ex_const(l, n))));
  return out->w != NULL;
}

static int lower_ctor(Low *l, Buf *b, const Value *v, Operand *out) {
  const CtorInfo *c = &l->m->ctors[v->inst];
  const FamilyInfo *fam = &l->m->families[c->family];
  if (word_family(l->m, c->family) != WORD_FAMILY_NONE)
    return word_ctor(l, b, v, c->family, out);
  return ctor_full(l, b, v, fam->param_count, c->field_count, v->inst - fam->first_ctor, out);
}

/* A canonical value. An eliminator here has too few arguments, so it is a
   function. */
static int lower_canon(Low *l, Buf *b, const Value *v, Operand *out) {
  switch (v->op) {
  case OP_NAT:
  case OP_WORD:
  case OP_FLAG:
  case OP_UNIT:
  case OP_PROD:
  case OP_SUM:
  case OP_OPTION:
  case OP_LIST:
  case OP_FAMILY:
  case OP_UNIT_VAL:
  case OP_FLAG_NO:
  case OP_INTERVAL:
  case OP_SYSTEM:
    return give(l, out, 0);
  case OP_TRANSP:
  case OP_HCOMP:
    return internal(l, "a stuck cubical operation");
  case OP_FLAG_YES:
    return give(l, out, 1);
  case OP_PAIR:
  case OP_PACK:
    return ctor_full(l, b, v, 0, 2, 0, out);
  case OP_INL:
    return ctor_full(l, b, v, 0, 1, 0, out);
  case OP_INR:
  case OP_SOME:
    return ctor_full(l, b, v, 0, 1, 1, out);
  case OP_NONE:
  case OP_NIL:
    return ctor_full(l, b, v, 0, 0, 0, out);
  case OP_CONS:
    return ctor_full(l, b, v, 0, 2, 1, out);
  case OP_CTOR:
    return lower_ctor(l, b, v, out);
  case OP_FIRST:
  case OP_SECOND:
  case OP_EITHER:
  case OP_OPTION_ELIM:
  case OP_PURE:
  case OP_MAP:
  case OP_BIND:
  case OP_FOLD_NAT:
  case OP_FOLD_LIST:
  case OP_FOLD_FAMILY:
  case OP_UNFOLD:
  case OP_FILTER:
  case OP_WITNESS:
  case OP_PAYLOAD:
  case OP_NAT_ADD:
  case OP_W_ADD:
  case OP_W_SUB:
  case OP_W_MUL:
  case OP_W_MULHI:
  case OP_W_AND:
  case OP_W_OR:
  case OP_W_XOR:
  case OP_W_NOT:
  case OP_W_SHL:
  case OP_W_SHR:
  case OP_W_ROTL:
  case OP_W_ROTR:
  case OP_W_LT:
  case OP_W_EQ:
  case OP_FE_MUL:
  case OP_PT_MUL:
  case OP_PAIR_CHECK:
  case OP_DEF_APP:
  case OP_NAT_SUB:
  case OP_NAT_EQ:
  case OP_NAT_LE:
  case OP_FLAG_IF:
  case OP_PROJ:
  case OP_NAT_DIV:
  case OP_NAT_MOD:
  case OP_NONZERO_OF:
  case OP_NONZERO_VALUE:
    return closure(l);
  case OP_NONZERO:
    return give(l, out, 0);
  case OP_FM_LEN:
    return refuse(l, "len (the op count of a FreeMon trace at run time)");
  case OP_ELIM:
  case OP_KMAP:
  case OP_KMAP_OF:
  case OP_KMAP_GET:
  case OP_KMAP_SET:
  case OP_FIN:
  case OP_FIN_ADD:
  case OP_FIN_NEG:
  case OP_FIN_CASE:
  case OP_GLUE:
  case OP_GLUE_ELEM:
  case OP_UNGLUE:
    return refuse(l, "an elim, KMap, Fin or Glue operation");
  }
  return internal(l, "an unknown operation");
}

/* A Nat or Word operation on the two arguments of V. Each op is total. */
static int nat_bin(Low *l, Buf *b, const Value *v, IrOp op, Operand *out) {
  Operand x;
  Operand y;
  if (!lower(l, b, v->args[0], &x) || !lower(l, b, v->args[1], &y))
    return 0;
  *out = operand(atom(l, b, ex_bin(l, op, x.w, y.w)));
  return out->w != NULL;
}

/* wNot X: X xor 0xffffffff (S8). */
static int word_not(Low *l, Buf *b, const Value *v, Operand *out) {
  Operand x;
  if (!lower(l, b, v->args[0], &x))
    return 0;
  *out = operand(atom(l, b, ex_bin(l, IR_OP_W_XOR, x.w, ex_const(l, 0xffffffffu))));
  return out->w != NULL;
}

/* wLt X Y and wEq X Y: a Sum Unit Unit block. The test OP selects the tag:
   inr unit when the test holds, else inl unit (S8). */
static int word_cmp(Low *l, Buf *b, const Value *v, IrOp op, Operand *out) {
  Operand x;
  Operand y;
  Operand unit = operand(ex_const(l, 0));
  Operand yes;
  Operand no;
  Slot res = slot_new(l);
  Buf holds = buf_under(b);
  Buf fails = buf_under(b);
  *out = slot_get(l, res);
  if (!lower(l, b, v->args[0], &x) || !lower(l, b, v->args[1], &y))
    return 0;
  return unit.w != NULL && block_ops(l, &holds, 1, &unit, 1, &yes) && store(l, &holds, res, yes) &&
         block_ops(l, &fails, 0, &unit, 1, &no) && store(l, &fails, res, no) &&
         if_flag(l, b, ex_bin(l, op, x.w, y.w), &holds, &fails);
}

/* natAdd X Y: an Option block. ADD_CARRY selects the tag: none when the sum
   is larger than 2^64 - 1, else some (X + Y). */
static int nat_add(Low *l, Buf *b, const Value *v, Operand *out) {
  Operand x;
  Operand y;
  Operand sum;
  Operand some;
  Operand none;
  Slot res = slot_new(l);
  Buf carry = buf_under(b);
  Buf fits = buf_under(b);
  *out = slot_get(l, res);
  if (!lower(l, b, v->args[0], &x) || !lower(l, b, v->args[1], &y))
    return 0;
  sum = operand(ex_bin(l, IR_OP_ADD, x.w, y.w));
  return sum.w != NULL && block_ops(l, &carry, 0, NULL, 0, &none) && store(l, &carry, res, none) &&
         block_ops(l, &fits, 1, &sum, 1, &some) && store(l, &fits, res, some) &&
         if_flag(l, b, ex_bin(l, IR_OP_ADD_CARRY, x.w, y.w), &carry, &fits);
}

/* nonZero X: none when X is 0, else some X. */
static int nonzero_of(Low *l, Buf *b, const Value *v, Operand *out) {
  Operand x;
  Operand some;
  Operand none;
  Slot res = slot_new(l);
  Buf zero = buf_under(b);
  Buf other = buf_under(b);
  *out = slot_get(l, res);
  return lower(l, b, v->args[0], &x) && block_ops(l, &zero, 0, NULL, 0, &none) && store(l, &zero, res, none) &&
         block_ops(l, &other, 1, &x, 1, &some) && store(l, &other, res, some) &&
         if_flag(l, b, ex_bin(l, IR_OP_EQ, x.w, ex_const(l, 0)), &zero, &other);
}

/* Field K of the block X. */
static int field_of(Low *l, Buf *b, const Value *xv, uint32_t k, Operand *out) {
  Operand x;
  Slot res = slot_new(l);
  *out = slot_get(l, res);
  return lower(l, b, xv, &x) && store(l, b, res, field_op(l, x.w, k));
}

static int flag_if(Low *l, Buf *b, const Value *v, Operand *out) {
  Operand c;
  Slot res = slot_new(l);
  Buf yes = buf_under(b);
  Buf no = buf_under(b);
  *out = slot_get(l, res);
  return lower(l, b, v->args[0], &c) && lower_into(l, &yes, v->args[1], res) && lower_into(l, &no, v->args[2], res) &&
         if_flag(l, b, c.w, &yes, &no);
}

/* SWITCH on the tag of X, N arms. Arm I applies CASES[I] to the FIELDS[I]
   fields of the block (with 0 fields, the arm lowers CASES[I]). */
static int cases_of(Low *l, Buf *b, const Value *xv, const Value *const *cases, const uint32_t *fields, uint32_t n, Operand *out) {
  IrBlock *arms = arena_alloc(l->m->arena, ((size_t)n + 1u) * sizeof *arms);
  Operand *args;
  Operand x;
  Slot res = slot_new(l);
  Buf arm;
  uint32_t i;
  uint32_t k;
  *out = slot_get(l, res);
  if (arms == NULL)
    return oom(l);
  if (!lower(l, b, xv, &x))
    return 0;
  for (i = 0; i < n; i++) {
    arm = buf_under(b);
    args = arena_alloc(l->m->arena, ((size_t)fields[i] + 1u) * sizeof *args);
    if (args == NULL)
      return oom(l);
    for (k = 0; k < fields[i]; k++)
      if (!field_at(l, &arm, x.w, k, &args[k]))
        return 0;
    if (!apply_into(l, &arm, cases[i], args, fields[i], res))
      return 0;
    arms[i] = block_of(&arm);
  }
  return st_switch(l, b, ex_load(l, x.w, 0), arms, n);
}

static int two_cases(Low *l, Buf *b, const Value *v, uint32_t none_fields, Operand *out) {
  uint32_t fields[2];
  fields[0] = none_fields;
  fields[1] = 1;
  return cases_of(l, b, v->args[2], v->args, fields, 2, out);
}

/* The fuel of a walk over a heap spine. */
#define SPINE_FUEL ((uint64_t)1 << 63)

/* A constructor without a recursive field. */
#define NO_REC UINT32_MAX

/* The recursive field of each list tag: nil has none, cons has the tail. */
static const uint32_t LIST_REC[2] = {NO_REC, 1u};

/* The locals of a walk: the reversed chain, its length, the node where the
   walk stopped, and the WHILE condition. */
typedef struct {
  uint32_t rev;
  uint32_t count;
  uint32_t cur;
  uint32_t cont;
} Walk;

static Walk walk_new(Low *l) {
  Walk w;
  w.rev = local_new(l);
  w.count = local_new(l);
  w.cur = local_new(l);
  w.cont = local_new(l);
  return w;
}

/* REV := a new chain cell [1, the word of OP, REV], and COUNT increases by
   1. */
static int chain_push(Low *l, Buf *b, const Walk *w, Operand op) {
  Operand ops[2];
  Operand c;
  ops[0] = op;
  ops[1] = operand(ex_local(l, w->rev));
  return block_ops(l, b, 1, ops, 2u, &c) && set(l, b, w->rev, c.w) &&
         set(l, b, w->count, ex_bin(l, IR_OP_ADD, ex_local(l, w->count), ex_const(l, 1)));
}

/* A walk step at a node whose recursive field is K: push the node, then go
   to field K. */
static int walk_step(Low *l, Buf *b, const Walk *w, uint32_t k) {
  const IrExpr *cur = ex_local(l, w->cur);
  return chain_push(l, b, w, operand(cur)) && set(l, b, w->cur, field_op(l, cur, k).w);
}

/* The spine walk of X. A node with tag I has the recursive field REC[I] (or
   NO_REC). A node with a recursive field goes on the chain. The walk stops
   at a node without one (CUR). The cell of the last node comes first. The
   WHILE has the count FUEL (S9a, D4). */
static int walk_fuel(Low *l, Buf *b, Operand x, const uint32_t *rec, uint32_t n, uint64_t fuel, Walk *w) {
  IrBlock *arms = arena_alloc(l->m->arena, ((size_t)n + 1u) * sizeof *arms);
  Buf body = buf_under(b);
  Buf arm;
  uint32_t i;
  *w = walk_new(l);
  if (arms == NULL)
    return oom(l);
  for (i = 0; i < n; i++) {
    arm = buf_under(&body);
    if (!(rec[i] == NO_REC ? set(l, &arm, w->cont, ex_const(l, 0)) : walk_step(l, &arm, w, rec[i])))
      return 0;
    arms[i] = block_of(&arm);
  }
  return set(l, b, w->rev, ex_const(l, 0)) && set(l, b, w->count, ex_const(l, 0)) && set(l, b, w->cur, x.w) &&
         set(l, b, w->cont, ex_const(l, 1)) &&
         st_switch(l, &body, ex_load(l, ex_local(l, w->cur), 0), arms, n) &&
         st_while(l, b, ex_const(l, fuel), w->cont, &body);
}

/* The spine walk of X with the fuel SPINE_FUEL. */
static int walk(Low *l, Buf *b, Operand x, const uint32_t *rec, uint32_t n, Walk *w) {
  return walk_fuel(l, b, x, rec, n, SPINE_FUEL, w);
}

/* The start of a pass over the chain of W: NODE := field 0 of the cell. */
static int chain_node(Low *l, Buf *body, const Walk *w, const IrExpr **node) {
  uint32_t t = local_new(l);
  *node = ex_local(l, t);
  return set(l, body, t, field_op(l, ex_local(l, w->rev), 0).w);
}

/* The start of a pass over a chain of list nodes: H := the head. */
static int chain_head(Low *l, Buf *body, const Walk *w, Operand *h) {
  const IrExpr *node;
  return chain_node(l, body, w, &node) && field_at(l, body, node, 0, h);
}

/* The end of a pass: REV := the next cell, and REPEAT COUNT {BODY}. */
static int chain_end(Low *l, Buf *b, const Walk *w, Buf *body) {
  return set(l, body, w->rev, field_op(l, ex_local(l, w->rev), 1).w) && st_repeat(l, b, ex_local(l, w->count), body);
}

/* OUT := nil. */
static int tail_init(Low *l, Buf *b, Slot out) {
  Operand nil;
  return block_ops(l, b, 0, NULL, 0, &nil) && store(l, b, out, nil);
}

/* OUT := a new cons cell [1, H, OUT]. */
static int prepend(Low *l, Buf *b, Operand h, Slot out) {
  Operand ops[2];
  Operand c;
  ops[0] = h;
  ops[1] = slot_get(l, out);
  return block_ops(l, b, 1, ops, 2u, &c) && store(l, b, out, c);
}

/* A right fold over a list. F takes the head, then the acc. */
static int fold_list(Low *l, Buf *b, const Value *v, Operand *out) {
  Operand x;
  Operand args[2];
  Walk w;
  Slot acc = slot_new(l);
  Buf in = buf_under(b);
  Buf body = buf_under(b);
  *out = slot_get(l, acc);
  args[1] = slot_get(l, acc);
  return lower(l, b, v->args[2], &x) && walk(l, b, x, LIST_REC, 2u, &w) && lower_into(l, &in, v->args[1], acc) &&
         splice(l, b, &in) && chain_head(l, &body, &w, &args[0]) &&
         apply_into(l, &body, v->args[0], args, 2u, acc) && chain_end(l, b, &w, &body);
}

/* MAP on a list. */
static int list_map(Low *l, Buf *b, const Value *v, Operand *out) {
  Operand x;
  Operand h;
  Walk w;
  Slot res = slot_new(l);
  Slot ys = slot_new(l);
  Buf body = buf_under(b);
  *out = slot_get(l, res);
  return lower(l, b, v->args[1], &x) && walk(l, b, x, LIST_REC, 2u, &w) && tail_init(l, b, res) &&
         chain_head(l, &body, &w, &h) && apply_into(l, &body, v->args[0], &h, 1u, ys) &&
         prepend(l, &body, slot_get(l, ys), res) && chain_end(l, b, &w, &body);
}

/* BIND on a list. The pass starts at the last node and puts the items of
   each result of F in front. */
static int list_bind(Low *l, Buf *b, const Value *v, Operand *out) {
  Operand x;
  Operand h;
  Operand y;
  Walk w;
  Walk u;
  Slot res = slot_new(l);
  Slot rs = slot_new(l);
  Buf body = buf_under(b);
  Buf inner = buf_under(&body);
  *out = slot_get(l, res);
  return lower(l, b, v->args[0], &x) && walk(l, b, x, LIST_REC, 2u, &w) && tail_init(l, b, res) &&
         chain_head(l, &body, &w, &h) && apply_into(l, &body, v->args[1], &h, 1u, rs) &&
         walk(l, &body, slot_get(l, rs), LIST_REC, 2u, &u) &&
         chain_head(l, &inner, &u, &y) && prepend(l, &inner, y, res) && chain_end(l, &body, &u, &inner) &&
         chain_end(l, b, &w, &body);
}

/* FILTER on a list. The pass starts at the last node and keeps each item
   whose test is 1. */
static int list_filter(Low *l, Buf *b, const Value *v, Operand *out) {
  Operand x;
  Operand h;
  Walk w;
  Slot res = slot_new(l);
  Slot ts = slot_new(l);
  Buf body = buf_under(b);
  Buf keep = buf_under(&body);
  Buf drop = buf_under(&body);
  *out = slot_get(l, res);
  return lower(l, b, v->args[1], &x) && walk(l, b, x, LIST_REC, 2u, &w) && tail_init(l, b, res) &&
         chain_head(l, &body, &w, &h) && apply_into(l, &body, v->args[0], &h, 1u, ts) && prepend(l, &keep, h, res) &&
         if_flag(l, &body, ex_local(l, ts.w), &keep, &drop) &&
         chain_end(l, b, &w, &body);
}

/* Push the item of the pair at P, then SEED := the next seed. */
static int unfold_push(Low *l, Buf *b, const Walk *w, const IrExpr *p, Slot seed) {
  Operand a;
  Operand s;
  return field_at(l, b, p, 0, &a) && field_at(l, b, p, 1, &s) && chain_push(l, b, w, a) && store(l, b, seed, s);
}

/* The WHILE body of an unfold: R := G SEED. None stops; some(pair) pushes
   the item. */
static int unfold_step(Low *l, Buf *b, const Walk *w, const Value *g, Slot seed) {
  Operand s = slot_get(l, seed);
  Operand p;
  Slot rs = slot_new(l);
  Operand r = slot_get(l, rs);
  Buf some = buf_under(b);
  Buf none = buf_under(b);
  return apply_into(l, b, g, &s, 1u, rs) && field_at(l, &some, r.w, 0, &p) && unfold_push(l, &some, w, p.w, seed) &&
         set(l, &none, w->cont, ex_const(l, 0)) && if_flag(l, b, ex_load(l, r.w, 0), &some, &none);
}

/* UNFOLD G LIMIT SEED. The WHILE pushes the items on a chain, then a pass
   from the last item builds the list. */
static int unfold(Low *l, Buf *b, const Value *v, Operand *out) {
  Operand lim;
  Operand h;
  Walk w = walk_new(l);
  Slot res = slot_new(l);
  Slot seed = slot_new(l);
  Buf in = buf_under(b);
  Buf step = buf_under(&in);
  Buf body = buf_under(&in);
  *out = slot_get(l, res);
  return lower(l, b, v->args[1], &lim) && set(l, &in, w.rev, ex_const(l, 0)) && set(l, &in, w.count, ex_const(l, 0)) &&
         set(l, &in, w.cont, ex_const(l, 1)) && lower_into(l, &in, v->args[2], seed) &&
         unfold_step(l, &step, &w, v->args[0], seed) && st_while(l, &in, lim.w, w.cont, &step) && tail_init(l, &in, res) &&
         field_at(l, &body, ex_local(l, w.rev), 0, &h) && prepend(l, &body, h, res) && chain_end(l, &in, &w, &body) &&
         splice(l, b, &in);
}

/* A fold step at the node ADDR of the constructor C: ACC := FN applied to
   the fields, with ACC for the recursive field R. */
static int fam_case(Low *l, Buf *b, const IrExpr *base, const Value *fn, const CtorInfo *c, uint32_t r, Slot acc) {
  Operand *args = arena_alloc(l->m->arena, ((size_t)c->field_count + 1u) * sizeof *args);
  uint32_t k;
  if (args == NULL)
    return oom(l);
  for (k = 0; k < c->field_count; k++)
    if (k != r && !field_at(l, b, base, k, &args[k]))
      return 0;
  if (r < c->field_count)
    args[r] = slot_get(l, acc);
  return apply_into(l, b, fn, args, c->field_count, acc);
}

/* SWITCH on the tag of the node at ADDR into ACC. When REC_ARMS is 1, the
   arms of the constructors with a recursive field apply their case (else
   the arms of the ones without). The other arms are not reached; they are
   empty. */
static int fam_switch(Low *l, Buf *b, const IrExpr *base, const Value *v, const uint32_t *rec, int rec_arms, Slot acc) {
  const FamilyInfo *fam = &l->m->families[v->inst];
  IrBlock *arms = arena_alloc(l->m->arena, ((size_t)fam->ctor_count + 1u) * sizeof *arms);
  Buf arm;
  uint32_t i;
  if (arms == NULL)
    return oom(l);
  for (i = 0; i < fam->ctor_count; i++) {
    arm = buf_under(b);
    if ((rec[i] != NO_REC) == rec_arms && !fam_case(l, &arm, base, v->args[i], &l->m->ctors[fam->first_ctor + i], rec[i], acc))
      return 0;
    arms[i] = block_of(&arm);
  }
  return st_switch(l, b, ex_load(l, base, 0), arms, fam->ctor_count);
}

/* A fold over a linear family: the walk, the base node into ACC, then a
   pass from the last recursive node with ACC for its recursive field. */
static int family_spine(Low *l, Buf *b, const Value *v, const uint32_t *rec, Operand *out) {
  const FamilyInfo *fam = &l->m->families[v->inst];
  const IrExpr *node;
  Operand x;
  Walk w;
  Slot acc = slot_new(l);
  Buf in = buf_under(b);
  Buf body = buf_under(b);
  *out = slot_get(l, acc);
  return lower(l, b, v->args[v->argc - 1u], &x) && walk(l, b, x, rec, fam->ctor_count, &w) &&
         fam_switch(l, &in, ex_local(l, w.cur), v, rec, 0, acc) && splice(l, b, &in) &&
         chain_node(l, &body, &w, &node) && fam_switch(l, &body, node, v, rec, 1, acc) && chain_end(l, b, &w, &body);
}

/* A fold over a family without a recursive field is a case split. With at
   most one recursive field in each constructor, it is a spine walk. The
   arguments are the cases of each constructor of the block, the target of
   each family of the block, then the value (stuck_fold). A family outside a
   block is a block of one. The cases of this family start at off. A
   recursive fold over a block of families refuses (S3). */
static int family_fold(Low *l, Buf *b, const Value *v, Operand *out) {
  const FamilyInfo *fam = &l->m->families[v->inst];
  uint32_t off = fam->first_ctor - l->m->families[fam->block_first].first_ctor;
  uint32_t *fields = arena_alloc(l->m->arena, ((size_t)fam->ctor_count + 1u) * sizeof *fields);
  uint32_t *recs = arena_alloc(l->m->arena, ((size_t)fam->ctor_count + 1u) * sizeof *recs);
  const CtorInfo *c;
  uint32_t rec = 0;
  uint32_t here;
  uint32_t i;
  uint32_t k;
  if (fields == NULL || recs == NULL)
    return oom(l);
  for (i = 0; i < fam->ctor_count; i++) {
    c = &l->m->ctors[fam->first_ctor + i];
    here = 0;
    recs[i] = NO_REC;
    for (k = 0; k < c->field_count; k++) {
      here += c->fields[k].recursive ? 1u : 0u;
      recs[i] = c->fields[k].recursive ? k : recs[i];
    }
    if (here > 1u)
      return diag_fail(l->m->diag, "REFUSE_RUNTIME_TREE", l->m->def, "a fold over a tree of %s must exist at run time (HOST-LIMIT)", fam->name);
    rec += here;
    fields[i] = c->field_count;
  }
  if (rec != 0 && fam->block_count > 1u)
    return diag_fail(l->m->diag, "REFUSE_LOWER", l->m->def, "a recursive fold over a block of families");
  return rec != 0 ? family_spine(l, b, v, recs, out) : cases_of(l, b, v->args[v->argc - 1u], v->args + off, fields, fam->ctor_count, out);
}

/* The two arms of an option or a sum: a SWITCH on the tag of X. */
static int two_arms(Low *l, Buf *b, Operand x, const Buf *a0, const Buf *a1) {
  IrBlock *arms = arena_alloc(l->m->arena, 2u * sizeof *arms);
  if (arms == NULL)
    return oom(l);
  arms[0] = block_of(a0);
  arms[1] = block_of(a1);
  return st_switch(l, b, ex_load(l, x.w, 0), arms, 2u);
}

/* Lowers the option or sum XV into X. Arm 0 gets RES := X; arm 1 gets P :=
   the payload. */
static int carrier_begin(Low *l, Buf *b, const Value *xv, Slot res, Operand *x, Buf *a0, Buf *a1, Operand *p) {
  return lower(l, b, xv, x) && store(l, a0, res, *x) && field_at(l, a1, x->w, 0, p);
}

static int carrier_map(Low *l, Buf *b, const Value *v, Operand *out) {
  Operand x;
  Operand p;
  Operand y;
  Operand t;
  Slot res = slot_new(l);
  Slot ts = slot_new(l);
  Buf a0 = buf_under(b);
  Buf a1 = buf_under(b);
  *out = slot_get(l, res);
  t = slot_get(l, ts);
  return carrier_begin(l, b, v->args[1], res, &x, &a0, &a1, &p) && apply_into(l, &a1, v->args[0], &p, 1, ts) &&
         block_ops(l, &a1, 1, &t, 1, &y) && store(l, &a1, res, y) && two_arms(l, b, x, &a0, &a1);
}

static int carrier_bind(Low *l, Buf *b, const Value *v, Operand *out) {
  Operand x;
  Operand p;
  Slot res = slot_new(l);
  Buf a0 = buf_under(b);
  Buf a1 = buf_under(b);
  *out = slot_get(l, res);
  return carrier_begin(l, b, v->args[0], res, &x, &a0, &a1, &p) && apply_into(l, &a1, v->args[1], &p, 1, res) &&
         two_arms(l, b, x, &a0, &a1);
}

static int carrier_filter(Low *l, Buf *b, const Value *v, Operand *out) {
  Operand x;
  Operand p;
  Operand y;
  Slot res = slot_new(l);
  Slot ts = slot_new(l);
  Buf a0 = buf_under(b);
  Buf a1 = buf_under(b);
  Buf keep = buf_under(&a1);
  Buf drop = buf_under(&a1);
  *out = slot_get(l, res);
  return carrier_begin(l, b, v->args[1], res, &x, &a0, &a1, &p) && apply_into(l, &a1, v->args[0], &p, 1, ts) &&
         store(l, &keep, res, x) && block_ops(l, &drop, 0, NULL, 0, &y) && store(l, &drop, res, y) &&
         if_flag(l, &a1, ex_local(l, ts.w), &keep, &drop) &&
         two_arms(l, b, x, &a0, &a1);
}

/* FILTER on a list, or on a sum. */
static int not_option(Low *l, Buf *b, const Value *v, Operand *out) {
  return v->inst == CARRIER_LIST ? list_filter(l, b, v, out) : internal(l, "a filter over a sum");
}

/* ACC := Z, then ACC := STEP ACC, N times. */
static int fold_nat(Low *l, Buf *b, const Value *v, Operand *out) {
  Operand n;
  Operand a;
  Slot acc = slot_new(l);
  Buf in = buf_under(b);
  Buf body = buf_under(&in);
  *out = slot_get(l, acc);
  a = slot_get(l, acc);
  return lower(l, b, v->args[2], &n) && lower_into(l, &in, v->args[1], acc) && apply_into(l, &body, v->args[0], &a, 1, acc) &&
         st_repeat(l, &in, n.w, &body) && splice(l, b, &in);
}

/* An eliminator on a variable. */
/* S9a (Q-S9-2): amMax (MAX is 1) or amMin of two Amount words. */
static int amount_pick(Low *l, Buf *b, const Value *v, int max, Operand *out) {
  Operand x;
  Operand y;
  Slot res = slot_new(l);
  Buf le = buf_under(b);
  Buf gt = buf_under(b);
  *out = slot_get(l, res);
  if (!lower(l, b, v->args[0], &x) || !lower(l, b, v->args[1], &y))
    return 0;
  return store(l, &le, res, max ? y : x) && store(l, &gt, res, max ? x : y) &&
         if_flag(l, b, ex_bin(l, IR_OP_LE, x.w, y.w), &le, &gt);
}

/* S9a (Q-S9-2): amSelect A x y p q is p when x <= y, else q. Only the arm
   that the test selects runs. */
static int amount_select(Low *l, Buf *b, const Value *v, Operand *out) {
  Operand x;
  Operand y;
  Slot res = slot_new(l);
  Buf le = buf_under(b);
  Buf gt = buf_under(b);
  *out = slot_get(l, res);
  if (!lower(l, b, v->args[1], &x) || !lower(l, b, v->args[2], &y))
    return 0;
  return lower_into(l, &le, v->args[3], res) && lower_into(l, &gt, v->args[4], res) &&
         if_flag(l, b, ex_bin(l, IR_OP_LE, x.w, y.w), &le, &gt);
}

/* S9a (Q-S9-2): a sealed application. The four Amount operations of the kit
   are word operations (seal_lowering in eval.c); each other sealed def goes
   to sealed_spec. */
static int amount_op(Low *l, Buf *b, const Value *v, Operand *out) {
  const DefInfo *d = &l->m->defs[v->inst];
  const char *name = d->origin != ORIGIN_PROGRAM && d->name != NULL ? d->name : "";
  if (v->argc == 2u && strcmp(name, "amLe") == 0)
    return word_cmp(l, b, v, IR_OP_LE, out);
  if (v->argc == 2u && strcmp(name, "amMax") == 0)
    return amount_pick(l, b, v, 1, out);
  if (v->argc == 2u && strcmp(name, "amMin") == 0)
    return amount_pick(l, b, v, 0, out);
  if (v->argc == 5u && strcmp(name, "amSelect") == 0)
    return amount_select(l, b, v, out);
  return sealed_spec(l, b, v, out);
}

/* S9a: the refusal of a stuck elim. An elim of a family has the family
   index + 1 in its field. */
static int elim_refuse(Low *l, const Value *v) {
  return refuse(l, v->field != 0u && word_family(l->m, v->field - 1u) != WORD_FAMILY_NONE
                     ? "an elim over a run-time Amount or Size (the value is one i64 word)"
                     : "an elim, KMap, Fin or Glue operation");
}

/* S9a (D26): the lowering reads the operand of LEVEL. */
static void watch_read(Low *l, uint32_t level) {
  uint32_t i;
  for (i = 0; i < l->watch_count; i++)
    l->watch_hit |= l->watch[i] == level ? (uint32_t)1 << i : 0u;
}

/* S9a (D3 (ii)): a step of a run-time elim at the node BASE of the
   constructor C: ACC := FN applied to each field, with ACC after the
   recursive field R (the induction hypothesis). */
static int elim_case(Low *l, Buf *b, const IrExpr *base, const Value *fn, const CtorInfo *c, uint32_t r, Slot acc) {
  Operand *args = arena_alloc(l->m->arena, ((size_t)c->field_count + 2u) * sizeof *args);
  uint32_t k;
  uint32_t n = 0;
  if (args == NULL)
    return oom(l);
  for (k = 0; k < c->field_count; k++) {
    if (!field_at(l, b, base, k, &args[n++]))
      return 0;
    if (k == r)
      args[n++] = slot_get(l, acc);
  }
  return apply_into(l, b, fn, args, n, acc);
}

/* S9a (D3 (ii)): SWITCH on the tag of the node at BASE into ACC. When
   REC_ARMS is 1, the arms of the constructors with a recursive field apply
   their case (else the arms of the ones without). The other arms are
   empty. */
static int elim_switch(Low *l, Buf *b, const IrExpr *base, const Value *v, const uint32_t *rec, int rec_arms, Slot acc) {
  const FamilyInfo *fam = &l->m->families[v->field - 1u];
  IrBlock *arms = arena_alloc(l->m->arena, ((size_t)fam->ctor_count + 1u) * sizeof *arms);
  Buf arm;
  uint32_t i;
  if (arms == NULL)
    return oom(l);
  for (i = 0; i < fam->ctor_count; i++) {
    arm = buf_under(b);
    if ((rec[i] != NO_REC) == rec_arms &&
        !elim_case(l, &arm, base, v->args[1u + i], &l->m->ctors[fam->first_ctor + i], rec[i], acc))
      return 0;
    arms[i] = block_of(&arm);
  }
  return st_switch(l, b, ex_load(l, base, 0), arms, fam->ctor_count);
}

/* S9a (D3 (ii), D4): a run-time elim with a case that reads `ih`. The walk
   of the spine of X, the case of the stop node into ACC, then a pass from
   the last recursive node to the first. In a host-form entry, the walk over
   a family with the bound B stops after B + 1 nodes: a longer spine returns
   the status 1, and the pass is a WHILE with a count-down local. Else the
   walk has the fuel SPINE_FUEL and the pass is a REPEAT; then an ALLOC in a
   step has no static heap bound. */
static int elim_walk(Low *l, Buf *b, const Value *v, Operand x, const uint32_t *rec, Slot acc) {
  const FamilyInfo *fam = &l->m->families[v->field - 1u];
  uint64_t bound = l->host != 0u ? family_bound(l->m, v->field - 1u) : 0u;
  uint64_t fuel = bound != 0u ? bound + 1u : SPINE_FUEL;
  const IrExpr *node;
  Walk w;
  Buf stop = buf_under(b);
  Buf go = buf_under(b);
  Buf in = buf_under(b);
  Buf body = buf_under(b);
  uint32_t left;
  if (!walk_fuel(l, b, x, rec, fam->ctor_count, fuel, &w))
    return 0;
  if (bound != 0u && !(emit(l, &stop, stmt_expr(l, IR_STMT_RETURN, ex_const(l, 1))) &&
                       st_if(l, b, ex_local(l, w.cont), &stop, &go)))
    return 0;
  if (!elim_switch(l, &in, ex_local(l, w.cur), v, rec, 0, acc) || !splice(l, b, &in) ||
      !chain_node(l, &body, &w, &node) || !elim_switch(l, &body, node, v, rec, 1, acc))
    return 0;
  if (bound == 0u)
    return chain_end(l, b, &w, &body);
  left = local_new(l);
  return set(l, b, left, ex_local(l, w.count)) &&
         set(l, &body, w.rev, field_op(l, ex_local(l, w.rev), 1).w) &&
         set(l, &body, left, ex_bin(l, IR_OP_SUB, ex_local(l, left), ex_const(l, 1))) &&
         st_while(l, b, ex_const(l, fuel), left, &body);
}

/* S9a (D3, D4, D26 to D28): a stuck elim of a family at run time. Arm I
   applies case I to each field, with a variable for the induction
   hypothesis after a recursive field. That variable has no value in an
   arm. When no case reads it, the elim is a SWITCH on the tag of the
   scrutinee. When a case reads it, the elim is the walk of elim_walk. These
   stay refusals: a function motive, a word family, a block of families, a
   family with a parameter, an index or a path constructor, and a
   constructor with two recursive fields. */
static int family_elim(Low *l, Buf *b, const Value *v, Operand *out) {
  const FamilyInfo *fam = &l->m->families[v->field - 1u];
  IrBlock *arms = arena_alloc(l->m->arena, ((size_t)fam->ctor_count + 1u) * sizeof *arms);
  uint32_t *rec = arena_alloc(l->m->arena, ((size_t)fam->ctor_count + 1u) * sizeof *rec);
  const CtorInfo *c;
  const Value *var;
  const Value *type;
  Operand *args;
  Operand x;
  Slot res;
  Buf arm;
  uint32_t i;
  uint32_t k;
  uint32_t n;
  uint32_t ih;
  uint32_t slot;
  uint32_t hit = 0;
  int ok;
  if (arms == NULL || rec == NULL)
    return oom(l);
  if (word_family(l->m, v->field - 1u) != WORD_FAMILY_NONE || fam->block_count > 1u || fam->param_count != 0u ||
      fam->index_count != 0u || v->argc != fam->ctor_count + 2u)
    return elim_refuse(l, v);
  for (i = 0; i < fam->ctor_count; i++) {
    c = &l->m->ctors[fam->first_ctor + i];
    rec[i] = NO_REC;
    if (c->interval_count != 0u)
      return elim_refuse(l, v);
    for (k = 0; k < c->field_count; k++) {
      if (c->fields[k].recursive && rec[i] != NO_REC)
        return elim_refuse(l, v);
      if (c->fields[k].recursive)
        rec[i] = k;
    }
  }
  var = fresh(l, operand(ex_const(l, 0)));
  type = var == NULL ? NULL : apply_value(l->m, v->args[0], var);
  if (!got(l, type))
    return 0;
  if (type->kind == VAL_PI)
    return elim_refuse(l, v);
  res = slot_new(l);
  *out = slot_get(l, res);
  if (!lower(l, b, v->args[v->argc - 1u], &x))
    return 0;
  for (i = 0; i < fam->ctor_count && hit == 0u; i++) {
    c = &l->m->ctors[fam->first_ctor + i];
    arm = buf_under(b);
    args = arena_alloc(l->m->arena, ((size_t)c->field_count + 2u) * sizeof *args);
    if (args == NULL)
      return oom(l);
    if (rec[i] != NO_REC && l->watch_count == LOW_WATCH_MAX)
      return elim_refuse(l, v);
    n = 0;
    ih = NO_REC;
    for (k = 0; k < c->field_count; k++) {
      if (!field_at(l, &arm, x.w, k, &args[n++]))
        return 0;
      if (k == rec[i]) {
        ih = n;
        args[n++] = operand(ex_const(l, 0));
      }
    }
    slot = l->watch_count;
    if (ih != NO_REC) {
      l->watch[slot] = l->level_count + ih;
      l->watch_hit &= ~((uint32_t)1 << slot);
      l->watch_count++;
    }
    ok = apply_into(l, &arm, v->args[1u + i], args, n, res);
    if (ih != NO_REC) {
      l->watch_count--;
      hit = (l->watch_hit >> slot) & 1u;
    }
    if (!ok)
      return 0;
    arms[i] = block_of(&arm);
  }
  if (hit != 0u)
    return elim_walk(l, b, v, x, rec, res);
  return st_switch(l, b, ex_load(l, x.w, 0), arms, fam->ctor_count);
}

static int lower_stuck(Low *l, Buf *b, const Value *v, Operand *out) {
  switch (v->op) {
  case OP_NAT:
  case OP_WORD:
  case OP_FLAG:
  case OP_UNIT:
  case OP_PROD:
  case OP_SUM:
  case OP_OPTION:
  case OP_LIST:
  case OP_FAMILY:
  case OP_UNIT_VAL:
  case OP_FLAG_YES:
  case OP_FLAG_NO:
  case OP_PAIR:
  case OP_INL:
  case OP_INR:
  case OP_NONE:
  case OP_SOME:
  case OP_NIL:
  case OP_CONS:
  case OP_PACK:
  case OP_CTOR:
  case OP_PURE:
  case OP_INTERVAL:
  case OP_SYSTEM:
    return internal(l, "a stuck canonical value");
  case OP_TRANSP:
  case OP_HCOMP:
    return internal(l, "a stuck cubical operation");
  case OP_FIRST:
  case OP_WITNESS:
    return field_of(l, b, v->args[0], 0, out);
  case OP_SECOND:
  case OP_PAYLOAD:
    return field_of(l, b, v->args[0], 1, out);
  case OP_PROJ:
    return field_of(l, b, v->args[0], v->field, out);
  case OP_EITHER:
    return two_cases(l, b, v, 1, out);
  case OP_OPTION_ELIM:
    return two_cases(l, b, v, 0, out);
  case OP_MAP:
    return v->inst == CARRIER_LIST ? list_map(l, b, v, out) : carrier_map(l, b, v, out);
  case OP_BIND:
    return v->inst == CARRIER_LIST ? list_bind(l, b, v, out) : carrier_bind(l, b, v, out);
  case OP_FILTER:
    return v->inst == CARRIER_OPTION ? carrier_filter(l, b, v, out) : not_option(l, b, v, out);
  case OP_FOLD_NAT:
    return fold_nat(l, b, v, out);
  case OP_FOLD_LIST:
    return fold_list(l, b, v, out);
  case OP_UNFOLD:
    return unfold(l, b, v, out);
  case OP_FOLD_FAMILY:
    return word_family(l->m, v->inst) != WORD_FAMILY_NONE
             ? refuse(l, "a fold over a run-time Amount or Size (the value is one i64 word)")
             : family_fold(l, b, v, out);
  case OP_NAT_SUB:
    return nat_bin(l, b, v, IR_OP_SUB, out);
  case OP_NAT_EQ:
    return nat_bin(l, b, v, IR_OP_EQ, out);
  case OP_NAT_LE:
    return nat_bin(l, b, v, IR_OP_LE, out);
  case OP_FLAG_IF:
    return flag_if(l, b, v, out);
  case OP_NAT_ADD:
    return nat_add(l, b, v, out);
  case OP_NAT_DIV:
    return nat_bin(l, b, v, IR_OP_DIV, out);
  case OP_NAT_MOD:
    return nat_bin(l, b, v, IR_OP_MOD, out);
  case OP_W_ADD:
    return nat_bin(l, b, v, IR_OP_W_ADD, out);
  case OP_W_SUB:
    return nat_bin(l, b, v, IR_OP_W_SUB, out);
  case OP_W_MUL:
    return nat_bin(l, b, v, IR_OP_W_MUL, out);
  case OP_W_MULHI:
    return nat_bin(l, b, v, IR_OP_W_MULHI, out);
  case OP_W_AND:
    return nat_bin(l, b, v, IR_OP_W_AND, out);
  case OP_W_OR:
    return nat_bin(l, b, v, IR_OP_W_OR, out);
  case OP_W_XOR:
    return nat_bin(l, b, v, IR_OP_W_XOR, out);
  case OP_W_SHL:
    return nat_bin(l, b, v, IR_OP_W_SHL, out);
  case OP_W_SHR:
    return nat_bin(l, b, v, IR_OP_W_SHR, out);
  case OP_W_ROTL:
    return nat_bin(l, b, v, IR_OP_W_ROTL, out);
  case OP_W_ROTR:
    return nat_bin(l, b, v, IR_OP_W_ROTR, out);
  case OP_W_NOT:
    return word_not(l, b, v, out);
  case OP_W_LT:
    return word_cmp(l, b, v, IR_OP_LT, out);
  case OP_W_EQ:
    return word_cmp(l, b, v, IR_OP_EQ, out);
  case OP_FE_MUL:
    return builtin_spec(l, b, v, OP_FE_MUL, out);
  case OP_PT_MUL:
    return builtin_spec(l, b, v, OP_PT_MUL, out);
  case OP_PAIR_CHECK:
    return builtin_spec(l, b, v, OP_PAIR_CHECK, out);
  case OP_DEF_APP:
    return amount_op(l, b, v, out);
  case OP_NONZERO_OF:
    return nonzero_of(l, b, v, out);
  case OP_NONZERO_VALUE:
    return lower(l, b, v->args[0], out);
  case OP_NONZERO:
    return internal(l, "a stuck canonical value");
  case OP_FM_LEN:
    return refuse(l, "len (the op count of a FreeMon trace at run time)");
  case OP_ELIM:
    return v->field != 0u ? family_elim(l, b, v, out) : elim_refuse(l, v);
  case OP_KMAP:
  case OP_KMAP_OF:
  case OP_KMAP_GET:
  case OP_KMAP_SET:
  case OP_FIN:
  case OP_FIN_ADD:
  case OP_FIN_NEG:
  case OP_FIN_CASE:
  case OP_GLUE:
  case OP_GLUE_ELEM:
  case OP_UNGLUE:
    return refuse(l, "an elim, KMap, Fin or Glue operation");
  }
  return internal(l, "an unknown operation");
}

static int lower_value(Low *l, Buf *b, const Value *v, Operand *out) {
  switch (v->kind) {
  case VAL_NAT:
  case VAL_WORD:
    return give(l, out, v->nat);
  case VAL_UNIV:
  case VAL_PI:
  case VAL_SIGMA:
  case VAL_PLAM:
  case VAL_PATHP:
  case VAL_IVAL:
    return give(l, out, 0);
  case VAL_PAPP:
    return internal(l, "a stuck cubical operation");
  case VAL_LAM:
  case VAL_APP:
    return closure(l);
  case VAL_VAR:
    if (v->nat >= l->level_count)
      return internal(l, "a variable without an operand");
    watch_read(l, v->nat);
    *out = l->levels[v->nat];
    return 1;
  case VAL_OP:
    return lower_canon(l, b, v, out);
  case VAL_STUCK:
    return lower_stuck(l, b, v, out);
  }
  return internal(l, "an unknown value");
}

static int lower(Low *l, Buf *b, const Value *v, Operand *out) {
  int ok;
  if (v == NULL)
    return got(l, v);
  if (memo_kind(v) && memo_find(b, v, out))
    return 1;
  if (l->m->depth >= EVAL_DEPTH_LIMIT)
    return diag_fail(l->m->diag, "EVAL_DEPTH", l->m->def, "the lowering nests deeper than %u levels", EVAL_DEPTH_LIMIT);
  l->m->depth++;
  ok = lower_value(l, b, v, out);
  l->m->depth--;
  return ok && (!memo_kind(v) || memo_put(l, b, v, *out));
}

/* The heap bound (S3): the ALLOC words on the longest path of a block.
   An IF or a SWITCH takes its largest branch. A REPEAT or a WHILE with a
   constant count takes the count times its body. The bound stops at
   IR_HEAP_WORDS_MAX + 1. 0 (no static bound) when an ALLOC is under a
   REPEAT or a WHILE with a run-time count. */
static uint64_t heap_add(uint64_t a, uint64_t b) {
  return a + b > IR_HEAP_WORDS_MAX ? IR_HEAP_WORDS_MAX + 1u : a + b;
}

static uint64_t heap_times(uint64_t count, uint64_t words) {
  return words != 0 && count > IR_HEAP_WORDS_MAX / words ? IR_HEAP_WORDS_MAX + 1u : count * words;
}

static int heap_block(const IrBlock *b, uint64_t *out);

static int heap_stmt(const IrStmt *s, uint64_t *out) {
  uint64_t x = 0;
  uint64_t y = 0;
  size_t k;
  int ok = 1;
  *out = 0;
  switch (s->kind) {
    case IR_STMT_SET:
    case IR_STMT_STORE:
    case IR_STMT_RETURN:
      return 1;
    case IR_STMT_ALLOC:
      *out = heap_add(0, s->field_count);
      return 1;
    case IR_STMT_IF:
      ok = heap_block(&s->body, &x) && heap_block(&s->otherwise, &y);
      *out = x > y ? x : y;
      return ok;
    case IR_STMT_SWITCH:
      for (k = 0; k < s->arm_count && ok; k++) {
        ok = heap_block(&s->arms[k], &y);
        x = y > x ? y : x;
      }
      *out = x;
      return ok;
    case IR_STMT_REPEAT:
    case IR_STMT_WHILE:
      ok = heap_block(&s->body, &x) && (x == 0 || s->expr->kind == IR_EXPR_CONST);
      *out = s->expr->kind == IR_EXPR_CONST ? heap_times(s->expr->value, x) : 0;
      return ok;
  }
  return 0;
}

static int heap_block(const IrBlock *b, uint64_t *out) {
  uint64_t x = 0;
  size_t i;
  int ok = 1;
  *out = 0;
  for (i = 0; i < b->count && ok; i++) {
    ok = heap_stmt(b->items[i], &x);
    *out = heap_add(*out, x);
  }
  return ok;
}

typedef char entry_out_words_fit[ENTRY_OUT_WORDS_MAX <= IR_OUT_WORDS_MAX ? 1 : -1];

/* S9a (D5): out word WORD := E. */
static int out_store(Low *l, Buf *b, uint32_t word, const IrExpr *e) {
  IrStmt *s = stmt_expr(l, IR_STMT_STORE, e);
  if (s != NULL)
    s->local = word;
  return emit(l, b, s);
}

/* S9a (D5): the host form. The result block at R goes to the out words: a
   SWITCH on its tag, and the arm of each constructor stores the tag, its
   fields, and 0 in each other word (a block has only the words of its
   constructor). Then the entry returns the status 0. */
static int host_result(Low *l, Buf *b, const Entry *e, Operand r) {
  const FamilyInfo *fam = &l->m->families[e->result_family];
  IrBlock *arms = arena_alloc(l->m->arena, ((size_t)fam->ctor_count + 1u) * sizeof *arms);
  const IrExpr *base = atom(l, b, r.w);
  Buf arm;
  uint32_t i;
  uint32_t k;
  uint32_t n;
  if (arms == NULL)
    return oom(l);
  if (base == NULL)
    return 0;
  for (i = 0; i < fam->ctor_count; i++) {
    n = l->m->ctors[fam->first_ctor + i].field_count;
    arm = buf_under(b);
    if (!out_store(l, &arm, 0, ex_const(l, i)))
      return 0;
    for (k = 1; k < e->out_words; k++)
      if (!out_store(l, &arm, k, k <= n ? ex_load(l, base, k) : ex_const(l, 0)))
        return 0;
    arms[i] = block_of(&arm);
  }
  return st_switch(l, b, ex_load(l, base, 0), arms, fam->ctor_count) &&
         emit(l, b, stmt_expr(l, IR_STMT_RETURN, ex_const(l, 0)));
}

/* The entry INDEX, applied to a variable for each parameter. */
/* S9a (D37): the byte of list J in the host region. */
static uint64_t list_at(uint32_t j) {
  return IR_HOST_HEAP_BASE + IR_HOST_LIST_BYTES * j;
}

/* S9a (D39): a count above IR_HOST_LIST_MAX in the local K gives the status
   1 with no out word (D29). LT is unsigned. */
static int list_over(Low *l, Buf *b, uint32_t k) {
  Buf stop = buf_under(b);
  Buf go = buf_under(b);
  return emit(l, &stop, stmt_expr(l, IR_STMT_RETURN, ex_const(l, 1))) &&
         st_if(l, b, ex_bin(l, IR_OP_LT, ex_const(l, IR_HOST_LIST_MAX), ex_local(l, k)), &stop, &go);
}

/* S9a (D39): the cons blocks of the list at the byte BASE, with its count in
   the local K. A pass from element IR_HOST_LIST_MAX - 1 to element 0 adds a
   block for each element below the count, so K then holds the block of
   element 0. */
static int list_build(Low *l, Buf *b, uint32_t k, uint64_t base) {
  Buf body = buf_under(b);
  Buf yes = buf_under(b);
  Buf no = buf_under(b);
  uint32_t cur = local_new(l);
  uint32_t idx = local_new(l);
  Operand nil;
  Operand cell;
  Operand ops[2];
  if (!block_ops(l, b, 0, ops, 0, &nil) || !set(l, b, cur, nil.w) || !set(l, b, idx, ex_const(l, IR_HOST_LIST_MAX)))
    return 0;
  ops[0] = operand(
      ex_load(l, ex_bin(l, IR_OP_ADD, ex_const(l, base), ex_bin(l, IR_OP_MUL, ex_local(l, idx), ex_const(l, 8))), 1));
  ops[1] = operand(ex_local(l, cur));
  return block_ops(l, &yes, 1, ops, 2, &cell) && set(l, &yes, cur, cell.w) &&
         set(l, &body, idx, ex_bin(l, IR_OP_SUB, ex_local(l, idx), ex_const(l, 1))) &&
         st_if(l, &body, ex_bin(l, IR_OP_LT, ex_local(l, idx), ex_local(l, k)), &yes, &no) &&
         st_repeat(l, b, ex_const(l, IR_HOST_LIST_MAX), &body) && set(l, b, k, ex_local(l, cur));
}

/* S9a (D39): the prologue of an entry with a list parameter. The list
   parameters come last (D36). First each count, then the bound test of each
   count, then the blocks of each list. */
static int host_lists(Low *l, Buf *b, const Entry *e) {
  uint32_t first = e->param_count - e->list_count;
  uint32_t j;
  for (j = 0; j < e->list_count; j++)
    if (!set(l, b, first + j, ex_load(l, ex_const(l, list_at(j)), 0)))
      return 0;
  for (j = 0; j < e->list_count; j++)
    if (!list_over(l, b, first + j))
      return 0;
  for (j = 0; j < e->list_count; j++)
    if (!list_build(l, b, first + j, list_at(j)))
      return 0;
  return 1;
}

static int lower_entry(Machine *m, uint32_t index, const Entry *e, IrFunc *fn) {
  Low l;
  Buf body = buf_under(NULL);
  IrScalar *params = arena_alloc(m->arena, ((size_t)e->param_count + 1u) * sizeof *params);
  const Value *v;
  const Value *x;
  Operand r;
  uint32_t k;
  memset(&l, 0, sizeof l);
  l.m = m;
  l.watch_count = 0;
  l.watch_hit = 0;
  l.host = e->out_words != 0u ? 1u : 0u;
  l.local_count = e->param_count;
  m->def = m->defs[index].name;
  m->fuel = EVAL_FUEL_STEPS;
  if (params == NULL)
    return oom(&l);
  v = def_value(m, index);
  /* S9a (D39): the prologue of the host lists comes before the body. */
  if (e->list_count != 0u && !host_lists(&l, &body, e))
    return 0;
  for (k = 0; k < e->param_count && v != NULL; k++) {
    params[k] = e->param_flag[k] ? IR_SCALAR_FLAG
                : e->param_word[k] ? IR_SCALAR_WORD
                : e->param_amount[k] ? IR_SCALAR_AMOUNT
                : e->param_list[k] != 0u ? IR_SCALAR_LIST
                : IR_SCALAR_NAT;
    x = fresh(&l, operand(ex_local(&l, k)));
    v = x == NULL ? NULL : apply_value(m, v, x);
  }
  if (!got(&l, v) || !lower(&l, &body, v, &r))
    return 0;
  if (e->out_words != 0u ? !host_result(&l, &body, e, r) : !emit(&l, &body, stmt_expr(&l, IR_STMT_RETURN, r.w)))
    return 0;
  fn->name = m->defs[index].name;
  fn->params = params;
  fn->param_count = e->param_count;
  fn->result = e->result_flag ? IR_SCALAR_FLAG : IR_SCALAR_NAT;
  fn->local_count = l.local_count;
  fn->body = block_of(&body);
  fn->heap_words = 0;
  fn->out_words = e->out_words;
  fn->heap_base = e->out_words != 0u ? IR_HOST_HEAP_BASE + IR_HOST_LIST_BYTES * e->list_count : 0u;
  if (!heap_block(&fn->body, &fn->heap_words) || fn->heap_words > IR_HEAP_WORDS_MAX)
    return diag_fail(m->diag, "REFUSE_LOWER", m->def, "heap use has no static bound");
  return 1;
}

/* Q-S9-39: the host form exports the Wasm memory under the name memory
   (wasm.c 404). So a module with a host-form entry cannot also have an
   entry named memory: the two exports would have the same name. */
static int memory_clash(const IrProgram *prog) {
  size_t i;
  int host = 0;
  int named = 0;
  for (i = 0; i < prog->func_count; i++) {
    host = host || prog->funcs[i].out_words != 0u;
    named = named || strcmp(prog->funcs[i].name, "memory") == 0;
  }
  return host && named;
}

int lower_program(Machine *m, IrProgram *prog) {
  IrFunc *funcs = arena_alloc(m->arena, ((size_t)m->def_count + 1u) * sizeof *funcs);
  Entry e;
  size_t n = 0;
  uint32_t i;
  int entry;
  prog->funcs = funcs;
  prog->func_count = 0;
  if (funcs == NULL)
    return diag_fail(m->diag, "OOM", m->def, "out of memory");
  seal_lowering(m);
  for (i = 0; i < m->def_count; i++) {
    m->fuel = EVAL_FUEL_STEPS;
    entry = m->defs[i].origin == ORIGIN_PROGRAM && entry_of(m, m->defs[i].type, &e) && !e.result_option;
    if (entry && !lower_entry(m, i, &e, &funcs[n]))
      return 0;
    n += entry ? 1u : 0u;
  }
  prog->func_count = n;
  if (memory_clash(prog))
    return diag_fail(m->diag, "REFUSE_LOWER", "memory", "the name memory clashes with the memory export of the host form");
  return 1;
}
