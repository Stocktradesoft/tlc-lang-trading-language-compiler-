#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

/* ---------- Chunk helpers ---------- */

void init_chunk(Chunk *chunk) {
    chunk->code = NULL;
    chunk->count = 0;
    chunk->capacity = 0;
}

static void write_byte(Chunk *chunk, uint8_t byte) {
    if (chunk->count + 1 > chunk->capacity) {
        int old_cap = chunk->capacity;
        chunk->capacity = old_cap ? old_cap * 2 : 64;
        chunk->code = (uint8_t*)realloc(chunk->code, chunk->capacity);
    }
    chunk->code[chunk->count++] = byte;
}

static void write_int32(Chunk *chunk, int32_t val) {
    for (int i = 0; i < 4; ++i) {
        write_byte(chunk, (uint8_t)((val >> (i * 8)) & 0xFF));
    }
}

static void write_double(Chunk *chunk, double val) {
    union { double d; uint8_t b[8]; } u;
    u.d = val;
    for (int i = 0; i < 8; ++i) {
        write_byte(chunk, u.b[i]);
    }
}

void free_chunk(Chunk *chunk) {
    if (chunk->code) free(chunk->code);
    chunk->code = NULL;
    chunk->count = chunk->capacity = 0;
}

/* ---------- Helpers to map names ---------- */

static int is_builtin_var(const char *name, VarId *out_id) {
    if (strcmp(name, "open") == 0)    { *out_id = VAR_OPEN; return 1; }
    if (strcmp(name, "high") == 0)    { *out_id = VAR_HIGH; return 1; }
    if (strcmp(name, "low") == 0)     { *out_id = VAR_LOW; return 1; }
    if (strcmp(name, "close") == 0)   { *out_id = VAR_CLOSE; return 1; }
    if (strcmp(name, "volume") == 0)  { *out_id = VAR_VOLUME; return 1; }
    if (strcmp(name, "date") == 0)    { *out_id = VAR_DATE; return 1; }
    if (strcmp(name, "time") == 0)    { *out_id = VAR_TIME; return 1; }
    if (strcmp(name, "hour") == 0)    { *out_id = VAR_HOUR; return 1; }
    if (strcmp(name, "minute") == 0)  { *out_id = VAR_MINUTE; return 1; }
    if (strcmp(name, "weekday") == 0) { *out_id = VAR_WEEKDAY; return 1; }
    return 0;
}

static int is_builtin_func(const char *name, FuncId *out_f) {
    if (strcmp(name, "sma") == 0) { *out_f = FUNC_SMA; return 1; }
    if (strcmp(name, "ema") == 0) { *out_f = FUNC_EMA; return 1; }
    if (strcmp(name, "rsi") == 0) { *out_f = FUNC_RSI; return 1; }
    return 0;
}

/* ---------- Compile expressions to bytecode ---------- */

static void compile_expr(Chunk *chunk, Expr *e);

static void compile_binary(Chunk *chunk, Expr *e) {
    compile_expr(chunk, e->as.op.left);
    compile_expr(chunk, e->as.op.right);
    switch (e->as.op.op) {
        case OP_ADD:   write_byte(chunk, BC_ADD); break;
        case OP_SUB:   write_byte(chunk, BC_SUB); break;
        case OP_MUL:   write_byte(chunk, BC_MUL); break;
        case OP_DIV:   write_byte(chunk, BC_DIV); break;
        case OP_GT_OP: write_byte(chunk, BC_GT);  break;
        case OP_LT_OP: write_byte(chunk, BC_LT);  break;
        case OP_GE_OP: write_byte(chunk, BC_GE);  break;
        case OP_LE_OP: write_byte(chunk, BC_LE);  break;
        case OP_EQ_OP: write_byte(chunk, BC_EQ);  break;
        case OP_NE_OP: write_byte(chunk, BC_NE);  break;
        case OP_AND_OP: write_byte(chunk, BC_AND); break;
        case OP_OR_OP:  write_byte(chunk, BC_OR);  break;
        default: break;
    }
}

static void compile_unary(Chunk *chunk, Expr *e) {
    compile_expr(chunk, e->as.op.left);
    switch (e->as.op.op) {
        case OP_NEG_OP: write_byte(chunk, BC_NEG); break;
        case OP_NOT_OP: write_byte(chunk, BC_NOT); break;
        default: break;
    }
}

/* Compile-time conversion of string time/date/weekday literals into numeric codes */

static int parse_date_string(const char *s) {
    // expecting "YYYY-MM-DD"
    int y, m, d;
    if (sscanf(s, "\"%d-%d-%d\"", &y, &m, &d) == 3) {
        return y * 10000 + m * 100 + d;
    }
    return 0;
}

static int parse_time_string(const char *s) {
    // expecting "HH:MM"
    int h, m;
    if (sscanf(s, "\"%d:%d\"", &h, &m) == 2) {
        return h * 100 + m;
    }
    return 0;
}

