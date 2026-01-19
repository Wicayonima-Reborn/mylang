/**
 * @file borrowchecker.c
 * @brief Static analysis pass for enforcing ownership and borrowing rules.
 * * Rules Enforced:
 * 1. Move Semantics: Values are moved on assignment; the source becomes invalid.
 * 2. Shared Borrowing: Multiple immutable references (&T) allowed.
 * 3. Exclusive Borrowing: Only one mutable reference (&mut T) allowed.
 * 4. Mutual Exclusion: Cannot mutably borrow if shared borrows exist, and vice versa.
 */

#include "../include/borrowchecker.h"
#include "../include/ast.h"
#include "../include/common.h"

#include <string.h>
#include <stdarg.h>
#include <stdlib.h>

/**
 * @struct VarInfo
 * @brief Tracks the borrow state and validity of a variable within a scope.
 */
typedef struct VarInfo {
    char name[MAX_IDENT];
    Type type;

    bool valid;         /* False if the value has been moved */
    int imm_count;      /* Active count of immutable references */
    bool mut_borrowed;  /* True if a mutable reference is active */

    int scope_depth;
    struct VarInfo *next;
} VarInfo;

/**
 * @struct BCState
 * @brief Context for the Borrow Checker traversal.
 */
typedef struct {
    VarInfo *vars;      /* Linked list of variables in the current symbol table */
    int depth;          /* Current lexical nesting level */
    const char *file;   /* Source filename for error reporting */
} BCState;

/* ---------------------------------------------------------
   STATE MANAGEMENT HELPERS
   --------------------------------------------------------- */

/** Retrieves variable metadata from the current context by name. */
static VarInfo *find_var(BCState *s, const char *name) {
    for (VarInfo *v = s->vars; v; v = v->next) {
        if (strcmp(v->name, name) == 0)
            return v;
    }
    return NULL;
}

/** Registers a new variable into the current scope. */
static void add_var(BCState *s, const char *name, Type t) {
    VarInfo *v = xmalloc(sizeof(VarInfo));

    snprintf(v->name, sizeof(v->name), "%s", name);
    v->type = t;
    v->valid = true;
    v->imm_count = 0;
    v->mut_borrowed = false;
    v->scope_depth = s->depth;

    /* Prepend to the linked list (shadowing support) */
    v->next = s->vars;
    s->vars = v;
}

/** Reports a borrow-check violation and terminates compilation. */
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

/* Forward declarations for recursive traversal */
static void visit_stmt(BCState *s, Stmt *st);
static void visit_expr(BCState *s, Expr *e);

/* ---------------------------------------------------------
   EXPRESSION ANALYSIS
   --------------------------------------------------------- */

/**
 * @brief Checks for use-after-move and validity of expressions.
 */
static void visit_expr(BCState *s, Expr *e) {
    if (!e) return;

    switch (e->kind) {

    case E_IDENT: {
        VarInfo *v = find_var(s, e->v.ident);
        if (!v)
            bc_error(s, e->line, e->col, "use of undeclared variable '%s'", e->v.ident);
        
        /* Check Move Semantics */
        if (!v->valid)
            bc_error(s, e->line, e->col, "use of moved value '%s'", e->v.ident);
        break;
    }

    case E_CALL:
        for (int i = 0; i < e->v.call.nargs; i++)
            visit_expr(s, e->v.call.args[i]);
        break;

    case E_ADDR:
    case E_MUTADDR:
        /* Recursive check of the expression being referenced */
        visit_expr(s, e->v.inner);
        break;

    default:
        break;
    }
}

/* ---------------------------------------------------------
   STATEMENT ANALYSIS
   --------------------------------------------------------- */

/**
 * @brief Analyzes statements to enforce move and borrow boundaries.
 */
static void visit_stmt(BCState *s, Stmt *st) {
    if (!st) return;

    switch (st->kind) {

    case S_DECL: {
        if (st->v.decl.init) {
            Expr *init = st->v.decl.init;

            /* RULE: MOVE SEMANTICS (let x = y) */
            if (init->kind == E_IDENT) {
                const char *src = init->v.ident;
                VarInfo *v = find_var(s, src);

                if (!v) bc_error(s, st->line, st->col, "use of undeclared '%s'", src);
                if (!v->valid) bc_error(s, st->line, st->col, "use of moved value '%s'", src);
                
                /* Check if source is currently borrowed */
                if (v->imm_count > 0 || v->mut_borrowed)
                    bc_error(s, st->line, st->col, "cannot move '%s' because it is borrowed", src);

                v->valid = false; /* Invalidate source after move */
            }

            /* RULE: SHARED BORROW (let r = &x) */
            else if (init->kind == E_ADDR) {
                Expr *inner = init->v.inner;
                if (inner->kind != E_IDENT)
                    bc_error(s, st->line, st->col, "cannot borrow from non-identifier");

                VarInfo *v = find_var(s, inner->v.ident);
                if (!v) bc_error(s, st->line, st->col, "borrow of undeclared '%s'", inner->v.ident);
                if (!v->valid) bc_error(s, st->line, st->col, "borrow of moved value '%s'", inner->v.ident);
                
                /* Conflict: Existing mutable borrow */
                if (v->mut_borrowed)
                    bc_error(s, st->line, st->col, "cannot shared-borrow '%s' while mutably borrowed", inner->v.ident);

                v->imm_count++;
            }

            /* RULE: MUTABLE BORROW (let r = &mut x) */
            else if (init->kind == E_MUTADDR) {
                Expr *inner = init->v.inner;
                if (inner->kind != E_IDENT)
                    bc_error(s, st->line, st->col, "cannot mutably borrow non-identifier");

                VarInfo *v = find_var(s, inner->v.ident);
                if (!v) bc_error(s, st->line, st->col, "mut borrow of undeclared '%s'", inner->v.ident);
                if (!v->valid) bc_error(s, st->line, st->col, "mut borrow of moved value '%s'", inner->v.ident);
                
                /* Conflict: Any existing borrow (shared or mutable) */
                if (v->imm_count > 0 || v->mut_borrowed)
                    bc_error(s, st->line, st->col, "cannot mutably borrow '%s' (already borrowed)", inner->v.ident);

                v->mut_borrowed = true;
            }
            else {
                visit_expr(s, init);
            }
        }

        add_var(s, st->v.decl.name, st->v.decl.type);
        break;
    }

    case S_BLOCK: {
        s->depth++;
        for (int i = 0; i < st->v.block.n; i++)
            visit_stmt(s, st->v.block.stmts[i]);

        /* Scope Exit: Cleanup variables defined at this depth */
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

    case S_IF:
        visit_expr(s, st->v.ifs.cond);
        visit_stmt(s, st->v.ifs.then_s);
        if (st->v.ifs.else_s) visit_stmt(s, st->v.ifs.else_s);
        break;

    case S_WHILE:
        visit_expr(s, st->v.wh.cond);
        visit_stmt(s, st->v.wh.body);
        break;

    default:
        break;
    }
}

/* ---------------------------------------------------------
   ENTRY POINT
   --------------------------------------------------------- */

/**
 * @brief Public interface to trigger the borrow check analysis.
 */
void borrow_check(Function *f, const char *filename) {
    BCState s = {
        .vars = NULL,
        .depth = 0,
        .file = filename
    };

    visit_stmt(&s, f->body);
}