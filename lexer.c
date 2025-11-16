#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "ast.h"

static const char *src;
static const char *start;
static const char *current;

static void skip_whitespace(void) {
    for (;;) {
        char c = *current;
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            current++;
        } else {
            break;
        }
    }
}

static int is_alpha(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_';
}

static int is_digit(char c) {
    return (c >= '0' && c <= '9');
}

static Token make_token(TokenType type) {
    Token t;
    t.type = type;
    size_t len = current - start;
    char *s = (char*)malloc(len + 1);
    memcpy(s, start, len);
    s[len] = '\0';
    t.lexeme = s;
    t.number = 0.0;
    return t;
}

static Token error_token(const char *msg) {
    Token t;
    t.type = TOK_ERROR;
    size_t len = strlen(msg);
    char *s = (char*)malloc(len + 1);
    memcpy(s, msg, len + 1);
    t.lexeme = s;
    t.number = 0.0;
    return t;
}

void init_lexer(const char *source) {
    src = source;
    start = src;
    current = src;
}

static int match(char expected) {
    if (*current == expected) {
        current++;
        return 1;
    }
    return 0;
}

static Token string_token(void) {
    while (*current && *current != '"') {
        current++;
    }
    if (!*current) {
        return error_token("Unterminated string");
    }
    // consume closing "
    current++;
    return make_token(TOK_STRING);
}

static Token number_token(void) {
    while (is_digit(*current)) current++;
    if (*current == '.') {
        current++;
        while (is_digit(*current)) current++;
    }
    Token t = make_token(TOK_NUMBER);
    t.number = atof(t.lexeme);
    return t;
}

static TokenType identifier_type(const char *text) {
    if (strcmp(text, "symbol") == 0) return TOK_SYMBOL;
    if (strcmp(text, "if") == 0)     return TOK_IF;
    if (strcmp(text, "then") == 0)   return TOK_THEN;
    if (strcmp(text, "end") == 0)    return TOK_END;
    if (strcmp(text, "buy") == 0)    return TOK_BUY;
    if (strcmp(text, "sell") == 0)   return TOK_SELL;
    if (strcmp(text, "and") == 0)    return TOK_AND;
    if (strcmp(text, "or") == 0)     return TOK_OR;
    if (strcmp(text, "not") == 0)    return TOK_NOT;
    return TOK_IDENT;
}

static Token identifier_token(void) {
    while (is_alpha(*current) || is_digit(*current)) current++;
    Token t = make_token(TOK_IDENT);
    TokenType kw = identifier_type(t.lexeme);
    t.type = kw;
    return t;
}

Token next_token(void) {
    skip_whitespace();
    start = current;

    if (*current == '\0') {
        Token t = make_token(TOK_EOF);
        return t;
    }

    char c = *current++;
    switch (c) {
        case '+': return make_token(TOK_PLUS);
        case '-': return make_token(TOK_MINUS);
        case '*': return make_token(TOK_STAR);
        case '/': return make_token(TOK_SLASH);
        case '(': return make_token(TOK_LPAREN);
        case ')': return make_token(TOK_RPAREN);
        case ',': return make_token(TOK_COMMA);
        case '>':
            if (match('=')) return make_token(TOK_GE);
            return make_token(TOK_GT);
        case '<':
            if (match('=')) return make_token(TOK_LE);
            return make_token(TOK_LT);
        case '=':
            if (match('=')) return make_token(TOK_EQ);
            break;
        case '!':
            if (match('=')) return make_token(TOK_NE);
            break;
        case '"':
            return string_token();
        default:
            break;
    }

    if (is_digit(c)) {
        current--; // step back, number_token will handle
        return number_token();
    }

    if (is_alpha(c)) {
        current--;
        return identifier_token();
    }

    return error_token("Unexpected character");
}
