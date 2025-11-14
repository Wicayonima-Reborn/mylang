/* 
 * Minimal runtime used by generated programs:
 * - runtime_new_string(const char* lit) -> alloc and copy string to heap
 * - runtime_print_any : checks pointer/number rudimentarily and prints
 * - runtime_clone_string
 * - runtime_drop_string
 *
 * Note: These are simplistic helpers to let compiled examples run.
 */

#include "../include/common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Allocate a heap string from literal pointer */
char* runtime_new_string(const char *lit) {
    if (!lit) return NULL;
    size_t n = strlen(lit);
    char *p = malloc(n + 1);
    memcpy(p, lit, n + 1);
    return p;
}

int runtime_print_int(long v) {
    printf("%ld\n", v);
    return 0;
}

int runtime_print_string(char *s) {
    if (s)
        printf("%s\n", s);
    else
        printf("(null)\n");
    return 0;
}

char* runtime_clone_string(char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = malloc(n + 1);
    memcpy(p, s, n + 1);
    return p;
}

void runtime_drop_string(char *s) {
    if (!s) return;
    free(s);
}