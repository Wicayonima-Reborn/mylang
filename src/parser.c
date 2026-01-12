#include "../include/parser.h"
#include "../include/lexer.h"
#include "../include/ast.h"
#include "../include/common.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/**
 * PARSER STATE
 * Global state for the recursive descent parser.
 */
static Token cur;
static Lexer L;

/* ---------------------------------------------------------
   PARSER UTILITIES
   Internal helper functions for token stream navigation.
   --------------------------------------------------------- */

/** Advances the lexer to the next token in the stream. */
static void nexttok() {
    cur = lexer_next(&L);
}

/** Checks if the current token matches the specified kind. */
static bool tok_is(TokenKind k) {
    return cur.kind == k;
}

/** * Enforces token matching. Advances on success, 
 * terminates with a diagnostic message on failure. 
 */
static void expect(TokenKind k, const char *msg) {
    if (!tok_is(k)) {
        errorf("Parse error at %d:%d: expected %s (got '%s')\n",
               cur.line, cur.col, msg, cur.lexeme);
    }
    nexttok();
}

/* Forward declarations for recursive descent */
static Expr* parse_expr();
static Expr* parse_primary();
static Stmt* parse_stmt();
static Stmt* parse_block();

/* ---------------------------------------------------------
   EXPRESSION PARSING
   Implements precedence climbing and primary atom parsing.
   --------------------------------------------------------- */

/**
 * @brief Parses primary expressions (literals, identifiers, calls, arrays).
 * Handles high-precedence postfix operators like indexing and function calls.
 */
static Expr* parse_primary() {
    int l = cur.line, c = cur.col;

    /* Integer constant literal */
    if (tok_is(T_INTLIT)) {
        long v = cur.int_val;
        nexttok();
        return expr_int(v, l, c);
    }

    /* String constant literal */
    if (tok_is(T_STRLIT)) {
        char *s = strdup(cur.lexeme);
        nexttok();
        return expr_str(s, l, c);
    }

    /* Identifier-based nodes: Variables, Function Calls, or Indexing */
    if (tok_is(T_IDENT) || tok_is(T_PRINT)) {
        char name[MAX_IDENT];
        strncpy(name, cur.lexeme, MAX_IDENT - 1);
        name[MAX_IDENT - 1] = 0;
        nexttok();

        Expr *base = expr_ident(name, l, c);

        /* Subroutine invocation (Function Call) */
        if (tok_is(T_LPAREN)) {
            nexttok();

            Expr **args = NULL;
            int nargs = 0;

            if (!tok_is(T_RPAREN)) {
                while (1) {
                    Expr *a = parse_expr();
                    args = realloc(args, sizeof(Expr*) * (nargs + 1));
                    args[nargs++] = a;

                    if (tok_is(T_COMMA)) {
                        nexttok();
                        continue;
                    }
                    break;
                }
            }

            expect(T_RPAREN, "')'");
            base = expr_call(name, args, nargs, l, c);
        }

        /* Postfix array indexing: <base>[<expr>] */
        while (tok_is(T_LBRACKET)) {
            nexttok();
            Expr *idx = parse_expr();
            expect(T_RBRACKET, "']'");
            base = expr_index(base, idx, l, c);
        }

        return base;
    }

    /* Array literal: [<expr>, <expr>, ...] */
    if (tok_is(T_LBRACKET)) {
        nexttok();

        Expr **items = NULL;
        int count = 0;

        if (!tok_is(T_RBRACKET)) {
            while (1) {
                Expr *e = parse_expr();
                items = realloc(items, sizeof(Expr*) * (count + 1));
                items[count++] = e;

                if (tok_is(T_COMMA)) {
                    nexttok();
                    continue;
                }
                break;
            }
        }

        expect(T_RBRACKET, "']'");
        return expr_array(items, count, l, c);
    }

    /* Unary reference operators: &expr or &mut expr */
    if (tok_is(T_AND) || tok_is(T_ANDMUT)) {
        bool mut = tok_is(T_ANDMUT);
        nexttok();
        Expr *inner = parse_primary();
        return expr_addr(inner, mut, l, c);
    }

    errorf("Unexpected token '%s' at %d:%d\n", cur.lexeme, l, c);
    return NULL;
}

