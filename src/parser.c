// src/parser.c
#include "../include/parser.h"
#include "../include/lexer.h"
#include "../include/ast.h"
#include "../include/common.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static Token cur;
static Lexer L;

/* ------------------------------------------- */
/* Helpers */
/* ------------------------------------------- */

static void nexttok() {
    cur = lexer_next(&L);
}

static bool tok_is(TokenKind k) {
    return cur.kind == k;
}

static void expect(TokenKind k, const char *msg) {
    if (!tok_is(k)) {
        errorf("Parse error at %d:%d: expected %s (got '%s')\n",
               cur.line, cur.col, msg, cur.lexeme);
    }
    nexttok();
}

/* Forward decls */
static Expr* parse_expr();
static Stmt* parse_stmt();
static Stmt* parse_block();

/* ------------------------------------------- */
/* Primary expressions */
/* ------------------------------------------- */

static Expr* parse_primary() {
    /* int literal */
    if (tok_is(T_INTLIT)) {
        long v = cur.int_val;
        int l = cur.line, c = cur.col;
        nexttok();
        return expr_int(v, l, c);
    }

    /* string literal */
    if (tok_is(T_STRLIT)) {
        char *s = strdup(cur.lexeme);
        int l = cur.line, c = cur.col;
        nexttok();
        return expr_str(s, l, c);
    }

    /* identifier OR print */
    if (tok_is(T_IDENT) || tok_is(T_PRINT)) {
        char name[MAX_IDENT];
        strncpy(name, cur.lexeme, MAX_IDENT - 1);
        name[MAX_IDENT - 1] = 0;

        int l = cur.line, c = cur.col;
        nexttok();

        /* call expression? */
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
            return expr_call(name, args, nargs, l, c);
        }

        /* plain identifier */
        return expr_ident(name, l, c);
    }

    /* &expr or &mut expr */
    if (tok_is(T_AND) || tok_is(T_ANDMUT)) {
        bool mut = tok_is(T_ANDMUT);
        int l = cur.line, c = cur.col;
        nexttok();
        Expr *inner = parse_primary();
        return expr_addr(inner, mut, l, c);
    }

    errorf("Unexpected token '%s' at %d:%d\n", cur.lexeme, cur.line, cur.col);
    return NULL;
}

/* ------------------------------------------- */
/* Expressions (no binary ops yet) */
/* ------------------------------------------- */

static Expr* parse_expr() {
    return parse_primary();
}

/* ------------------------------------------- */
/* Statement parsing */
/* ------------------------------------------- */

static Stmt* parse_stmt() {
    int l = cur.line, c = cur.col;

    /* let declaration */
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

        /* optional type annotation */
        if (tok_is(T_COLON)) {
            nexttok();
            if (tok_is(T_INT_TYPE)) { ty = mktype(TY_INT); nexttok(); }
            else if (tok_is(T_STRING_TYPE)) { ty = mktype(TY_STRING); nexttok(); }
            else {
                errorf("Unknown type at %d:%d\n", cur.line, cur.col);
            }
        }

        /* optional initializer */
        if (tok_is(T_EQ)) {
            nexttok();
            init = parse_expr();
        }

        expect(T_SEMI, "';'");
        return stmt_decl(name, ty, init, l, c);
    }

    /* block */
    if (tok_is(T_LBRACE)) {
        return parse_block();
    }

    /* expression statement */
    Expr *e = parse_expr();
    expect(T_SEMI, "';'");
    return stmt_expr(e, l, c);
}

/* ------------------------------------------- */
/* Block parsing */
/* ------------------------------------------- */

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

/* ------------------------------------------- */
/* Entry: parse the whole file into main() */
/* ------------------------------------------- */

Function* parse_program(const char *filename) {
    lexer_init(&L, filename);
    nexttok();

    /* Parse GLOBAL STATEMENT LIST until EOF */
    Stmt **list = NULL;
    int n = 0;

    while (!tok_is(T_EOF)) {
        Stmt *s = parse_stmt();
        list = realloc(list, sizeof(Stmt*) * (n + 1));
        list[n++] = s;
    }

    /* Wrap them inside main() */
    Stmt *body = stmt_block(list, n, 0, 0);
    return make_main(body);
}