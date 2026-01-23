/**
 * @file main.c
 * @brief Entry point for the mycc compiler frontend.
 * * This module orchestrates the compilation pipeline: Lexing/Parsing, 
 * Semantic Analysis (including borrow checking), and x86_64 Code Generation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Internal compiler phase headers */
#include "../include/lexer.h"
#include "../include/parser.h"
#include "../include/semantic.h"
#include "../include/codegen.h"
#include "../include/common.h"

/**
 * @brief Prints CLI usage instructions and terminates the process.
 */
static void usage() {
    fprintf(stderr, "Usage: mycc <input.my> -o <output> [--debug-borrow]\n");
    exit(1);
}

/**
 * @brief Main execution loop for the compiler.
 * * Implements a linear compilation pass:
 * 1. CLI Argument Parsing
 * 2. Abstract Syntax Tree (AST) Generation (via parse_program)
 * 3. Static Analysis & Type Checking (via semantic_check)
 * 4. Assembly Generation (via codegen_function)
 */
int main(int argc, char **argv) {
    /* Minimum required: <bin> <input> -o <output> */
    if (argc < 4) usage();

    const char *input = NULL;
    const char *outfile = NULL;
    bool debug_borrow = false;

    /* --- Command Line Interface (CLI) Parsing --- */
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

    /* Validate mandatory arguments */
    if (!input || !outfile) usage();

    /* Prepare intermediate assembly filename buffer */
    char asmfile[256];
    snprintf(asmfile, sizeof(asmfile), "%s.asm", outfile);

    /* --- Compilation Pipeline --- */

    // Phase 1: Parsing (Lexing is handled internally by the parser)
    // Returns the root of the AST (Function node)
    Function *f = parse_program(input);

    // Phase 2: Semantic Analysis
    // Performs type checking and validates ownership/borrow rules
    semantic_check(f, input);

    // Phase 3: Code Generation
    // Emits x86_64 assembly to the .asm file and handles final binary output
    if (codegen_function(f, asmfile, outfile, debug_borrow) != 0) {
        fprintf(stderr, "Error: Codegen failed for input '%s'\n", input);
        return 1;
    }

    /* --- Post-Compilation Build Instructions --- */
    printf("Successfully generated assembly: %s\n", asmfile);
    
#ifdef _WIN32
    /* Windows (Win64) build steps using NASM and GCC */
    printf("To link and run:\n");
    printf(">> nasm -f win64 %s -o %s.obj\n", asmfile, outfile);
    printf(">> gcc %s.obj runtime.o -o %s.exe\n", outfile, outfile);
    printf(">> .\\\\%s.exe\n", outfile);
#else
    /* Unix/Linux (ELF64) build steps using NASM and GCC */
    printf("To link and run:\n");
    printf(">> nasm -f elf64 %s -o %s.o\n", asmfile, outfile);
    printf(">> gcc %s.o runtime.o -o %s\n", outfile, outfile);
    printf(">> ./%s\n", outfile);
#endif

    return 0;
}