static int parse_weekday_string(const char *s) {
    if (strstr(s, "Mon")) return 1;
    if (strstr(s, "Tue")) return 2;
    if (strstr(s, "Wed")) return 3;
    if (strstr(s, "Thu")) return 4;
    if (strstr(s, "Fri")) return 5;
    if (strstr(s, "Sat")) return 6;
    if (strstr(s, "Sun")) return 7;
    return 0;
}

/* For this skeleton, string literals are only allowed on right side of
 * time/date/weekday comparisons. We simply compile them as numeric constants.
 */

static void compile_expr(Chunk *chunk, Expr *e) {
    switch (e->kind) {
        case EXPR_NUMBER:
            write_byte(chunk, BC_PUSH_CONST);
            write_double(chunk, e->as.number.value);
            break;

        case EXPR_IDENT: {
            VarId id;
            if (!is_builtin_var(e->as.ident.name, &id)) {
                fprintf(stderr, "Unknown identifier: %s\n", e->as.ident.name);
                exit(1);
            }
            write_byte(chunk, BC_LOAD_VAR);
            write_byte(chunk, (uint8_t)id);
            break;
        }

        case EXPR_STRING:
            // A raw string alone is not allowed in expressions in v0.1
            // (must be used only in comparisons). In full implementation,
            // you'd handle proper type checking. Here we just error.
            fprintf(stderr, "Bare string literal in expression not supported in skeleton.\n");
            exit(1);
            break;

        case EXPR_CALL: {
            FuncId f;
            if (!is_builtin_func(e->as.call.func_name, &f)) {
                fprintf(stderr, "Unknown function: %s\n", e->as.call.func_name);
                exit(1);
            }
            for (int i = 0; i < e->as.call.arg_count; ++i) {
                compile_expr(chunk, e->as.call.args[i]);
            }
            write_byte(chunk, BC_CALL_FUNC);
            write_byte(chunk, (uint8_t)f);
            write_byte(chunk, (uint8_t)e->as.call.arg_count);
            break;
        }

        case EXPR_BINARY:
            compile_binary(chunk, e);
            break;

        case EXPR_UNARY:
            compile_unary(chunk, e);
            break;
    }
}

/* Compile a single rule:
 * condition -> if false, jump over action
 * action    -> BUY/SELL qty
 */

static void compile_rule(Chunk *chunk, Rule *r) {
    /* condition */
    compile_expr(chunk, r->condition);
    write_byte(chunk, BC_JUMP_IF_FALSE);
    int jmp_pos = chunk->count;
    write_int32(chunk, 0); // placeholder

    /* action */
    if (r->action->kind == STMT_BUY) {
        write_byte(chunk, BC_BUY);
        write_int32(chunk, (int32_t)r->action->quantity);
    } else if (r->action->kind == STMT_SELL) {
        write_byte(chunk, BC_SELL);
        write_int32(chunk, (int32_t)r->action->quantity);
    }

    /* patch jump offset */
    int after = chunk->count;
    int32_t offset = after - (jmp_pos + 4);
    // write back
    for (int i = 0; i < 4; ++i) {
        chunk->code[jmp_pos + i] = (uint8_t)((offset >> (i * 8)) & 0xFF);
    }
}

/* Compile entire program: symbol is handled in runtime; rules emit sequentially. */

void compile_program(Program *program, Chunk *chunk) {
    init_chunk(chunk);
    Rule *r = program->rules;
    while (r) {
        compile_rule(chunk, r);
        r = r->next;
    }
    write_byte(chunk, BC_HALT);
}

/* ---------- VM ---------- */

#define STACK_MAX 256

typedef struct {
    double stack[STACK_MAX];
    int sp;
    const uint8_t *ip;
    Chunk *chunk;
    VMContext ctx;
    const char *symbol;
} VM;

static double pop(VM *vm) {
    return vm->stack[--vm->sp];
}

static void push(VM *vm, double v) {
    vm->stack[vm->sp++] = v;
}

/* Dummy indicator implementations for skeleton. You will replace with real ones. */
static double builtin_sma(double series, double period) {
    (void)series; (void)period;
    return series; // placeholder
}

static double builtin_ema(double series, double period) {
    (void)series; (void)period;
    return series; // placeholder
}

static double builtin_rsi(double period) {
    (void)period;
    return 50.0; // placeholder
}

