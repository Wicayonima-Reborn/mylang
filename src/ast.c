#include "../include/ast.h"
#include "../include/common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** ---------------------------------------------------------
 * EXPRESSION CONSTRUCTORS
 * Allocation and initialization of expression-type nodes.
 * --------------------------------------------------------- */

Expr *expr_int(long v, int line, int col) {
    Expr *e = xmalloc(sizeof(Expr));
    e->kind = E_INT_LIT;
    e->line = line;
    e->col = col;
    e->type = mktype(TY_INT);
    e->v.int_val = v;
    return e;
}

Expr *expr_str(const char *s, int line, int col) {
    Expr *e = xmalloc(sizeof(Expr));
    e->kind = E_STR_LIT;
    e->line = line;
    e->col = col;
    e->type = mktype(TY_STRING);
    e->v.str_val = strdup(s);
    return e;
}

Expr *expr_ident(const char *s, int line, int col) {
    Expr *e = xmalloc(sizeof(Expr));
    e->kind = E_IDENT;
    e->line = line;
    e->col = col;
    e->type = mktype(TY_UNKNOWN);
    strncpy(e->v.ident, s, MAX_IDENT - 1);
    e->v.ident[MAX_IDENT - 1] = 0;
    return e;
}

Expr *expr_addr(Expr *inner, bool mut, int line, int col) {
    Expr *e = xmalloc(sizeof(Expr));
    e->kind = mut ? E_MUTADDR : E_ADDR;
    e->line = line;
    e->col = col;
    e->type = mktype(TY_UNKNOWN);
    e->v.inner = inner;
    return e;
}

Expr *expr_call(const char *name, Expr **args, int nargs, int line, int col) {
    Expr *e = xmalloc(sizeof(Expr));
    e->kind = E_CALL;
    e->line = line;
    e->col = col;
    e->type = mktype(TY_UNKNOWN);

    strncpy(e->v.call.name, name, MAX_IDENT - 1);
    e->v.call.name[MAX_IDENT - 1] = 0;
    e->v.call.args = args;
    e->v.call.nargs = nargs;
    return e;
}

Expr *expr_range(Expr *start, Expr *end, int line, int col) {
    Expr *e = xmalloc(sizeof(Expr));
    e->kind = E_RANGE;
    e->line = line;
    e->col = col;
    e->type = mktype(TY_UNKNOWN);
    e->v.range.start = start;
    e->v.range.end = end;
    return e;
}

Expr *expr_array(Expr **items, int count, int line, int col) {
    Expr *e = xmalloc(sizeof(Expr));
    e->kind = E_ARRAY_LIT;
    e->line = line;
    e->col = col;
    e->type = mktype(TY_UNKNOWN);
    e->v.array.items = items;
    e->v.array.count = count;
    return e;
}

Expr *expr_index(Expr *array, Expr *index, int line, int col) {
    Expr *e = xmalloc(sizeof(Expr));
    e->kind = E_INDEX;
    e->line = line;
    e->col = col;
    e->type = mktype(TY_UNKNOWN);
    e->v.index.array = array;
    e->v.index.index = index;
    return e;
}

/** ---------------------------------------------------------
 * STATEMENT CONSTRUCTORS
 * Allocation and initialization of statement-type nodes.
 * --------------------------------------------------------- */

Stmt *stmt_decl(const char *name, Type t, Expr *init, int line, int col) {
    Stmt *s = xmalloc(sizeof(Stmt));
    s->kind = S_DECL;
    s->line = line;
    s->col = col;
    strncpy(s->v.decl.name, name, MAX_IDENT - 1);
    s->v.decl.name[MAX_IDENT - 1] = 0;
    s->v.decl.type = t;
    s->v.decl.init = init;
    return s;
}

Stmt *stmt_expr(Expr *e, int line, int col) {
    Stmt *s = xmalloc(sizeof(Stmt));
    s->kind = S_EXPR;
    s->line = line;
    s->col = col;
    s->v.expr = e;
    return s;
}

Stmt *stmt_block(Stmt **stmts, int n, int line, int col) {
    Stmt *s = xmalloc(sizeof(Stmt));
    s->kind = S_BLOCK;
    s->line = line;
    s->col = col;
    s->v.block.stmts = stmts;
    s->v.block.n = n;
    return s;
}

Stmt *stmt_if(Expr *cond, Stmt *then_s, Stmt *else_s, int line, int col) {
    Stmt *s = xmalloc(sizeof(Stmt));
    s->kind = S_IF;
    s->line = line;
    s->col = col;
    s->v.ifs.cond = cond;
    s->v.ifs.then_s = then_s;
    s->v.ifs.else_s = else_s;
    return s;
}

