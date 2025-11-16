#include <stdio.h>
#include <stdlib.h>
#include "ast.h"

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror("fopen"); exit(1); }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char*)malloc(sz + 1);
    if (!buf) { fprintf(stderr, "Out of memory\n"); exit(1); }
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s program.tl\n", argv[0]);
        return 1;
    }

    char *source = read_file(argv[1]);

    Program *prog = parse_program(source);
    Chunk chunk;
    compile_program(prog, &chunk);

    // Dummy candle context for testing
    VMContext ctx;
    ctx.open = 100.0;
    ctx.high = 110.0;
    ctx.low =  95.0;
    ctx.close = 108.0;
    ctx.volume = 1000000;
    ctx.date = 20251117;  // YYYYMMDD
    ctx.time = 940;       // 09:40
    ctx.hour = 9;
    ctx.minute = 40;
    ctx.weekday = 1;      // Monday

    run_chunk(&chunk, &ctx, prog->symbol);

    free_chunk(&chunk);
    free_program(prog);
    free(source);
    return 0;
}
