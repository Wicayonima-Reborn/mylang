// include/lexer.h
#ifndef LEXER_H
#define LEXER_H

#include "common.h"

#define MAX_TOK_LEN 256

typedef struct {
    const char *src;
    const char *filename;
    int pos;
    int line;
    int col;
} Lexer;

typedef enum {
    T_EOF = 0,

    T_INTLIT,
    T_STRLIT,
    T_IDENT,

    T_PRINT,       // keyword print
    T_LET,         // let
    T_INT_TYPE,    // int
    T_STRING_TYPE, // string

    T_LPAREN, T_RPAREN,
    T_LBRACE, T_RBRACE,
    T_COMMA, T_SEMI,
    T_COLON, T_EQ,

    T_AND,         // &
    T_ANDMUT       // &mut

} TokenKind;

typedef struct {
    TokenKind kind;
    char lexeme[MAX_TOK_LEN];
    long int_val;
    int line;
    int col;
} Token;

void lexer_init(Lexer *l, const char *filename);
Token lexer_next(Lexer *l);

#endif