#ifndef UTILS_H
#define UTILS_H
#include <stdlib.h>

#define DEFINE_ARRAY(T, Name)                                                  \
  typedef struct {                                                             \
    T *data;                                                                   \
    size_t len, cap;                                                           \
  } Name;                                                                      \
  static inline int Name##_push(Name *a, T value) {                            \
    if (a->len == a->cap) {                                                    \
      size_t cap = a->cap ? a->cap * 2 : 8;                                    \
      T *p = realloc(a->data, cap * sizeof *p);                                \
      if (!p)                                                                  \
        return 0;                                                              \
      a->data = p;                                                             \
      a->cap = cap;                                                            \
    }                                                                          \
    a->data[a->len++] = value;                                                 \
    return 1;                                                                  \
  }                                                                            \
  static inline void Name##_free(Name *a) {                                    \
    if (!a)                                                                    \
      return;                                                                  \
    free(a->data);                                                             \
    free(a);                                                                   \
  }

double middle(double a, double b);

#endif