Stmt *stmt_while(Expr *cond, Stmt *body, int line, int col) {
    Stmt *s = xmalloc(sizeof(Stmt));
    s->kind = S_WHILE;
    s->line = line;
    s->col = col;
    s->v.wh.cond = cond;
    s->v.wh.body = body;
    return s;
}

Stmt *stmt_for(const char *var, Expr *iter, Stmt *body, int line, int col) {
    Stmt *s = xmalloc(sizeof(Stmt));
    s->kind = S_FOR;
    s->line = line;
    s->col = col;
    strncpy(s->v.fors.var, var, MAX_IDENT - 1);
    s->v.fors.var[MAX_IDENT - 1] = 0;
    s->v.fors.iter = iter;
    s->v.fors.body = body;
    return s;
}

/** ---------------------------------------------------------
 * FUNCTION DEFINITION
 * --------------------------------------------------------- */

Function *make_main(Stmt *body) {
    Function *f = xmalloc(sizeof(Function));
    strncpy(f->name, "main", MAX_IDENT - 1);
    f->name[MAX_IDENT - 1] = 0;
    f->ret_type = mktype(TY_INT);
    f->body = body;
    return f;
}

/** ---------------------------------------------------------
 * PRETTY PRINTER (DEBUG UTILS)
 * Visualizes the AST structure with indentation-based nesting.
 * --------------------------------------------------------- */

/** Generates indentation for nested tree visualization. */
static void indentf(int n) {
    for (int i = 0; i < n; i++) printf("  ");
}

static void print_expr(Expr *e, int indent);

/** Recursively prints statement nodes and their child expressions. */
static void print_stmt(Stmt *s, int indent) {
    indentf(indent);

    switch (s->kind) {
    case S_DECL:
        printf("DECL %s\n", s->v.decl.name);
        if (s->v.decl.init)
            print_expr(s->v.decl.init, indent + 1);
        break;

    case S_EXPR:
        printf("EXPR\n");
        print_expr(s->v.expr, indent + 1);
        break;

    case S_BLOCK:
        printf("BLOCK (%d stmts)\n", s->v.block.n);
        for (int i = 0; i < s->v.block.n; i++)
            print_stmt(s->v.block.stmts[i], indent + 1);
        break;

    case S_IF:
        printf("IF\n");
        print_expr(s->v.ifs.cond, indent + 1);
        print_stmt(s->v.ifs.then_s, indent + 1);
        if (s->v.ifs.else_s)
            print_stmt(s->v.ifs.else_s, indent + 1);
        break;

    case S_WHILE:
        printf("WHILE\n");
        print_expr(s->v.wh.cond, indent + 1);
        print_stmt(s->v.wh.body, indent + 1);
        break;

    case S_FOR:
        printf("FOR %s\n", s->v.fors.var);
        print_expr(s->v.fors.iter, indent + 1);
        print_stmt(s->v.fors.body, indent + 1);
        break;

    default:
        printf("UNKNOWN STMT\n");
    }
}

/** Recursively prints expression nodes and literal values. */
static void print_expr(Expr *e, int indent) {
    indentf(indent);

    switch (e->kind) {
    case E_INT_LIT:
        printf("INT %ld\n", e->v.int_val);
        break;

    case E_STR_LIT:
        printf("STRING \"%s\"\n", e->v.str_val);
        break;

    case E_IDENT:
        printf("IDENT %s\n", e->v.ident);
        break;

    case E_ADDR:
        printf("&\n");
        print_expr(e->v.inner, indent + 1);
        break;

    case E_MUTADDR:
        printf("&mut\n");
        print_expr(e->v.inner, indent + 1);
        break;

    case E_RANGE:
        printf("RANGE\n");
        print_expr(e->v.range.start, indent + 1);
        print_expr(e->v.range.end, indent + 1);
        break;

    case E_ARRAY_LIT:
        printf("ARRAY (%d items)\n", e->v.array.count);
        for (int i = 0; i < e->v.array.count; i++)
            print_expr(e->v.array.items[i], indent + 1);
        break;

    case E_INDEX:
        printf("INDEX\n");
        print_expr(e->v.index.array, indent + 1);
        print_expr(e->v.index.index, indent + 1);
        break;

    case E_CALL:
        printf("CALL %s\n", e->v.call.name);
        for (int i = 0; i < e->v.call.nargs; i++)
            print_expr(e->v.call.args[i], indent + 1);
        break;

    default:
        printf("EXPR kind=%d\n", e->kind);
    }
}

void ast_print_function(Function *f) {
    printf("Function %s:\n", f->name);
    print_stmt(f->body, 1);
}

/** ---------------------------------------------------------
 * MEMORY MANAGEMENT
 * --------------------------------------------------------- */

void ast_free_function(Function *f) {
    /* TODO: Implement recursive memory deallocation logic */
    (void)f;
}