// src/main.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "../include/lexer.h"
#include "../include/parser.h"
#include "../include/semantic.h"
#include "../include/codegen.h"
#include "../include/common.h"

static void usage() {
    printf("Usage: mycc <input.my> -o <output>\n");
    exit(1);
}

int main(int argc, char **argv) {
    if (argc < 4) usage();

    const char *input = NULL;
    const char *outfile = NULL;
    bool debug_borrow = false;

    // simple CLI parsing
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug-borrow") == 0) {
            debug_borrow = true;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) usage();
            outfile = argv[++i];
        } else if (argv[i][0] != '-') {
            input = argv[i];
        }
    }

    if (!input || !outfile) usage();

    // generate ASM filename
    char asmfile[256];
    snprintf(asmfile, sizeof(asmfile), "%s.asm", outfile);

    // parse → sem → codegen
    Function *f = parse_program(input);

    semantic_check(f, input);

    if (codegen_function(f, asmfile, outfile, debug_borrow) != 0) {
        fprintf(stderr, "Codegen failed\n");
        return 1;
    }

#ifdef _WIN32
    printf("Generated asm: %s\n", asmfile);
    printf(">> nasm -f win64 %s -o %s.obj\n", asmfile, outfile);
    printf(">> gcc %s.obj runtime.o -o %s.exe\n", outfile, outfile);
    printf(">> .\\\\%s.exe\n", outfile);
#else
    printf("Generated asm: %s\n", asmfile);
    printf(">> nasm -f elf64 %s -o %s.o\n", asmfile, outfile);
    printf(">> gcc %s.o runtime.o -o %s\n", outfile, outfile);
    printf(">> ./\%s\n", outfile);
#endif

    return 0;
}