// src/common.c
#include "../include/common.h"

void *xmalloc(size_t s) {
    void *p = malloc(s);
    if (!p) {
        fprintf(stderr, "FATAL: out of memory allocating %zu bytes\n", s);
        exit(1);
    }
    return p;
}

void errorf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    exit(1);
}