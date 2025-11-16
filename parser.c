#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

/* ---------- Utilities to allocate AST ---------- */

static void *xmalloc(size_t sz) {
    void *p = malloc(sz);
    if (!p) { fprintf(stderr, "Out of memory\n"); exit(1); }
    return p;
}

static Expr *new_number(double v) {
    Expr *e = (Expr*)xmalloc(sizeof(Expr));
    e->kind = EXPR_NUMBER;
    e->as.number.value = v;
    return e;
}

static Expr *new_ident(char *name) {
    Expr *e = (Expr*)xmalloc(sizeof(Expr));
    e->kind = EXPR_IDENT;
    e->as.ident.name = name;
    return e;
}

static Expr *new_string(char *val) {
    Expr *e = (Expr*)xmalloc(sizeof(Expr));
    e->kind = EXPR_STRING;
    e->as.string.value = val;
    return e;
}

static Expr *new_unary(OpKind op, Expr *sub) {
    Expr *e = (Expr*)xmalloc(sizeof(Expr));
    e->kind = EXPR_UNARY;
    e->as.op.op = op;
    e->as.op.left = sub;
    e->as.op.right = NULL;
    return e;
}

static Expr *new_binary(OpKind op, Expr *left, Expr *right) {
    Expr *e = (Expr*)xmalloc(sizeof(Expr));
    e->kind = EXPR_BINARY;
    e->as.op.op = op;
    e->as.op.left = left;
    e->as.op.right = right;
    return e;
}

static Expr *new_call(char *name, Expr **args, int arg_count) {
    Expr *e = (Expr*)xmalloc(sizeof(Expr));
    e->kind = EXPR_CALL;
    e->as.call.func_name = name;
    e->as.call.args = args;
    e->as.call.arg_count = arg_count;
    return e;
}

static Stmt *new_stmt(StmtKind kind, int qty) {
    Stmt *s = (Stmt*)xmalloc(sizeof(Stmt));
    s->kind = kind;
    s->quantity = qty;
    s->next = NULL;
    return s;
}

static Rule *new_rule(Expr *cond, Stmt *act) {
    Rule *r = (Rule*)xmalloc(sizeof(Rule));
    r->condition = cond;
    r->action = act;
    r->next = NULL;
    return r;
}

static Program *new_program(char *symbol, Rule *rules) {
    Program *p = (Program*)xmalloc(sizeof(Program));
    p->symbol = symbol;
    p->rules = rules;
    return p;
}

/* ---------- Parser state ---------- */

static Token current_token;

static void advance(void) {
    current_token = next_token();
}

static void error(const char *msg) {
    fprintf(stderr, "Parse error: %s (token: %s)\n", msg, current_token.lexeme);
    exit(1);
}

static void consume(TokenType type, const char *msg) {
    if (current_token.type != type) {
        error(msg);
    }
    advance();
}

/* Forward declarations for expression parsing */
static Expr *parse_expr(void);
static Expr *parse_or(void);
static Expr *parse_and(void);
static Expr *parse_not(void);
static Expr *parse_cmp(void);
static Expr *parse_add(void);
static Expr *parse_mul(void);
static Expr *parse_primary(void);

/* ---------- Parsing functions ---------- */

static Expr *parse_primary(void) {
    if (current_token.type == TOK_NUMBER) {
        double v = current_token.number;
        advance();
        return new_number(v);
    }
    if (current_token.type == TOK_IDENT) {
        char *name = strdup(current_token.lexeme);
        advance();
        // function call or simple identifier?
        if (current_token.type == TOK_LPAREN) {
            // function call
            advance(); // consume '('
            Expr **args = NULL;
            int arg_count = 0, cap = 0;
            if (current_token.type != TOK_RPAREN) {
                for (;;) {
                    if (arg_count == cap) {
                        cap = cap ? cap * 2 : 4;
                        args = (Expr**)realloc(args, cap * sizeof(Expr*));
                    }
                    args[arg_count++] = parse_expr();
                    if (current_token.type == TOK_COMMA) {
                        advance();
                        continue;
                    }
                    break;
                }
            }
            consume(TOK_RPAREN, "Expected ')' after function arguments");
            return new_call(name, args, arg_count);
        }
        // variable / builtin ident
        return new_ident(name);
    }
    if (current_token.type == TOK_STRING) {
        char *s = strdup(current_token.lexeme);
        advance();
        return new_string(s);
    }
    if (current_token.type == TOK_LPAREN) {
        advance();
        Expr *e = parse_expr();
        consume(TOK_RPAREN, "Expected ')'");
        return e;
    }
    error("Expected expression");
    return NULL;
}

static Expr *parse_mul(void) {
    Expr *left = parse_primary();
    for (;;) {
        if (current_token.type == TOK_STAR) {
            advance();
            left = new_binary(OP_MUL, left, parse_primary());
        } else if (current_token.type == TOK_SLASH) {
            advance();
            left = new_binary(OP_DIV, left, parse_primary());
        } else {
            break;
        }
    }
    return left;
}