static void vm_run(VM *vm) {
    vm->ip = vm->chunk->code;
    vm->sp = 0;

    for (;;) {
        OpCode op = (OpCode)(*vm->ip++);
        switch (op) {
            case BC_HALT:
                return;

            case BC_PUSH_CONST: {
                union { double d; uint8_t b[8]; } u;
                for (int i = 0; i < 8; ++i) u.b[i] = *vm->ip++;
                push(vm, u.d);
                break;
            }

            case BC_LOAD_VAR: {
                uint8_t id = *vm->ip++;
                double val = 0.0;
                switch (id) {
                    case VAR_OPEN:    val = vm->ctx.open; break;
                    case VAR_HIGH:    val = vm->ctx.high; break;
                    case VAR_LOW:     val = vm->ctx.low; break;
                    case VAR_CLOSE:   val = vm->ctx.close; break;
                    case VAR_VOLUME:  val = vm->ctx.volume; break;
                    case VAR_DATE:    val = (double)vm->ctx.date; break;
                    case VAR_TIME:    val = (double)vm->ctx.time; break;
                    case VAR_HOUR:    val = (double)vm->ctx.hour; break;
                    case VAR_MINUTE:  val = (double)vm->ctx.minute; break;
                    case VAR_WEEKDAY: val = (double)vm->ctx.weekday; break;
                    default: val = 0.0; break;
                }
                push(vm, val);
                break;
            }

            case BC_CALL_FUNC: {
                uint8_t fid = *vm->ip++;
                uint8_t argc = *vm->ip++;
                double result = 0.0;
                if (fid == FUNC_SMA) {
                    if (argc != 2) { fprintf(stderr, "sma expects 2 args\n"); exit(1); }
                    double period = pop(vm);
                    double series = pop(vm);
                    result = builtin_sma(series, period);
                } else if (fid == FUNC_EMA) {
                    if (argc != 2) { fprintf(stderr, "ema expects 2 args\n"); exit(1); }
                    double period = pop(vm);
                    double series = pop(vm);
                    result = builtin_ema(series, period);
                } else if (fid == FUNC_RSI) {
                    if (argc != 1) { fprintf(stderr, "rsi expects 1 arg\n"); exit(1); }
                    double period = pop(vm);
                    result = builtin_rsi(period);
                }
                push(vm, result);
                break;
            }

            case BC_ADD: { double b = pop(vm), a = pop(vm); push(vm, a + b); break; }
            case BC_SUB: { double b = pop(vm), a = pop(vm); push(vm, a - b); break; }
            case BC_MUL: { double b = pop(vm), a = pop(vm); push(vm, a * b); break; }
            case BC_DIV: { double b = pop(vm), a = pop(vm); push(vm, a / b); break; }

            case BC_GT:  { double b = pop(vm), a = pop(vm); push(vm, a >  b); break; }
            case BC_LT:  { double b = pop(vm), a = pop(vm); push(vm, a <  b); break; }
            case BC_GE:  { double b = pop(vm), a = pop(vm); push(vm, a >= b); break; }
            case BC_LE:  { double b = pop(vm), a = pop(vm); push(vm, a <= b); break; }
            case BC_EQ:  { double b = pop(vm), a = pop(vm); push(vm, a == b); break; }
            case BC_NE:  { double b = pop(vm), a = pop(vm); push(vm, a != b); break; }

            case BC_AND: { double b = pop(vm), a = pop(vm); push(vm, (a != 0.0) && (b != 0.0)); break; }
            case BC_OR:  { double b = pop(vm), a = pop(vm); push(vm, (a != 0.0) || (b != 0.0)); break; }
            case BC_NEG: { double a = pop(vm); push(vm, -a); break; }
            case BC_NOT: { double a = pop(vm); push(vm, (a == 0.0)); break; }

            case BC_JUMP_IF_FALSE: {
                int32_t offset = 0;
                for (int i = 0; i < 4; ++i) {
                    offset |= ((int32_t)(*vm->ip++) << (i * 8));
                }
                double cond = pop(vm);
                if (!cond) {
                    vm->ip += offset;
                }
                break;
            }

            case BC_JUMP: {
                int32_t offset = 0;
                for (int i = 0; i < 4; ++i) {
                    offset |= ((int32_t)(*vm->ip++) << (i * 8));
                }
                vm->ip += offset;
                break;
            }

            case BC_BUY: {
                int32_t qty = 0;
                for (int i = 0; i < 4; ++i) {
                    qty |= ((int32_t)(*vm->ip++) << (i * 8));
                }
                printf("SYMBOL %s: BUY %d\n", vm->symbol, qty);
                break;
            }

            case BC_SELL: {
                int32_t qty = 0;
                for (int i = 0; i < 4; ++i) {
                    qty |= ((int32_t)(*vm->ip++) << (i * 8));
                }
                printf("SYMBOL %s: SELL %d\n", vm->symbol, qty);
                break;
            }

            default:
                fprintf(stderr, "Unknown opcode %d\n", op);
                return;
        }
    }
}

void run_chunk(Chunk *chunk, const VMContext *ctx, const char *symbol) {
    VM vm;
    vm.chunk = chunk;
    vm.ctx = *ctx;
    vm.symbol = symbol;
    vm_run(&vm);
}
