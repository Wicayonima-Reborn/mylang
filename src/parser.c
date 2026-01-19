/**
 * @file parser.c
 * @brief Implementasi Recursive Descent Parser untuk MyLang.
 * * Parser ini bertanggung jawab untuk mengubah aliran token (token stream) menjadi 
 * Abstract Syntax Tree (AST). Menggunakan teknik Predictive Parsing dengan 
 * satu token lookahead (cur).
 */

#include "../include/parser.h"
#include "../include/lexer.h"
#include "../include/ast.h"
#include "../include/common.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/** * @brief State internal parser.
 * L: Instance lexer untuk pemindaian karakter.
 * cur: Buffer untuk token saat ini (lookahead).
 */
static Token cur;
static Lexer L;

/* ---------------------------------------------------------
   PARSER UTILITIES
   --------------------------------------------------------- */

/**
 * @brief Mengambil token berikutnya dari lexer dan menyimpannya ke state 'cur'.
 */
static void nexttok() {
    cur = lexer_next(&L);
}

/**
 * @brief Memeriksa apakah token saat ini sesuai dengan jenis yang diharapkan.
 */
static bool tok_is(TokenKind k) {
    return cur.kind == k;
}

/**
 * @brief Memastikan token saat ini adalah jenis 'k', jika tidak maka memicu error fatal.
 * Jika sesuai, parser akan berlanjut ke token berikutnya (consume).
 */
static void expect(TokenKind k, const char *msg) {
    if (!tok_is(k)) {
        errorf("Parse error at %d:%d: expected %s (got '%s')\n",
                cur.line, cur.col, msg, cur.lexeme);
    }
    nexttok();
}

/* Forward declarations untuk menangani mutual recursion pada grammar */
static Expr *parse_expr();
static Expr *parse_primary();
static Stmt *parse_stmt();
static Stmt *parse_block();

/* ---------------------------------------------------------
   EXPRESSION PARSING (Precedence: Primary > Range)
   --------------------------------------------------------- */

/**
 * @brief Menangani unit ekspresi terkecil (Atoms).
 * Mencakup literal, identifikasi variabel, pemanggilan fungsi, indexing array, 
 * dan operasi pengambilan alamat (borrowing).
 * * Grammar:
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

        // Pencabangan untuk Function Call: ident(...)
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

        // Pencabangan untuk Postfix Array Indexing: ident[idx]
        while (tok_is(T_LBRACKET)) {
            nexttok();
            Expr *idx = parse_expr();
            expect(T_RBRACKET, "']'");
            base = expr_index(base, idx, l, c);
        }

        return base;
    }

    // Handle Array Literal: [1, 2, 3]
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

    // Handle Borrowing / Referencing: &x atau &mut x
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
 * @brief Entry point untuk parsing ekspresi.
 * Saat ini menangani operator Range (..) dengan presedensi terendah.
 * * Grammar: Expr -> Primary ( '..' Primary )?
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
 * @brief Dispatcher utama untuk berbagai jenis statement.
 * Menangani deklarasi (let), perulangan (for), blok kode, dan expression statements.
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

        Type ty = mktype(TY_UNKNOWN); // Default untuk Type Inference
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

        // Optional Assignment
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

    // 4. Expression Statement: call();
    Expr *e = parse_expr();
    expect(T_SEMI, "';'");
    return stmt_expr(e, l, c);
}

/**
 * @brief Memproses sekumpulan statement di dalam kurung kurawal.
 * Mengimplementasikan scoping level pada level sintaksis.
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
 * @brief Fungsi utama untuk memulai proses parsing program.
 * Inisialisasi lexer dan membangun root node dari AST (biasanya fungsi main).
 * * @param filename Path file sumber yang akan diparsing.
 * @return Function* Pointer ke root node AST program.
 */
Function *parse_program(const char *filename) {
    // Inisialisasi stream scanner
    lexer_init(&L, filename);
    
    // Seed lookahead pertama
    nexttok();

    Stmt **list = NULL;
    int n = 0;

    // Parsing hingga End of File
    while (!tok_is(T_EOF)) {
        Stmt *s = parse_stmt();
        list = realloc(list, sizeof(Stmt *) * (n + 1));
        list[n++] = s;
    }

    // Membungkus semua statement global ke dalam blok fungsi main implisit
    Stmt *body = stmt_block(list, n, 0, 0);
    return make_main(body);
}