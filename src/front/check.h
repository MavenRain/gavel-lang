/* The type checker, the refusals R1-R4 and the eval command. SHARED by the
   tcc kits. */
#ifndef LANG_FRONT_CHECK_H
#define LANG_FRONT_CHECK_H

#include "front/core.h"

#define ENTRY_PARAMS_MAX 64u /* 16 before sealed-lang S2 (R23): slashUsr takes 19
                                words. 32 before S8a: curveMadd takes 56 words. */

/* An entry: a program definition of type Nat, Flag, or a function of Nat and
   Flag arguments to a Nat or Flag result. S9a: a function of 1 or more
   arguments to a family result is a host-form entry, when the family has
   no parameter, no recursive field, one word for each field, and
   ENTRY_OUT_WORDS_MAX out words or less. */
#define ENTRY_OUT_WORDS_MAX 8u
typedef struct {
  uint32_t param_count;
  int param_flag[ENTRY_PARAMS_MAX];
  int param_word[ENTRY_PARAMS_MAX]; /* S8: the parameter is a Word */
  int param_amount[ENTRY_PARAMS_MAX]; /* S9a: the parameter is an Amount */
  /* S9a (D36): the list family of the parameter plus 1 (0 = not a list).
     The list parameters come after the scalar parameters. */
  uint32_t param_list[ENTRY_PARAMS_MAX];
  uint32_t list_count;
  int result_flag;
  int result_option;
  /* S9a: out_words is the count of the out words of a host-form entry (0 =
     an old-form entry). result_family is its result family. */
  uint32_t out_words;
  uint32_t result_family;
} Entry;

/* Checks the domain declarations, then the program, into M. Returns 1, or 0
   after a diagnostic. */
/* The check mode (orbit-lang S3). In guest mode a program declaration
   cannot use a kit item (REFUSE_KIT). Kit mode is the trusted test scope of
   the kit gate (`langc --kit`). */
typedef enum {
  CHECK_GUEST,
  CHECK_KIT
} CheckMode;

int check_program(Arena *arena, const DeclList *decls, Machine *m, Diag *diag, CheckMode mode);
/* Returns 1 when TYPE is the type of an entry. */
int entry_of(Machine *m, const Value *type, Entry *out);

/* S9a (D4): the bound of the list family FAMILY, or 0 (no bound). */
uint32_t family_bound(const Machine *m, uint32_t family);
/* Evaluates the definition NAME on ARGS and prints the result. Returns 0, 1
   after a diagnostic, or 2 after an EVAL_ARGS diagnostic. */
int eval_command(Machine *m, const char *name, char *const *args, int arg_count, FILE *out, FILE *err);

#endif
