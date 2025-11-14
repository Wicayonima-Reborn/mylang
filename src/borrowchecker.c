/* 
 * A compile-time borrow checker for a subset of MyLang.
 *
 * Enforces:
 *  - move semantics (let b = a; a invalid afterward)
 *  - immutable borrows (&a) → allow multiple if no mutable borrow exists
 *  - mutable borrow (&mut a) → exclusive; disallow if any borrow exists
 *  - use after move → error
 *  - borrow of moved variable → error
 *
 * This is a simplified version similar to Rust's borrow checker.
 */

#include "../include/borrowchecker.h"
#include "../include/ast.h"
#include "../include/common.h"
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

typedef struct VarInfo {
    char name[MAX_IDENT];
    Type type;

    bool valid;        // invalid after move
    int imm_count;     // number of &a borrows
    bool mut_borrowed; // true if &mut a is active

    int scope_depth;
    struct VarInfo *next;
} VarInfo;

typedef struct {
    VarInfo *vars;
    int depth;
    const char *file;
} BCState;

/* ---------------------------------------------- */
/* Helpers */
/* ---------------------------------------------- */

static VarInfo* find_var(BCState *s, const char *name) {
    for (VarInfo *v = s->vars; v; v = v->next) {
        if (strcmp(v->name, name) == 0)
            return v;
    }
    return NULL;
}

static void add_var(BCState *s, const char *name, Type t) {
    VarInfo *v = xmalloc(sizeof(VarInfo));

    strncpy(v->name, name, MAX_IDENT - 1);
    v->name[MAX_IDENT - 1] = 0;

    v->type = t;
    v->valid = true;
    v->imm_count = 0;
    v->mut_borrowed = false;
    v->scope_depth = s->depth;

    v->next = s->vars;
    s->vars = v;
}

static void bc_error(BCState *s, int line, int col, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    fprintf(stderr, "%s:%d:%d: borrow error: ",
            s->file ? s->file : "<input>", line, col);

    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");

    va_end(ap);
    exit(1);
}

/* ---------------------------------------------- */
/* Forward decls */
/* ---------------------------------------------- */

static void visit_stmt(BCState *s, Stmt *st);
static void visit_expr(BCState *s, Expr *e);

/* ---------------------------------------------- */
/* Expr Checker */
/* ---------------------------------------------- */

static void visit_expr(BCState *s, Expr *e) {
    if (!e) return;

    switch (e->kind) {

    case E_IDENT: {
        VarInfo *v = find_var(s, e->v.ident);
        if (!v)
            bc_error(s, e->line, e->col,
                     "use of undeclared variable '%s'", e->v.ident);
        if (!v->valid)
            bc_error(s, e->line, e->col,
                     "use of moved value '%s'", e->v.ident);
        break;
    }

    case E_CALL: {
        for (int i = 0; i < e->v.call.nargs; i++)
            visit_expr(s, e->v.call.args[i]);
        break;
    }

    case E_ADDR:
    case E_MUTADDR:
        visit_expr(s, e->v.inner);
        break;

    default:
        break;
    }
}

/* ---------------------------------------------- */
/* Stmt Checker */
/* ---------------------------------------------- */

static void visit_stmt(BCState *s, Stmt *st) {
    if (!st) return;

    switch (st->kind) {

    /* ------------------------------- */
    case S_DECL: {

        /* ----- Initializer: MOVE, BORROW ----- */
        if (st->v.decl.init) {
            Expr *init = st->v.decl.init;

            /* MOVE: let x = y; */
            if (init->kind == E_IDENT) {
                const char *src = init->v.ident;
                VarInfo *v = find_var(s, src);

                if (!v)
                    bc_error(s, st->line, st->col,
                             "use of undeclared '%s'", src);
                if (!v->valid)
                    bc_error(s, st->line, st->col,
                             "use of moved value '%s'", src);

                if (v->imm_count > 0 || v->mut_borrowed)
                    bc_error(s, st->line, st->col,
                             "cannot move '%s' because it is borrowed", src);

                /* apply move */
                v->valid = false;
            }

            /* IMM BORROW: let r = &x; */
            else if (init->kind == E_ADDR) {
                Expr *inner = init->v.inner;
                if (inner->kind != E_IDENT)
                    bc_error(s, st->line, st->col,
                             "cannot borrow from non-identifier");

                const char *name = inner->v.ident;
                VarInfo *v = find_var(s, name);

                if (!v)
                    bc_error(s, st->line, st->col,
                             "borrow of undeclared '%s'", name);
                if (!v->valid)
                    bc_error(s, st->line, st->col,
                             "borrow of moved value '%s'", name);
                if (v->mut_borrowed)
                    bc_error(s, st->line, st->col,
                             "cannot immutably borrow '%s' because it is mutably borrowed",
                             name);

                v->imm_count++;
            }

            /* MUT BORROW: let r = &mut x; */
            else if (init->kind == E_MUTADDR) {
                Expr *inner = init->v.inner;
                if (inner->kind != E_IDENT)
                    bc_error(s, st->line, st->col,
                             "cannot mutably borrow non-identifier");

                const char *name = inner->v.ident;
                VarInfo *v = find_var(s, name);

                if (!v)
                    bc_error(s, st->line, st->col,
                             "mut borrow of undeclared '%s'", name);
                if (!v->valid)
                    bc_error(s, st->line, st->col,
                             "mut borrow of moved value '%s'", name);
                if (v->imm_count > 0 || v->mut_borrowed)
                    bc_error(s, st->line, st->col,
                             "cannot mutably borrow '%s' because it is already borrowed",
                             name);

                v->mut_borrowed = true;
            }

            /* Other initializers (literal, call) */
            else {
                visit_expr(s, init);
            }
        }

        /* declare the variable AFTER analyzing RHS */
        add_var(s, st->v.decl.name, st->v.decl.type);

        break;
    }

    /* ------------------------------- */
    case S_EXPR:
        visit_expr(s, st->v.expr);
        break;

    /* ------------------------------- */
    case S_BLOCK: {
        s->depth++;

        for (int i = 0; i < st->v.block.n; i++)
            visit_stmt(s, st->v.block.stmts[i]);

        /* drop variables in this scope */
        VarInfo **pp = &s->vars;
        while (*pp) {
            if ((*pp)->scope_depth == s->depth) {
                VarInfo *to_free = *pp;
                *pp = (*pp)->next;
                free(to_free);
                continue;
            }
            pp = &(*pp)->next;
        }

        s->depth--;
        break;
    }

    /* ------------------------------- */
    case S_IF:
        visit_expr(s, st->v.ifs.cond);
        visit_stmt(s, st->v.ifs.then_s);
        if (st->v.ifs.else_s)
            visit_stmt(s, st->v.ifs.else_s);
        break;

    /* ------------------------------- */
    case S_WHILE:
        visit_expr(s, st->v.wh.cond);
        visit_stmt(s, st->v.wh.body);
        break;

    default:
        break;
    }
}

/* ---------------------------------------------- */
/* Entry point */
/* ---------------------------------------------- */

void borrow_check(Function *f, const char *filename) {
    BCState s = {
        .vars = NULL,
        .depth = 0,
        .file = filename
    };

    visit_stmt(&s, f->body);
}