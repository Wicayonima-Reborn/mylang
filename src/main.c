/**
 * @file main.c
 * @brief Compiler frontend driver for MyLang.
 *
 * Orchestrates the compilation pipeline:
 *   - Command-line argument parsing
 *   - Lexical analysis and parsing (AST generation)
 *   - Semantic analysis (type checking and inference)
 *   - Borrow checking (ownership and borrowing rules)
 *   - x86_64 code generation (NASM assembly)
 *
 * The compiler exits with a non-zero status if any phase fails.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "../include/lexer.h"
#include "../include/parser.h"
#include "../include/semantic.h"
#include "../include/borrowchecker.h"
#include "../include/codegen.h"
#include "../include/common.h"

#define MAX_PATH 256

/**
 * @brief Prints usage information to stderr and terminates the process.
 *
 * @param progname The name of the executable as passed to main().
 */
static void print_usage_and_exit(const char *progname) {
    fprintf(stderr, "Usage: %s <input.my> -o <output> [--debug-borrow]\n", progname);
    exit(EXIT_FAILURE);
}

/**
 * @brief Parses command-line arguments and populates configuration pointers.
 *
 * @param argc      Argument count from main().
 * @param argv      Argument vector from main().
 * @param input     Output parameter: source file path.
 * @param outfile   Output parameter: output binary base name.
 * @param debug     Output parameter: debug_borrow flag.
 * @return true if arguments are valid, false otherwise.
 */
static bool parse_arguments(int argc, char **argv,
                            const char **input,
                            const char **outfile,
                            bool *debug) {
    if (argc < 4) {
        print_usage_and_exit(argv[0]);
        return false;
    }

    *input = NULL;
    *outfile = NULL;
    *debug = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--debug-borrow") == 0) {
            *debug = true;
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                print_usage_and_exit(argv[0]);
                return false;
            }
            *outfile = argv[++i];
        } else if (argv[i][0] != '-') {
            *input = argv[i];
        }
    }

    if (!*input || !*outfile) {
        print_usage_and_exit(argv[0]);
        return false;
    }

    return true;
}

/**
 * @brief Constructs the assembly output filename from the output base name.
 *
 * @param outfile   Base output name (e.g., "program").
 * @param asmfile   Output buffer to hold the .asm filename.
 */
static void build_assembly_filename(const char *outfile, char *asmfile) {
    snprintf(asmfile, MAX_PATH, "%s.asm", outfile);
}

/**
 * @brief Displays post-compilation build instructions for the user.
 *
 * @param asmfile   Assembly file name.
 * @param outfile   Output binary base name.
 */
static void print_build_instructions(const char *asmfile, const char *outfile) {
    printf("Assembly generated: %s\n", asmfile);
    printf("\nLinking instructions:\n");

#ifdef _WIN32
    printf("  nasm -f win64 %s -o %s.obj\n", asmfile, outfile);
    printf("  gcc %s.obj runtime.o -o %s.exe\n", outfile, outfile);
    printf("  .\\%s.exe\n", outfile);
#else
    printf("  nasm -f elf64 %s -o %s.o\n", asmfile, outfile);
    printf("  gcc %s.o runtime.o -o %s\n", outfile, outfile);
    printf("  ./%s\n", outfile);
#endif
}

/**
 * @brief Main entry point for the MyLang compiler.
 *
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line argument strings.
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on error.
 */
int main(int argc, char **argv) {
    const char *input = NULL;
    const char *outfile = NULL;
    bool debug_borrow = false;

    /* Parse command-line arguments */
    if (!parse_arguments(argc, argv, &input, &outfile, &debug_borrow)) {
        return EXIT_FAILURE;
    }

    /* Prepare output assembly file path */
    char asmfile[MAX_PATH];
    build_assembly_filename(outfile, asmfile);

    /* Phase 1: Parse source code into AST */
    Function *func = parse_program(input);
    if (!func) {
        fprintf(stderr, "Error: parsing failed for '%s'\n", input);
        return EXIT_FAILURE;
    }

    /* Phase 2: Semantic analysis (type checking and inference) */
    semantic_check(func, input);

    /* Phase 3: Borrow checking (ownership and borrowing rules) */
    borrow_check(func, input);

    /* Phase 4: Code generation (x86_64 NASM assembly) */
    if (codegen_function(func, asmfile, outfile, debug_borrow) != 0) {
        fprintf(stderr, "Error: code generation failed for '%s'\n", input);
        return EXIT_FAILURE;
    }

    /* Phase 5: Display build instructions */
    print_build_instructions(asmfile, outfile);

    return EXIT_SUCCESS;
}
