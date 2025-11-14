/* src/semantic.c
 * Very small semantic checker:
 * - type checking
 * - symbol table for basic variables
 * - infers types for expressions
 *
 * This version also *annotates* Expr nodes with their inferred Type
 * so backend can use e->type to select correct runtime calls.
 */

#include "../include/semantic.h"
#include "../include/ast.h"
#include "../include/common.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>

typedef struct Sym {
    char name[MAX_IDENT];
    Type type;
    int defined_line;
    struct Sym *next;
} Sym;

typedef struct {
    Sym *head;
} SymTable;

/* ----------------------------------------- */
/* Symbol Table Helpers */
/* ----------------------------------------- */

static void sym_add(SymTable *t, const char *name, Type ty, int line) {
    Sym *s = xmalloc(sizeof(Sym));
    strncpy(s->name, name, MAX_IDENT - 1);
    s->name[MAX_IDENT - 1] = 0;

    s->type = ty;
    s->defined_line = line;
    s->next = t->head;
    t->head = s;
}

static Sym* sym_find(SymTable *t, const char *name) {
    for (Sym *s = t->head; s; s = s->next) {
        if (strcmp(s->name, name) == 0)
            return s;
    }
    return NULL;
}

/* ----------------------------------------- */
/* Type Inference + annotate Expr->type */
/* ----------------------------------------- */

static Type infer_expr(Expr *e, SymTable *sym) {
    if (!e) return mktype(TY_UNKNOWN);

    Type result = mktype(TY_UNKNOWN);

    switch (e->kind) {

    /* --------------------------- */
    case E_INT_LIT:
        result = mktype(TY_INT);
        break;

    /* --------------------------- */
    case E_STR_LIT:
        result = mktype(TY_STRING);
        break;

    /* --------------------------- */
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

    /* --------------------------- */
    case E_ADDR: {
        Type inner = infer_expr(e->v.inner, sym);
        Type *p = xmalloc(sizeof(Type));
        *p = inner;
        result = mkref(TY_REF, p);
        break;
    }

    /* --------------------------- */
    case E_MUTADDR: {
        Type inner = infer_expr(e->v.inner, sym);
        Type *p = xmalloc(sizeof(Type));
        *p = inner;
        result = mkref(TY_MUTREF, p);
        break;
    }

    /* --------------------------- */
    case E_CALL: {
        const char *fn = e->v.call.name;

        if (strcmp(fn, "clone") == 0) {
            if (e->v.call.nargs != 1) {
                errorf("clone() expects exactly 1 argument at %d:%d\n",
                       e->line, e->col);
                exit(1);
            }

            Type arg = infer_expr(e->v.call.args[0], sym);
            if (arg.kind != TY_STRING) {
                errorf("clone() only implemented for string types at %d:%d\n",
                       e->line, e->col);
                exit(1);
            }

            result = mktype(TY_STRING);
            break;
        }

        if (strcmp(fn, "print") == 0) {
            if (e->v.call.nargs != 1) {
                errorf("print() expects exactly 1 argument at %d:%d\n",
                       e->line, e->col);
                exit(1);
            }

            /* infer arg (and annotate it) */
            Type arg = infer_expr(e->v.call.args[0], sym);
            (void)arg;
            /* print returns int (convention) */
            result = mktype(TY_INT);
            break;
        }

        errorf("Unknown function '%s' at %d:%d\n", fn, e->line, e->col);
        exit(1);
    }

    /* --------------------------- */
    default:
        errorf("Semantic: unsupported expr kind %d at %d:%d\n",
               e->kind, e->line, e->col);
        exit(1);
    }

    /* annotate node so backend can use e->type */
    e->type = result;
    return result;
}

/* ----------------------------------------- */
/* Statement Checker (annotates vars/types) */
/* ----------------------------------------- */

static void sem_stmt(Stmt *s, SymTable *sym) {
    if (!s) return;

    switch (s->kind) {

    /* --------------------------- */
    case S_DECL: {
        Type t = s->v.decl.type;

        /* if initializer present and declared type unknown, infer and adopt that type */
        if (s->v.decl.init) {
            Type init_t = infer_expr(s->v.decl.init, sym);

            /* If declared TY_UNKNOWN, adopt inferred type */
            if (t.kind == TY_UNKNOWN) {
                t = init_t;
            } else {
                /* If declared type exists, check compatibility (basic) */
                if (!(t.kind == init_t.kind ||
                      (t.kind == TY_REF && init_t.kind == TY_REF) )) {
                    errorf("Type mismatch in declaration of '%s' at %d:%d\n",
                           s->v.decl.name, s->line, s->col);
                    exit(1);
                }
            }
        }

        sym_add(sym, s->v.decl.name, t, s->line);
        break;
    }

    /* --------------------------- */
    case S_EXPR:
        /* annotate expression */
        infer_expr(s->v.expr, sym);
        break;

    /* --------------------------- */
    case S_BLOCK: {
        /* simple scoping: create a local table that points to same head so lookups find outer symbols.
           This minimal model doesn't support shadowing well; extending would require push/pop. */
        SymTable local;
        local.head = sym->head;

        for (int i = 0; i < s->v.block.n; i++)
            sem_stmt(s->v.block.stmts[i], &local);
        break;
    }

    /* --------------------------- */
    case S_IF:
        infer_expr(s->v.ifs.cond, sym);
        sem_stmt(s->v.ifs.then_s, sym);
        if (s->v.ifs.else_s)
            sem_stmt(s->v.ifs.else_s, sym);
        break;

    /* --------------------------- */
    case S_WHILE:
        infer_expr(s->v.wh.cond, sym);
        sem_stmt(s->v.wh.body, sym);
        break;

    /* --------------------------- */
    default:
        break;
    }
}

/* ----------------------------------------- */
/* Entry Point */
/* ----------------------------------------- */

void semantic_check(Function *f, const char *filename) {
    (void)filename;

    SymTable st = { .head = NULL };

    sem_stmt(f->body, &st);

    /* In this mini-compiler, no return type checking for main(). */
}