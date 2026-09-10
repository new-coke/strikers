#ifndef _RUNTIME_VA_ARG_H
#define _RUNTIME_VA_ARG_H

#include "types.h"

/* clang-format off */
typedef struct {
  char gpr;
  char fpr;
  char mPadding[2];
  char* input_arg_area;
  char* reg_save_area;
} va_list;

typedef enum {
  ARGPOINTER = 0,
  WORD,
  DOUBLEWORD,
  REAL,
} _va_arg_type;
/* clang-format on */

#endif
