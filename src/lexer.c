#include "../include/lexer.h"
#include "../include/common.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int is_ident_start(char c) {
    return isalpha(c) || c == '_';
}

static int is_ident_char(char c) {
    return isalnum(c) || c == '_';
}

void lexer_init(Lexer *l, const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        exit(1);
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(sz + 1);
    fread(buf, 1, sz, f);
    buf[sz] = 0;
    fclose(f);

    l->src = buf;
    l->filename = filename;
    l->pos = 0;
    l->line = 1;
    l->col = 1;
}

static char peek(Lexer *l) {
    return l->src[l->pos];
}

static char getc_lex(Lexer *l) {
    char c = l->src[l->pos];
    if (c == 0) return 0;

    l->pos++;
    if (c == '\n') {
        l->line++;
        l->col = 1;
    } else {
        l->col++;
    }
    return c;
}

Token lexer_next(Lexer *l) {
    Token t;
    memset(&t, 0, sizeof(t));
    t.line = l->line;
    t.col = l->col;

    char c = peek(l);

    /* skip whitespace */
    while (c && isspace(c)) {
        getc_lex(l);
        c = peek(l);
    }

    /* EOF */
    if (!c) {
        t.kind = T_EOF;
        return t;
    }

    /* string literal */
    if (c == '"') {
        getc_lex(l); // consume opening quote

        int i = 0;
        while (peek(l) && peek(l) != '"') {
            t.lexeme[i++] = getc_lex(l);
        }
        t.lexeme[i] = 0;

        if (peek(l) != '"') {
            errorf("Unterminated string literal at %d:%d\n", t.line, t.col);
            exit(1);
        }

        getc_lex(l); // consume closing quote

        t.kind = T_STRLIT;
        return t;
    }

    /* integer literal */
    if (isdigit(c)) {
        long val = 0;

        while (isdigit(peek(l))) {
            val = val * 10 + (getc_lex(l) - '0');
        }

        t.kind = T_INTLIT;
        t.int_val = val;
        sprintf(t.lexeme, "%ld", val);
        return t;
    }

    /* identifier / keywords */
    if (is_ident_start(c)) {
        int i = 0;
        while (is_ident_char(peek(l))) {
            t.lexeme[i++] = getc_lex(l);
        }
        t.lexeme[i] = 0;

        if (strcmp(t.lexeme, "let") == 0) {
            t.kind = T_LET;
        } else if (strcmp(t.lexeme, "int") == 0) {
            t.kind = T_INT_TYPE;
        } else if (strcmp(t.lexeme, "string") == 0) {
            t.kind = T_STRING_TYPE;
        } else if (strcmp(t.lexeme, "print") == 0) {
            t.kind = T_PRINT;
        } else {
            t.kind = T_IDENT;
        }

        return t;
    }

    /* symbols */
    c = getc_lex(l);
    t.lexeme[0] = c;
    t.lexeme[1] = 0;

    switch (c) {
        case '{': t.kind = T_LBRACE; return t;
        case '}': t.kind = T_RBRACE; return t;
        case '(': t.kind = T_LPAREN; return t;
        case ')': t.kind = T_RPAREN; return t;
        case ';': t.kind = T_SEMI; return t;
        case ':': t.kind = T_COLON; return t;
        case '=': t.kind = T_EQ; return t;
        case ',': t.kind = T_COMMA; return t;

        case '&': {
            /* detect &mut (3 chars: m u t) */
            if (peek(l) == 'm') {
                // &mut
                getc_lex(l); // m
                if (peek(l) == 'u') getc_lex(l);
                if (peek(l) == 't') getc_lex(l);
                t.kind = T_ANDMUT;
            } else {
                t.kind = T_AND;
            }
            return t;
        }
    }

    errorf("Unknown character '%c' at %d:%d\n", c, l->line, l->col);
    exit(1);
}