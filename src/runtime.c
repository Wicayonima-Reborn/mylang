/**
 * @file runtime.c
 * @brief Runtime support library for MyLang generated binaries.
 * * This library is linked with the assembly output of the compiler.
 * It provides the concrete implementation for functions called by 
 * the x86_64 backend.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------
   STRING MANAGEMENT
   --------------------------------------------------------- */

/**
 * @brief Allocates a new heap-backed string from a static literal.
 * Called by the backend when a "string literal" is encountered.
 */
char *runtime_new_string(const char *lit) {
    if (!lit) return NULL;

    /* Safe length check using snprintf per project style */
    size_t n = snprintf(NULL, 0, "%s", lit);

    char *p = malloc(n + 1);
    if (!p) {
        fprintf(stderr, "runtime error: out of memory in runtime_new_string\n");
        exit(1);
    }

    snprintf(p, n + 1, "%s", lit);
    return p;
}

/**
 * @brief Deep-copies a string object.
 * Supporting 'move' semantics: if a user wants to keep the original
 * and pass a copy, the 'clone()' function in MyLang maps here.
 */
char *runtime_clone_string(char *s) {
    if (!s) return NULL;

    size_t n = snprintf(NULL, 0, "%s", s);
    char *p = malloc(n + 1);
    if (!p) {
        fprintf(stderr, "runtime error: out of memory in runtime_clone_string\n");
        exit(1);
    }

    snprintf(p, n + 1, "%s", s);
    return p;
}

/**
 * @brief Destroys a string object and releases heap memory.
 * This is the target for the "drop" logic (RAII) when a variable 
 * goes out of scope.
 */
void runtime_drop_string(char *s) {
    if (s) {
        free(s);
    }
}

/* ---------------------------------------------------------
   I/O OPERATIONS
   --------------------------------------------------------- */

/**
 * @brief Prints a 64-bit signed integer to stdout.
 */
int runtime_print_int(long v) {
    return printf("%ld\n", v);
}

/**
 * @brief Prints a MyLang string object to stdout.
 */
int runtime_print_string(char *s) {
    if (s) {
        return printf("%s\n", s);
    } else {
        return printf("(null)\n");
    }
}