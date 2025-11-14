#ifndef COMMON_H
#define COMMON_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

#define MAX_TOK_LEN 256
#define MAX_IDENT 128

struct Type;

typedef enum {
    TY_INT,
    TY_STRING,
    TY_REF,
    TY_MUTREF,
    TY_RC,
    TY_UNKNOWN
} TypeKind;

// === define Type AFTER forward declaration ===
typedef struct Type {
    TypeKind kind;
    struct Type *inner;
} Type;

static inline Type mktype(TypeKind k) {
    Type t; t.kind = k; t.inner = NULL; return t;
}
static inline Type mkref(TypeKind k, Type *inner) {
    Type t; t.kind = k; t.inner = inner; return t;
}

void errorf(const char *fmt, ...);
void *xmalloc(size_t s);

#endif