#include "../include/lexer.h"
#include "../include/common.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/** ---------------------------------------------------------
 * SCANNING PREDICATES
 * Character classification for identifier resolution.
 * --------------------------------------------------------- */

static int is_ident_start(char c) {
    return isalpha((unsigned char)c) || c == '_';
}

static int is_ident_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

/** ---------------------------------------------------------
 * LEXER LIFECYCLE
 * Buffer management and stream initialization.
 * --------------------------------------------------------- */

/**
 * @brief Initializes the lexer by reading the source file into a buffer.
 * Performs a full file read to memory for fast non-sequential access.
 */
void lexer_init(Lexer *l, const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        perror(filename);
        exit(1);
    }

    /* Determine file size for buffer allocation */
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc(sz + 1);
    if (!buf) {
        perror("malloc");
        exit(1);
    }

    /* Populate buffer and ensure null-termination */
    size_t rd = fread(buf, 1, sz, f);
    if (rd != (size_t)sz) {
        perror("fread");
        exit(1);
    }

    buf[sz] = '\0';
    fclose(f);

    l->src = buf;
    l->filename = filename;
    l->pos = 0;
    l->line = 1;
    l->col = 1;
}

/** ---------------------------------------------------------
 * STREAM NAVIGATION
 * Lookahead and consumption primitives.
 * --------------------------------------------------------- */

/** Non-consuming lookahead of the current character. Returns NUL at EOF. */
static char peek(Lexer *l) {
    return l->src[l->pos];
}

/** Consumes the current character and updates stream coordinates. */
static char getc_lex(Lexer *l) {
    char c = l->src[l->pos];
    if (c == '\0') return '\0';

    l->pos++;
    if (c == '\n') {
        l->line++;
        l->col = 1;
    } else {
        l->col++;
    }
    return c;
}

/** ---------------------------------------------------------
 * TOKEN GENERATION
 * --------------------------------------------------------- */

/** Factory function for creating Token structures with coordinate metadata. */
static Token make_token(TokenKind k, const char *lex, long val, int line, int col) {
    Token t;
    memset(&t, 0, sizeof(Token));

    t.kind = k;
    t.int_val = val;
    t.line = line;
    t.col = col;

    if (lex) {
        /* Safe lexeme copying with guaranteed termination */
        snprintf(t.lexeme, sizeof(t.lexeme), "%s", lex);
    } else {
        t.lexeme[0] = '\0';
    }

    return t;
}

/** ---------------------------------------------------------
 * CORE SCANNER LOGIC
 * Implements the state machine for lexical analysis.
 * --------------------------------------------------------- */

/**
 * @brief Scans the next token from the source stream.
 * Handles whitespace, comments, literals, identifiers, and symbols.
 */
Token lexer_next(Lexer *l) {
    Token t = make_token(T_EOF, NULL, 0, l->line, l->col);

    /* Skip trivia (whitespace), reporting EOL where necessary */
    while (1) {
        char c = peek(l);
        if (!c) {
            t.kind = T_EOF;
            return t;
        }
        if (c == '\n') {
            getc_lex(l);
            continue;
        }
        if (isspace((unsigned char)c)) {
            getc_lex(l);
            continue;
        }
        break;
    }

    char c = peek(l);
    if (!c) {
        t.kind = T_EOF;
        return t;
    }

    /* String Literal Scanning (handles escape sequences) */
    if (c == '"') {
        getc_lex(l); /* Consume opening delimiter */
        int i = 0;

        while (peek(l) && peek(l) != '"') {
            char pc = getc_lex(l);
            if (pc == '\\' && peek(l)) {
                /* Escape sequence translation */
                char esc = getc_lex(l);
                if (esc == 'n') t.lexeme[i++] = '\n';
                else if (esc == 't') t.lexeme[i++] = '\t';
                else t.lexeme[i++] = esc;
            } else {
                t.lexeme[i++] = pc;
            }

            if (i >= MAX_TOK_LEN - 1) break;
        }

        t.lexeme[i] = '\0';

        if (peek(l) != '"') {
            errorf("Unterminated string literal at %d:%d\n", t.line, t.col);
            exit(1);
        }

        getc_lex(l); /* Consume closing delimiter */
        t.kind = T_STRLIT;
        return t;
    }

    /* Integer Literal Scanning */
    if (isdigit((unsigned char)c)) {
        long val = 0;
        while (isdigit((unsigned char)peek(l))) {
            val = val * 10 + (getc_lex(l) - '0');
        }

        char buf[64];
        snprintf(buf, sizeof(buf), "%ld", val);
        return make_token(T_INTLIT, buf, val, l->line, l->col);
    }

    /* Identifier and Keyword Scanning */
    if (is_ident_start(c)) {
        int i = 0;
        while (is_ident_char(peek(l))) {
            if (i < MAX_TOK_LEN - 1)
                t.lexeme[i++] = getc_lex(l);
            else
                getc_lex(l);
        }
        t.lexeme[i] = '\0';

        /* Keyword Triage */
        if (strcmp(t.lexeme, "let") == 0) t.kind = T_LET;
        else if (strcmp(t.lexeme, "int") == 0) t.kind = T_INT_TYPE;
        else if (strcmp(t.lexeme, "string") == 0) t.kind = T_STRING_TYPE;
        else if (strcmp(t.lexeme, "print") == 0) t.kind = T_PRINT;
        else if (strcmp(t.lexeme, "println") == 0) t.kind = T_PRINTLN;
        else t.kind = T_IDENT;

        return t;
    }

    /* Symbol and Operator Scanning */
    c = getc_lex(l);
    switch (c) {
        case '{': return make_token(T_LBRACE, "{", 0, l->line, l->col - 1);
        case '}': return make_token(T_RBRACE, "}", 0, l->line, l->col - 1);
        case '(': return make_token(T_LPAREN, "(", 0, l->line, l->col - 1);
        case ')': return make_token(T_RPAREN, ")", 0, l->line, l->col - 1);
        case ';': return make_token(T_SEMI, ";", 0, l->line, l->col - 1);
        case ':': return make_token(T_COLON, ":", 0, l->line, l->col - 1);
        case '=': return make_token(T_EQ, "=", 0, l->line, l->col - 1);
        case ',': return make_token(T_COMMA, ",", 0, l->line, l->col - 1);

        case '&': {
            /* Multi-character operator lookahead for '&mut' */
            if (peek(l) == 'm') {
                int saved = l->pos;
                int ok = 1;

                if (peek(l) == 'm') getc_lex(l); else ok = 0;
                if (peek(l) == 'u') getc_lex(l); else ok = 0;
                if (peek(l) == 't') getc_lex(l); else ok = 0;

                if (ok)
                    return make_token(T_ANDMUT, "&mut", 0, l->line, l->col - 1);

                /* Backtrack on failed multi-char match */
                l->pos = saved;
            }
            return make_token(T_AND, "&", 0, l->line, l->col - 1);
        }

        default:
            errorf("Unknown character '%c' at %d:%d\n", c, l->line, l->col - 1);
            exit(1);
    }
}