#ifndef AST_H
#define AST_H

#include "common.h"

/* AST node kinds */
typedef enum {
    S_DECL, S_EXPR, S_IF, S_WHILE, S_BLOCK, S_RETURN
} StmtKind;

typedef enum {
    E_INT_LIT, E_STR_LIT, E_IDENT, E_BINOP, E_CALL, E_ADDR, E_MUTADDR
} ExprKind;

typedef struct Expr {
    ExprKind kind;
    int line, col;
    Type type; // filled by semantic
    union {
        long int_val;
        char *str_val;
        char ident[MAX_IDENT];
        struct { char op; struct Expr *l, *r; } bin;
        struct { char name[MAX_IDENT]; struct Expr **args; int nargs; } call;
        struct Expr *inner; // for & and &mut
    } v;
} Expr;

typedef struct Stmt {
    StmtKind kind;
    int line, col;
    union {
        struct { char name[MAX_IDENT]; Type type; Expr *init; } decl;
        Expr *expr;
        struct { Expr *cond; struct Stmt *then_s; struct Stmt *else_s; } ifs;
        struct { Expr *cond; struct Stmt *body; } wh;
        struct { struct Stmt **stmts; int n; } block;
        Expr *ret;
    } v;
} Stmt;

typedef struct Function {
    char name[MAX_IDENT];
    Type ret_type;
    // currently no params for simplicity (main only)
    Stmt *body;
} Function;

/* AST ctor helpers */
Expr *expr_int(long v, int line, int col);
Expr *expr_str(const char *s, int line, int col);
Expr *expr_ident(const char *s, int line, int col);
Expr *expr_addr(Expr *inner, bool mut, int line, int col);
Expr *expr_call(const char *name, Expr **args, int nargs, int line, int col);
Stmt *stmt_decl(const char *name, Type t, Expr *init, int line, int col);
Stmt *stmt_expr(Expr *e, int line, int col);
Stmt *stmt_block(Stmt **stmts, int n, int line, int col);
Stmt *stmt_if(Expr *cond, Stmt *then_s, Stmt *else_s, int line, int col);
Stmt *stmt_while(Expr *cond, Stmt *body, int line, int col);
Function *make_main(Stmt *body);

void ast_free_function(Function *f);
void ast_print_function(Function *f);

#endif