/**
 * @brief General expression parser.
 * Currently handles binary range operators and delegates to primary parsing.
 */
static Expr* parse_expr() {
    Expr *lhs = parse_primary();

    /* Range expression: <expr> .. <expr> */
    if (tok_is(T_DOTDOT)) {
        int l = cur.line, c = cur.col;
        nexttok();
        Expr *rhs = parse_primary();
        return expr_range(lhs, rhs, l, c);
    }

    return lhs;
}

/* ---------------------------------------------------------
   STATEMENT PARSING
   Handles declarations and control flow structures.
   --------------------------------------------------------- */

/**
 * @brief Parses a single statement unit.
 */
static Stmt* parse_stmt() {
    int l = cur.line, c = cur.col;

    /* Variable declaration: let <id> [: <type>] [= <init>]; */
    if (tok_is(T_LET)) {
        nexttok();

        if (!tok_is(T_IDENT)) {
            errorf("Expected identifier after 'let' at %d:%d\n", l, c);
        }

        char name[MAX_IDENT];
        strncpy(name, cur.lexeme, MAX_IDENT - 1);
        name[MAX_IDENT - 1] = 0;
        nexttok();

        Type ty = mktype(TY_UNKNOWN);
        Expr *init = NULL;

        /* Optional explicit type annotation */
        if (tok_is(T_COLON)) {
            nexttok();
            if (tok_is(T_INT_TYPE)) {
                ty = mktype(TY_INT);
                nexttok();
            } else if (tok_is(T_STRING_TYPE)) {
                ty = mktype(TY_STRING);
                nexttok();
            } else {
                errorf("Unknown type at %d:%d\n", cur.line, cur.col);
            }
        }

        /* Optional assignment/initialization */
        if (tok_is(T_EQ)) {
            nexttok();
            init = parse_expr();
        }

        expect(T_SEMI, "';'");
        return stmt_decl(name, ty, init, l, c);
    }

    /* Iterator loop: for <var> in <iter> <block> */
    if (tok_is(T_FOR)) {
        nexttok();

        if (!tok_is(T_IDENT)) {
            errorf("Expected identifier after 'for' at %d:%d\n", l, c);
        }

        char var[MAX_IDENT];
        strncpy(var, cur.lexeme, MAX_IDENT - 1);
        var[MAX_IDENT - 1] = 0;
        nexttok();

        expect(T_IN, "'in'");
        Expr *iter = parse_expr();
        Stmt *body = parse_block();

        return stmt_for(var, iter, body, l, c);
    }

    /* Scoped block statement */
    if (tok_is(T_LBRACE)) {
        return parse_block();
    }

    /* Fallback: Expression statement */
    Expr *e = parse_expr();
    expect(T_SEMI, "';'");
    return stmt_expr(e, l, c);
}

/**
 * @brief Parses a compound statement (lexical block).
 */
static Stmt* parse_block() {
    int l = cur.line, c = cur.col;
    expect(T_LBRACE, "'{'");

    Stmt **list = NULL;
    int n = 0;

    while (!tok_is(T_RBRACE)) {
        if (tok_is(T_EOF)) {
            errorf("Unexpected EOF inside block\n");
        }
        Stmt *s = parse_stmt();
        list = realloc(list, sizeof(Stmt*) * (n + 1));
        list[n++] = s;
    }

    expect(T_RBRACE, "'}'");
    return stmt_block(list, n, l, c);
}

/* ---------------------------------------------------------
   PUBLIC INTERFACE
   --------------------------------------------------------- */

/**
 * @brief Top-level entry point for the parser.
 * Orchestrates lexing and AST construction for the entire source file.
 */
Function* parse_program(const char *filename) {
    lexer_init(&L, filename);
    nexttok();

    Stmt **list = NULL;
    int n = 0;

    /* Global statement list */
    while (!tok_is(T_EOF)) {
        Stmt *s = parse_stmt();
        list = realloc(list, sizeof(Stmt*) * (n + 1));
        list[n++] = s;
    }

    /* Encapsulate global statements into main entry point */
    Stmt *body = stmt_block(list, n, 0, 0);
    return make_main(body);
}