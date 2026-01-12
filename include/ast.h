#ifndef AST_H
#define AST_H

#include "common.h"

/**
 * @enum StmtKind
 * @brief Categorization of statement nodes within the Abstract Syntax Tree.
 */
typedef enum {
    S_DECL,     /* Variable declaration and initialization */
    S_EXPR,     /* Expression-based statement (e.g., assignments, side effects) */
    S_IF,       /* Conditional control flow (if-else) */
    S_WHILE,    /* Pre-condition iteration (while loop) */
    S_FOR,      /* Iterator-based iteration (for loop) */
    S_BLOCK,    /* Compound statement / Lexical scope block */
    S_RETURN,   /* Subroutine termination and value passing */
    S_BREAK,    /* Immediate loop termination */
    S_CONTINUE  /* Skip to next loop iteration */
} StmtKind;

/**
 * @enum ExprKind
 * @brief Categorization of expression nodes and literal types.
 */
typedef enum {
    E_INT_LIT,  /* Integer constant literal */
    E_STR_LIT,  /* String constant literal */
    E_IDENT,    /* Symbolic identifier (variable/function reference) */
    E_BINOP,    /* Binary operation (infix) */
    E_CALL,     /* Subroutine invocation */
    E_ADDR,     /* Immutable reference operator (&) */
    E_MUTADDR,  /* Mutable reference operator (&mut) */
    E_RANGE,    /* Interval/Range expression (e.g., start..end) */
    E_ARRAY_LIT,/* Array initializer list */
    E_INDEX     /* Subscript/Element access operation */
} ExprKind;

/**
 * @struct Expr
 * @brief Representation of an expression node.
 * Contains metadata for semantic analysis and a union of specific expression data.
 */
typedef struct Expr {
    ExprKind kind;
    int line, col;
    Type type;      /* Populated during the semantic analysis phase */

    union {
        long int_val;
        char *str_val;
        char ident[MAX_IDENT];

        struct { char op; struct Expr *l, *r; } bin;

        struct {
            char name[MAX_IDENT];
            struct Expr **args;
            int nargs;
        } call;

        struct Expr *inner; /* Target of unary reference operations */

        struct {
            struct Expr *start;
            struct Expr *end;
        } range;

        struct {
            struct Expr **items;
            int count;
        } array;

        struct {
            struct Expr *array;
            struct Expr *index;
        } index;
    } v;
} Expr;

/**
 * @struct Stmt
 * @brief Representation of a statement node.
 * Encapsulates program control flow and state declarations.
 */
typedef struct Stmt {
    StmtKind kind;
    int line, col;

    union {
        struct {
            char name[MAX_IDENT];
            Type type;
            Expr *init;
        } decl;

        Expr *expr;

        struct {
            Expr *cond;
            struct Stmt *then_s;
            struct Stmt *else_s;
        } ifs;

        struct {
            Expr *cond;
            struct Stmt *body;
        } wh;

        struct {
            char var[MAX_IDENT];
            Expr *iter;
            struct Stmt *body;
        } fors;

        struct {
            struct Stmt **stmts;
            int n;
        } block;

        Expr *ret;
    } v;
} Stmt;

/**
 * @struct Function
 * @brief High-level function definition.
 * Represents a single compilation unit consisting of a signature and a body.
 */
typedef struct Function {
    char name[MAX_IDENT];
    Type ret_type;
    Stmt *body;
} Function;

/* ---------------------------------------------------------
   AST Constructors (Factory Methods)
   Functions for heap allocation and node initialization.
   --------------------------------------------------------- */

Expr *expr_int(long v, int line, int col);
Expr *expr_str(const char *s, int line, int col);
Expr *expr_ident(const char *s, int line, int col);
Expr *expr_addr(Expr *inner, bool mut, int line, int col);
Expr *expr_call(const char *name, Expr **args, int nargs, int line, int col);
Expr *expr_range(Expr *start, Expr *end, int line, int col);
Expr *expr_array(Expr **items, int count, int line, int col);
Expr *expr_index(Expr *array, Expr *index, int line, int col);

Stmt *stmt_decl(const char *name, Type t, Expr *init, int line, int col);
Stmt *stmt_expr(Expr *e, int line, int col);
Stmt *stmt_block(Stmt **stmts, int n, int line, int col);
Stmt *stmt_if(Expr *cond, Stmt *then_s, Stmt *else_s, int line, int col);
Stmt *stmt_while(Expr *cond, Stmt *body, int line, int col);
Stmt *stmt_for(const char *var, Expr *iter, Stmt *body, int line, int col);

Function *make_main(Stmt *body);

/* ---------------------------------------------------------
   Management and Debug Utilities
   --------------------------------------------------------- */

/** Performs recursive deallocation of the function's AST. */
void ast_free_function(Function *f);

/** Outputs a formatted text representation of the AST to stdout. */
void ast_print_function(Function *f);

#endif