static Expr *parse_add(void) {
    Expr *left = parse_mul();
    for (;;) {
        if (current_token.type == TOK_PLUS) {
            advance();
            left = new_binary(OP_ADD, left, parse_mul());
        } else if (current_token.type == TOK_MINUS) {
            advance();
            left = new_binary(OP_SUB, left, parse_mul());
        } else {
            break;
        }
    }
    return left;
}

static Expr *parse_cmp(void) {
    Expr *left = parse_add();
    if (current_token.type == TOK_GT || current_token.type == TOK_LT ||
        current_token.type == TOK_GE || current_token.type == TOK_LE ||
        current_token.type == TOK_EQ || current_token.type == TOK_NE) {
        TokenType op_tok = current_token.type;
        advance();
        Expr *right = parse_add();
        OpKind op;
        switch (op_tok) {
            case TOK_GT: op = OP_GT_OP; break;
            case TOK_LT: op = OP_LT_OP; break;
            case TOK_GE: op = OP_GE_OP; break;
            case TOK_LE: op = OP_LE_OP; break;
            case TOK_EQ: op = OP_EQ_OP; break;
            case TOK_NE: op = OP_NE_OP; break;
            default: op = OP_EQ_OP; break;
        }
        return new_binary(op, left, right);
    }
    return left;
}

static Expr *parse_not(void) {
    if (current_token.type == TOK_NOT) {
        advance();
        return new_unary(OP_NOT_OP, parse_not());
    }
    return parse_cmp();
}

static Expr *parse_and(void) {
    Expr *left = parse_not();
    while (current_token.type == TOK_AND) {
        advance();
        left = new_binary(OP_AND_OP, left, parse_not());
    }
    return left;
}

static Expr *parse_or(void) {
    Expr *left = parse_and();
    while (current_token.type == TOK_OR) {
        advance();
        left = new_binary(OP_OR_OP, left, parse_and());
    }
    return left;
}

static Expr *parse_expr(void) {
    return parse_or();
}

/* rule        ::= "if" expr "then" action "end" */

static Stmt *parse_action(void) {
    if (current_token.type == TOK_BUY) {
        advance();
        if (current_token.type != TOK_NUMBER) {
            error("Expected number after 'buy'");
        }
        int qty = (int)current_token.number;
        advance();
        return new_stmt(STMT_BUY, qty);
    } else if (current_token.type == TOK_SELL) {
        advance();
        if (current_token.type != TOK_NUMBER) {
            error("Expected number after 'sell'");
        }
        int qty = (int)current_token.number;
        advance();
        return new_stmt(STMT_SELL, qty);
    }
    error("Expected 'buy' or 'sell'");
    return NULL;
}

static Rule *parse_rule_list(void) {
    Rule *head = NULL;
    Rule *tail = NULL;

    while (current_token.type == TOK_IF) {
        advance(); // consume 'if'
        Expr *cond = parse_expr();
        consume(TOK_THEN, "Expected 'then'");
        Stmt *act = parse_action();
        consume(TOK_END, "Expected 'end'");

        Rule *rule = new_rule(cond, act);
        if (!head) head = tail = rule;
        else { tail->next = rule; tail = rule; }
    }
    return head;
}

/* program     ::= symbol_decl rule_list
 * symbol_decl ::= "symbol" string_lit
 */

Program *parse_program(const char *source) {
    init_lexer(source);
    advance(); // load first token

    consume(TOK_SYMBOL, "Expected 'symbol' at beginning");
    if (current_token.type != TOK_STRING) {
        error("Expected string literal after 'symbol'");
    }
    char *sym = strdup(current_token.lexeme);
    advance();

    Rule *rules = parse_rule_list();

    if (current_token.type != TOK_EOF) {
        error("Expected end of input");
    }

    return new_program(sym, rules);
}

/* Very simple free routine (leaks some inner strings, OK for skeleton) */

static void free_expr(Expr *e) {
    if (!e) return;
    switch (e->kind) {
        case EXPR_CALL:
            for (int i = 0; i < e->as.call.arg_count; ++i)
                free_expr(e->as.call.args[i]);
            free(e->as.call.args);
            free(e->as.call.func_name);
            break;
        case EXPR_IDENT:
            free(e->as.ident.name);
            break;
        case EXPR_STRING:
            free(e->as.string.value);
            break;
        case EXPR_BINARY:
        case EXPR_UNARY:
            free_expr(e->as.op.left);
            if (e->as.op.right) free_expr(e->as.op.right);
            break;
        case EXPR_NUMBER:
        default:
            break;
    }
    free(e);
}

static void free_stmt(Stmt *s) {
    while (s) {
        Stmt *next = s->next;
        free(s);
        s = next;
    }
}

static void free_rule(Rule *r) {
    while (r) {
        Rule *next = r->next;
        free_expr(r->condition);
        free_stmt(r->action);
        free(r);
        r = next;
    }
}

void free_program(Program *program) {
    if (!program) return;
    free(program->symbol);
    free_rule(program->rules);
    free(program);
}
