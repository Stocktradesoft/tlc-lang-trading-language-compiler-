#ifndef TL_AST_H
#define TL_AST_H

#include <stdint.h>

/* ---------- TOKEN TYPES ---------- */

typedef enum {
    TOK_EOF = 0,
    TOK_ERROR,

    TOK_SYMBOL,
    TOK_IF,
    TOK_THEN,
    TOK_END,
    TOK_BUY,
    TOK_SELL,
    TOK_AND,
    TOK_OR,
    TOK_NOT,

    TOK_IDENT,
    TOK_NUMBER,
    TOK_STRING,

    TOK_PLUS,    // +
    TOK_MINUS,   // -
    TOK_STAR,    // *
    TOK_SLASH,   // /
    TOK_GT,      // >
    TOK_LT,      // <
    TOK_GE,      // >=
    TOK_LE,      // <=
    TOK_EQ,      // ==
    TOK_NE,      // !=
    TOK_LPAREN,  // (
    TOK_RPAREN,  // )
    TOK_COMMA    // ,
} TokenType;

typedef struct {
    TokenType type;
    const char *lexeme;  // allocated string
    double number;       // valid if type == TOK_NUMBER
} Token;

/* ---------- AST TYPES ---------- */

typedef enum {
    EXPR_NUMBER,
    EXPR_IDENT,
    EXPR_CALL,
    EXPR_BINARY,
    EXPR_UNARY,
    EXPR_STRING
} ExprKind;

typedef enum {
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_GT_OP,
    OP_LT_OP,
    OP_GE_OP,
    OP_LE_OP,
    OP_EQ_OP,
    OP_NE_OP,
    OP_AND_OP,
    OP_OR_OP,
    OP_NEG_OP,
    OP_NOT_OP
} OpKind;

struct Expr;

typedef struct Expr {
    ExprKind kind;
    union {
        struct {
            double value;
        } number;
        struct {
            char *name;
        } ident;
        struct {
            char *value;
        } string;
        struct {
            char *func_name;
            struct Expr **args;
            int arg_count;
        } call;
        struct {
            OpKind op;
            struct Expr *left;
            struct Expr *right; // for unary, right == NULL
        } op;
    } as;
} Expr;

/* Statements: only BUY / SELL quantity */

typedef enum {
    STMT_BUY,
    STMT_SELL
} StmtKind;

typedef struct Stmt {
    StmtKind kind;
    int quantity;
    struct Stmt *next;
} Stmt;

/* Rule: if condition then action end */

typedef struct Rule {
    Expr *condition;
    Stmt *action;    // single action for now
    struct Rule *next;
} Rule;

/* Program */

typedef struct Program {
    char *symbol;   // "NIFTY"
    Rule *rules;    // linked list
} Program;

/* ---------- BYTECODE & VM ---------- */

typedef enum {
    BC_HALT = 0,
    BC_PUSH_CONST,    // [double]
    BC_LOAD_VAR,      // [uint8 id]
    BC_CALL_FUNC,     // [uint8 func_id][uint8 argc]
    BC_ADD,
    BC_SUB,
    BC_MUL,
    BC_DIV,
    BC_GT,
    BC_LT,
    BC_GE,
    BC_LE,
    BC_EQ,
    BC_NE,
    BC_AND,
    BC_OR,
    BC_NEG,
    BC_NOT,
    BC_JUMP_IF_FALSE, // [int32 offset]
    BC_JUMP,          // [int32 offset]
    BC_BUY,           // [int32 qty]
    BC_SELL           // [int32 qty]
} OpCode;

/* Builtin variable IDs (for LOAD_VAR) */
typedef enum {
    VAR_OPEN = 0,
    VAR_HIGH,
    VAR_LOW,
    VAR_CLOSE,
    VAR_VOLUME,
    VAR_DATE,    // as YYYYMMDD
    VAR_TIME,    // as HHMM
    VAR_HOUR,
    VAR_MINUTE,
    VAR_WEEKDAY  // 1=Mon .. 7=Sun
} VarId;

/* Builtin function IDs (for CALL_FUNC) */
typedef enum {
    FUNC_SMA = 0,
    FUNC_EMA,
    FUNC_RSI
} FuncId;

typedef struct {
    uint8_t *code;
    int count;
    int capacity;
} Chunk;

typedef struct {
    double open, high, low, close, volume;
    int date;   // YYYYMMDD
    int time;   // HHMM
    int hour;
    int minute;
    int weekday; // 1–7
} VMContext;

/* ---------- PUBLIC API ---------- */

/* lexer.c */
void init_lexer(const char *source);
Token next_token(void);

/* parser.c */
Program *parse_program(const char *source);
void free_program(Program *program);

/* vm.c */
void init_chunk(Chunk *chunk);
void free_chunk(Chunk *chunk);
void compile_program(Program *program, Chunk *chunk);
void run_chunk(Chunk *chunk, const VMContext *ctx, const char *symbol);

#endif /* TL_AST_H */
