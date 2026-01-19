/**
 * @file parser.c
 * @brief Implementation of a Recursive Descent Parser for MyLang.
 *
 * This parser transforms a flat stream of tokens into an Abstract Syntax Tree (AST).
 * It utilizes a predictive parsing technique with a single token lookahead (LL(1)).
 */

#include "../include/parser.h"
#include "../include/lexer.h"
#include "../include/ast.h"
#include "../include/common.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/** * @brief Global parser state.
 * L: The lexer instance used for scanning source characters.
 * cur: The current lookahead token buffer.
 */
static Token cur;
static Lexer L;

/* ---------------------------------------------------------
   PARSER UTILITIES
   --------------------------------------------------------- */

/**
 * @brief Fetches the next token from the lexer and updates the 'cur' state.
 */
static void nexttok() {
    cur = lexer_next(&L);
}

/**
 * @brief Checks if the current token matches a specific kind without consuming it.
 */
static bool tok_is(TokenKind k) {
    return cur.kind == k;
}

/**
 * @brief Validates that the current token is of kind 'k'.
 * If it matches, the token is consumed via nexttok(). If not, a fatal 
 * parse error is reported.
 */
static void expect(TokenKind k, const char *msg) {
    if (!tok_is(k)) {
        errorf("Parse error at %d:%d: expected %s (got '%s')\n",
                cur.line, cur.col, msg, cur.lexeme);
    }
    nexttok();
}

/* Forward declarations to handle mutual recursion in the grammar */
static Expr *parse_expr();
static Expr *parse_primary();
static Stmt *parse_stmt();
static Stmt *parse_block();

/* ---------------------------------------------------------
   EXPRESSION PARSING (Precedence: Primary > Range)
   --------------------------------------------------------- */

/**
 * @brief Parses the smallest units of the language (Atoms).
 * This includes literals, variable identifiers, function calls, array indexing,
 * and memory referencing (borrowing).
 *
 * Grammar:
 * Primary -> INTLIT | STRLIT | IDENT ('(' args? ')')? ('[' expr ']')* | '[' items? ']' | '&' Primary
 */
static Expr *parse_primary() {
    int l = cur.line, c = cur.col;

    // Handle Integer Literals
    if (tok_is(T_INTLIT)) {
        long v = cur.int_val;
        nexttok();
        return expr_int(v, l, c);
    }

    // Handle String Literals
    if (tok_is(T_STRLIT)) {
        char *s = strdup(cur.lexeme);
        nexttok();
        return expr_str(s, l, c);
    }

    // Handle Identifiers, Function Calls, and Array Indexing
    if (tok_is(T_IDENT) || tok_is(T_PRINT)) {
        char name[MAX_IDENT];
        snprintf(name, sizeof(name), "%s", cur.lexeme);
        nexttok();

        Expr *base = expr_ident(name, l, c);

        // Branching for Function Calls: ident(...)
        if (tok_is(T_LPAREN)) {
            nexttok();
            Expr **args = NULL;
            int nargs = 0;

            if (!tok_is(T_RPAREN)) {
                while (1) {
                    Expr *a = parse_expr();
                    args = realloc(args, sizeof(Expr *) * (nargs + 1));
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

        // Branching for Postfix Array Indexing: ident[idx]
        while (tok_is(T_LBRACKET)) {
            nexttok();
            Expr *idx = parse_expr();
            expect(T_RBRACKET, "']'");
            base = expr_index(base, idx, l, c);
        }

        return base;
    }

    // Handle Array Literals: [1, 2, 3]
    if (tok_is(T_LBRACKET)) {
        nexttok();
        Expr **items = NULL;
        int count = 0;

        if (!tok_is(T_RBRACKET)) {
            while (1) {
                Expr *e = parse_expr();
                items = realloc(items, sizeof(Expr *) * (count + 1));
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

    // Handle Borrowing / Referencing: &x or &mut x
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
 * @brief Entry point for expression parsing.
 * Currently handles the Range operator (..) with the lowest precedence.
 *
 * Grammar: Expr -> Primary ( '..' Primary )?
 */
static Expr *parse_expr() {
    Expr *lhs = parse_primary();

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
   --------------------------------------------------------- */

/**
 * @brief Main dispatcher for various statement types.
 * Handles variable declarations (let), loops (for), code blocks, 
 * and expression-based statements.
 */
static Stmt *parse_stmt() {
    int l = cur.line, c = cur.col;

    // 1. Variable Declaration: let name: type = init;
    if (tok_is(T_LET)) {
        nexttok();

        if (!tok_is(T_IDENT)) {
            errorf("Expected identifier after 'let' at %d:%d\n", l, c);
        }

        char name[MAX_IDENT];
        snprintf(name, sizeof(name), "%s", cur.lexeme);
        nexttok();

        Type ty = mktype(TY_UNKNOWN); // Default for Type Inference
        Expr *init = NULL;

        // Optional Type Annotation: let x: int
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

        // Optional Assignment: let x = expression;
        if (tok_is(T_EQ)) {
            nexttok();
            init = parse_expr();
        }

        expect(T_SEMI, "';'");
        return stmt_decl(name, ty, init, l, c);
    }

    // 2. For Loop: for var in iter { ... }
    if (tok_is(T_FOR)) {
        nexttok();

        if (!tok_is(T_IDENT)) {
            errorf("Expected identifier after 'for' at %d:%d\n", l, c);
        }

        char var[MAX_IDENT];
        snprintf(var, sizeof(var), "%s", cur.lexeme);
        nexttok();

        expect(T_IN, "'in'");
        Expr *iter = parse_expr();
        Stmt *body = parse_block();

        return stmt_for(var, iter, body, l, c);
    }

    // 3. Block Statement: { ... }
    if (tok_is(T_LBRACE)) {
        return parse_block();
    }

    // 4. Expression Statement: call_func();
    Expr *e = parse_expr();
    expect(T_SEMI, "';'");
    return stmt_expr(e, l, c);
}

/**
 * @brief Processes a sequence of statements within curly braces.
 * This establishes a new lexical scope at the syntax level.
 */
static Stmt *parse_block() {
    int l = cur.line, c = cur.col;
    expect(T_LBRACE, "'{'");

    Stmt **list = NULL;
    int n = 0;

    while (!tok_is(T_RBRACE)) {
        if (tok_is(T_EOF)) {
            errorf("Unexpected EOF inside block\n");
        }
        Stmt *s = parse_stmt();
        list = realloc(list, sizeof(Stmt *) * (n + 1));
        list[n++] = s;
    }

    expect(T_RBRACE, "'}'");
    return stmt_block(list, n, l, c);
}

/* ---------------------------------------------------------
   PUBLIC INTERFACE
   --------------------------------------------------------- */

/**
 * @brief High-level entry point to begin program parsing.
 * Initializes the lexer and builds the root node of the AST.
 *
 * @param filename Path to the source file to be parsed.
 * @return Function* Pointer to the AST root node.
 */
Function *parse_program(const char *filename) {
    // Initialize the token stream scanner
    lexer_init(&L, filename);
    
    // Seed the first lookahead token
    nexttok();

    Stmt **list = NULL;
    int n = 0;

    // Parse until the End of File is reached
    while (!tok_is(T_EOF)) {
        Stmt *s = parse_stmt();
        list = realloc(list, sizeof(Stmt *) * (n + 1));
        list[n++] = s;
    }

    // Wrap all global statements into an implicit main function block
    Stmt *body = stmt_block(list, n, 0, 0);
    return make_main(body);
}