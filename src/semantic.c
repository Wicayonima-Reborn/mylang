/**
 * @file semantic.c
 * @brief Semantic analysis pass: Type checking, Inference, and Symbol Resolution.
 * * This pass traverses the AST to ensure logic follows language rules. 
 * It 'decorates' the AST by filling in Expr->type fields, which allows the 
 * backend to generate type-specific machine instructions (e.g., distinguishing 
 * between an integer print and a string print).
 */

#include "../include/semantic.h"
#include "../include/ast.h"
#include "../include/common.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>

/** Entry in the symbol table representing a variable. */
typedef struct Sym {
    char name[MAX_IDENT];
    Type type;
    int defined_line;
    struct Sym *next;
} Sym;

/** A simple linked-list based symbol table for lexical scoping. */
typedef struct {
    Sym *head;
} SymTable;

/* ---------------------------------------------------------
   SYMBOL TABLE HELPERS
   --------------------------------------------------------- */

/** Adds a symbol to the current scope. */
static void sym_add(SymTable *t, const char *name, Type ty, int line) {
    Sym *s = xmalloc(sizeof(Sym));
    snprintf(s->name, sizeof(s->name), "%s", name);
    s->type = ty;
    s->defined_line = line;
    s->next = t->head;
    t->head = s;
}

/** Looks up a symbol by name in the current and parent scopes. */
static Sym *sym_find(SymTable *t, const char *name) {
    for (Sym *s = t->head; s; s = s->next) {
        if (strcmp(s->name, name) == 0)
            return s;
    }
    return NULL;
}

/* ---------------------------------------------------------
   TYPE INFERENCE & ANNOTATION
   --------------------------------------------------------- */

/**
 * @brief Recursively determines the type of an expression.
 * @return The inferred Type of the expression.
 */
static Type infer_expr(Expr *e, SymTable *sym) {
    if (!e) return mktype(TY_UNKNOWN);

    Type result = mktype(TY_UNKNOWN);

    switch (e->kind) {
    case E_INT_LIT:
        result = mktype(TY_INT);
        break;

    case E_STR_LIT:
        result = mktype(TY_STRING);
        break;

    case E_IDENT: {
        Sym *s = sym_find(sym, e->v.ident);
        if (!s) {
            errorf("Semantic error: use of undeclared variable '%s' at %d:%d\n",
                   e->v.ident, e->line, e->col);
            exit(1);
        }
        result = s->type;
        break;
    }

    case E_ADDR: {
        Type inner = infer_expr(e->v.inner, sym);
        Type *p = xmalloc(sizeof(Type));
        *p = inner;
        result = mkref(TY_REF, p);
        break;
    }

    case E_MUTADDR: {
        Type inner = infer_expr(e->v.inner, sym);
        Type *p = xmalloc(sizeof(Type));
        *p = inner;
        result = mkref(TY_MUTREF, p);
        break;
    }

    case E_CALL: {
        const char *fn = e->v.call.name;

        /* Built-in: clone(string) -> string */
        if (strcmp(fn, "clone") == 0) {
            if (e->v.call.nargs != 1) {
                errorf("clone() expects 1 argument at %d:%d\n", e->line, e->col);
                exit(1);
            }
            Type arg = infer_expr(e->v.call.args[0], sym);
            if (arg.kind != TY_STRING) {
                errorf("clone() requires string type at %d:%d\n", e->line, e->col);
                exit(1);
            }
            result = mktype(TY_STRING);
        } 
        /* Built-in: print(any) -> int */
        else if (strcmp(fn, "print") == 0) {
            if (e->v.call.nargs != 1) {
                errorf("print() expects 1 argument at %d:%d\n", e->line, e->col);
                exit(1);
            }
            infer_expr(e->v.call.args[0], sym);
            result = mktype(TY_INT);
        } 
        else {
            errorf("Unknown function '%s' at %d:%d\n", fn, e->line, e->col);
            exit(1);
        }
        break;
    }

    default:
        errorf("Semantic: unsupported expr kind %d at %d:%d\n", e->kind, e->line, e->col);
        exit(1);
    }

    /* Important: Annotate the node so the Backend knows the type without re-inferring */
    e->type = result;
    return result;
}

/* ---------------------------------------------------------
   STATEMENT VALIDATION
   --------------------------------------------------------- */

/**
 * @brief Validates statement logic and manages symbol visibility.
 */
static void sem_stmt(Stmt *s, SymTable *sym) {
    if (!s) return;

    switch (s->kind) {
    case S_DECL: {
        Type t = s->v.decl.type;

        if (s->v.decl.init) {
            Type init_t = infer_expr(s->v.decl.init, sym);

            /* Type Inference: let x = 5; (t starts as UNKNOWN) */
            if (t.kind == TY_UNKNOWN) {
                t = init_t;
            } else {
                /* Type Checking: let x: int = "string"; (Mismatch) */
                if (!(t.kind == init_t.kind || (t.kind == TY_REF && init_t.kind == TY_REF))) {
                    errorf("Type mismatch in declaration of '%s' at %d:%d\n",
                           s->v.decl.name, s->line, s->col);
                    exit(1);
                }
            }
        }
        sym_add(sym, s->v.decl.name, t, s->line);
        break;
    }

    case S_EXPR:
        infer_expr(s->v.expr, sym);
        break;

    case S_BLOCK: {
        /* Create a local scope by passing the current head */
        SymTable local = { .head = sym->head };
        for (int i = 0; i < s->v.block.n; i++)
            sem_stmt(s->v.block.stmts[i], &local);
        break;
    }

    case S_IF:
        infer_expr(s->v.ifs.cond, sym);
        sem_stmt(s->v.ifs.then_s, sym);
        if (s->v.ifs.else_s) sem_stmt(s->v.ifs.else_s, sym);
        break;

    case S_WHILE:
        infer_expr(s->v.wh.cond, sym);
        sem_stmt(s->v.wh.body, sym);
        break;

    default:
        break;
    }
}

/* ---------------------------------------------------------
   ENTRY POINT
   --------------------------------------------------------- */

void semantic_check(Function *f, const char *filename) {
    (void)filename;
    SymTable st = { .head = NULL };
    sem_stmt(f->body, &st);
}