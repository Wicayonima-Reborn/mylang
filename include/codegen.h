#ifndef CODEGEN_H
#define CODEGEN_H
#include "ast.h"
int codegen_function(Function *f, const char *out_asm, const char *module_name, bool debug_borrow);
#endif