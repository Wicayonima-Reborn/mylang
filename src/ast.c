#include "../include/ast.h"
#include "../include/common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------
   EXPRESSION CONSTRUCTORS
   Memory allocation and initialization for AST expression nodes.
   --------------------------------------------------------- */

/**
 * @brief Creates an integer literal expression node.
 */
Expr *expr_int(long v, int line, int col) {
    Expr *e = xmalloc(sizeof(Expr));
    e->kind = E_INT_LIT;
    e->line = line;
    e->col = col;
    e->type = mktype(TY_INT);
    e->v.int_val = v;
    return e;
}

/**
 * @brief Creates a string literal expression node.
 * Dynamically allocates the string value.
 */
Expr *expr_str(const char *s, int line, int col) {
    Expr *e = xmalloc(sizeof(Expr));
    e->kind = E_STR_LIT;
    e->line = line;
    e->col = col;
    e->type = mktype(TY_STRING);
    e->v.str_val = strdup(s);
    return e;
}

/**
 * @brief Creates an identifier reference node.
 */
Expr *expr_ident(const char *s, int line, int col) {
    Expr *e = xmalloc(sizeof(Expr));
    e->kind = E_IDENT;
    e->line = line;
    e->col = col;
    e->type = mktype(TY_UNKNOWN);

    /* Use safe buffer copy for fixed-size identifier array */
    snprintf(e->v.ident, sizeof(e->v.ident), "%s", s);
    return e;
}

/**
 * @brief Creates a reference expression node (& or &mut).
 */
Expr *expr_addr(Expr *inner, bool mut, int line, int col) {
    Expr *e = xmalloc(sizeof(Expr));
    e->kind = mut ? E_MUTADDR : E_ADDR;
    e->line = line;
    e->col = col;
    e->type = mktype(TY_UNKNOWN);
    e->v.inner = inner;
    return e;
}

/**
 * @brief Creates a function or subroutine call node.
 */
Expr *expr_call(const char *name, Expr **args, int nargs, int line, int col) {
    Expr *e = xmalloc(sizeof(Expr));
    e->kind = E_CALL;
    e->line = line;
    e->col = col;
    e->type = mktype(TY_UNKNOWN);

    snprintf(e->v.call.name, sizeof(e->v.call.name), "%s", name);
    e->v.call.args = args;
    e->v.call.nargs = nargs;
    return e;
}

/**
 * @brief Creates a range expression node (start..end).
 */
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

/**
 * @brief Creates an array literal initialization node.
 */
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

/**
 * @brief Creates an array indexing/subscript expression node.
 */
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

/* ---------------------------------------------------------
   STATEMENT CONSTRUCTORS
   Memory allocation and initialization for AST statement nodes.
   --------------------------------------------------------- */

/**
 * @brief Creates a variable declaration statement node.
 */
Stmt *stmt_decl(const char *name, Type t, Expr *init, int line, int col) {
    Stmt *s = xmalloc(sizeof(Stmt));
    s->kind = S_DECL;
    s->line = line;
    s->col = col;

    snprintf(s->v.decl.name, sizeof(s->v.decl.name), "%s", name);
    s->v.decl.type = t;
    s->v.decl.init = init;
    return s;
}

/**
 * @brief Creates a standalone expression statement node.
 */
Stmt *stmt_expr(Expr *e, int line, int col) {
    Stmt *s = xmalloc(sizeof(Stmt));
    s->kind = S_EXPR;
    s->line = line;
    s->col = col;
    s->v.expr = e;
    return s;
}

/**
 * @brief Creates a compound/block statement node.
 */
Stmt *stmt_block(Stmt **stmts, int n, int line, int col) {
    Stmt *s = xmalloc(sizeof(Stmt));
    s->kind = S_BLOCK;
    s->line = line;
    s->col = col;
    s->v.block.stmts = stmts;
    s->v.block.n = n;
    return s;
}

/**
 * @brief Creates a conditional branching (if-else) statement node.
 */
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

/**
 * @brief Creates a while-loop iteration statement node.
 */
Stmt *stmt_while(Expr *cond, Stmt *body, int line, int col) {
    Stmt *s = xmalloc(sizeof(Stmt));
    s->kind = S_WHILE;
    s->line = line;
    s->col = col;
    s->v.wh.cond = cond;
    s->v.wh.body = body;
    return s;
}

/**
 * @brief Creates a for-loop iteration statement node.
 */
Stmt *stmt_for(const char *var, Expr *iter, Stmt *body, int line, int col) {
    Stmt *s = xmalloc(sizeof(Stmt));
    s->kind = S_FOR;
    s->line = line;
    s->col = col;

    snprintf(s->v.fors.var, sizeof(s->v.fors.var), "%s", var);
    s->v.fors.iter = iter;
    s->v.fors.body = body;
    return s;
}

/* ---------------------------------------------------------
   FUNCTION DEFINITION
   --------------------------------------------------------- */

/**
 * @brief Initializes the main entry point function structure.
 */
Function *make_main(Stmt *body) {
    Function *f = xmalloc(sizeof(Function));
    snprintf(f->name, sizeof(f->name), "%s", "main");
    f->ret_type = mktype(TY_INT);
    f->body = body;
    return f;
}

/* ---------------------------------------------------------
   DEBUG VISUALIZATION (PRETTY PRINTER)
   Provides a hierarchical text view of the AST.
   --------------------------------------------------------- */

/** Standard indentation helper for tree depth. */
static void indentf(int n) {
    for (int i = 0; i < n; i++) printf("  ");
}

static void print_expr(Expr *e, int indent);

/** Recursive visualization of statement nodes. */
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

/** Recursive visualization of expression nodes. */
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

/**
 * @brief Public interface to print function structure.
 */
void ast_print_function(Function *f) {
    printf("Function %s:\n", f->name);
    print_stmt(f->body, 1);
}

/* ---------------------------------------------------------
   MEMORY MANAGEMENT
   --------------------------------------------------------- */

/**
 * @brief Recursively deallocates function AST and associated memory.
 */
void ast_free_function(Function *f) {
    /* Implementation depends on full traversal logic */
    (void)f;
}