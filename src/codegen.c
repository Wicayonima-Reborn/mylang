/**
 * @file codegen.c
 * @brief x86_64 NASM backend supporting System V (Linux) and MS ABI (Windows).
 * * This generator implements a stack-based virtual machine approach where 
 * expression results are pushed onto the hardware stack and popped into 
 * registers for operations or function calls.
 */

#include "../include/codegen.h"
#include "../include/ast.h"
#include "../include/common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/** Tracks string literals to be emitted in the .data section. */
typedef struct Literal {
    char *s;
    int id;
    struct Literal *next;
} Literal;

/** Tracks local variable offsets relative to the Base Pointer (RBP). */
typedef struct VSlot {
    char name[MAX_IDENT];
    int offset;
    Type type;
    struct VSlot *next;
} VSlot;

/** Code Generator State Context. */
typedef struct CG {
    FILE *out;
    int stack_size;
    VSlot *slots;
    int label_counter;
    bool debug_borrow;
    Literal *lits;
    int lit_count;
} CG;

/* ---------------------------------------------------------
   SYMBOL & LITERAL MANAGEMENT
   --------------------------------------------------------- */

static VSlot *cg_find(CG *g, const char *name) {
    for (VSlot *v = g->slots; v; v = v->next)
        if (strcmp(v->name, name) == 0)
            return v;
    return NULL;
}

/** Allocates a new slot on the stack for a local variable. */
static void cg_add(CG *g, const char *name, Type t) {
    VSlot *v = xmalloc(sizeof(VSlot));
    snprintf(v->name, sizeof(v->name), "%s", name);

    /* x86_64 uses 8-byte alignment for standard types in this compiler */
    g->stack_size += 8;
    v->offset = -g->stack_size;
    v->type = t;
    v->next = g->slots;
    g->slots = v;
}

static int cg_new_label(CG *g) {
    return ++g->label_counter;
}

static int cg_register_literal(CG *g, const char *s) {
    Literal *L = xmalloc(sizeof(Literal));
    L->s = strdup(s);
    L->id = ++g->lit_count;
    L->next = g->lits;
    g->lits = L;
    return L->id;
}

/* ---------------------------------------------------------
   ABI-SPECIFIC EMISSION HELPERS
   --------------------------------------------------------- */

/** Emits the function prologue and establishes the stack frame. */
static void emit_prologue(CG *g) {
    fprintf(g->out,
        "global main\n"
        "extern runtime_new_string\n"
        "extern runtime_print_int\n"
        "extern runtime_print_string\n"
        "extern runtime_clone_string\n\n"
        "section .text\n"
        "main:\n"
        "    push rbp\n"
        "    mov rbp, rsp\n"
    );
}

/** Restores the stack frame and returns. */
static void emit_epilogue(CG *g) {
    fprintf(g->out,
        "    mov eax, 0\n"
        "    mov rsp, rbp\n"
        "    pop rbp\n"
        "    ret\n"
    );
}

static void cg_emit_expr(CG *g, Expr *e);

/**
 * @brief Handles string object creation.
 * Windows requires 32-byte "Shadow Space" (SPO) above the return address.
 */
static void cg_emit_string_literal(CG *g, int litid) {
#ifdef _WIN32
    fprintf(g->out,
        "    ; Windows x64 ABI: RCX for 1st arg + Shadow Space\n"
        "    lea rcx, [rel literal_%d]\n"
        "    sub rsp, 32\n"
        "    call runtime_new_string\n"
        "    add rsp, 32\n"
        "    push rax\n", litid, litid);
#else
    fprintf(g->out,
        "    ; System V ABI: RDI for 1st arg\n"
        "    lea rdi, [rel literal_%d]\n"
        "    call runtime_new_string\n"
        "    push rax\n", litid, litid);
#endif
}

/** Dispatches the appropriate runtime print function based on expression type. */
static void emit_print_call_for_expr(CG *g, Expr *arg) {
    fprintf(g->out, "    pop rax\n");

#ifdef _WIN32
    fprintf(g->out, "    mov rcx, rax\n    sub rsp, 32\n");
    if (arg && arg->type.kind == TY_STRING)
        fprintf(g->out, "    call runtime_print_string\n");
    else
        fprintf(g->out, "    call runtime_print_int\n");
    fprintf(g->out, "    add rsp, 32\n");
#else
    fprintf(g->out, "    mov rdi, rax\n");
    if (arg && arg->type.kind == TY_STRING)
        fprintf(g->out, "    call runtime_print_string\n");
    else
        fprintf(g->out, "    call runtime_print_int\n");
#endif
    fprintf(g->out, "    push 0\n"); /* Dummy return for statement-expressions */
}

/* ---------------------------------------------------------
   RECURSIVE CODE GENERATION (EXPRESSIONS & STATEMENTS)
   --------------------------------------------------------- */

static void cg_emit_expr(CG *g, Expr *e) {
    if (!e) return;
    switch (e->kind) {
    case E_INT_LIT:
        fprintf(g->out, "    mov rax, %ld\n    push rax\n", e->v.int_val);
        break;

    case E_STR_LIT:
        cg_emit_string_literal(g, cg_register_literal(g, e->v.str_val));
        break;

    case E_IDENT: {
        VSlot *v = cg_find(g, e->v.ident);
        if (!v) {
            errorf("codegen: unknown identifier '%s' at %d:%d\n",
                e->v.ident, e->line, e->col);
            exit(1);
        }
        fprintf(g->out,
            "    mov rax, [rbp%+d]\n"
            "    push rax\n",
            v->offset);
        break;
    }

    case E_CALL: {
        /* Push arguments in reverse order (standard C calling convention) */
        for (int i = e->v.call.nargs - 1; i >= 0; i--)
            cg_emit_expr(g, e->v.call.args[i]);

        const char *fn = e->v.call.name;
        if (strcmp(fn, "print") == 0) {
            emit_print_call_for_expr(g, (e->v.call.nargs > 0) ? e->v.call.args[0] : NULL);
        } else if (strcmp(fn, "clone") == 0) {
            fprintf(g->out, "    pop rax\n");
#ifdef _WIN32
            fprintf(g->out, "    mov rcx, rax\n    sub rsp, 32\n    call runtime_clone_string\n    add rsp, 32\n    push rax\n");
#else
            fprintf(g->out, "    mov rdi, rax\n    call runtime_clone_string\n    push rax\n");
#endif
        } else {
            errorf("codegen: unknown function '%s'\n", fn);
        }
        break;
    }

    case E_ADDR:
    case E_MUTADDR: {
        if (!e->v.inner || e->v.inner->kind != E_IDENT) {
            errorf("codegen: & expects identifier at %d:%d\n",
                e->line, e->col);
            exit(1);
        }

        VSlot *v = cg_find(g, e->v.inner->v.ident);
        if (!v) {
            errorf("codegen: unknown identifier '%s' in & at %d:%d\n",
                e->v.inner->v.ident, e->line, e->col);
            exit(1);
        }

        fprintf(g->out,
            "    lea rax, [rbp%+d]\n"
            "    push rax\n",
            v->offset);
        break;
    }
    default: errorf("codegen: unsupported expr kind %d\n", e->kind);
    }
}

static void cg_emit_stmt(CG *g, Stmt *s) {
    if (!s) return;
    switch (s->kind) {
    case S_DECL:
        cg_add(g, s->v.decl.name, s->v.decl.type);
        VSlot *v = cg_find(g, s->v.decl.name);
        if (s->v.decl.init) {
            cg_emit_expr(g, s->v.decl.init);
            fprintf(g->out, "    pop rax\n    mov [rbp%+d], rax\n", v->offset);
        } else {
            fprintf(g->out, "    mov qword [rbp%+d], 0\n", v->offset);
        }
        break;

    case S_EXPR:
        cg_emit_expr(g, s->v.expr);
        fprintf(g->out, "    add rsp, 8\n"); /* Cleanup stack after expression */
        break;

    case S_BLOCK:
        for (int i = 0; i < s->v.block.n; i++)
            cg_emit_stmt(g, s->v.block.stmts[i]);
        break;

    case S_IF: {
        int lbl = cg_new_label(g);
        cg_emit_expr(g, s->v.ifs.cond);
        fprintf(g->out, "    pop rax\n    cmp rax, 0\n    je .Lelse%d\n", lbl);
        cg_emit_stmt(g, s->v.ifs.then_s);
        fprintf(g->out, "    jmp .Lend%d\n.Lelse%d:\n", lbl, lbl);
        if (s->v.ifs.else_s) cg_emit_stmt(g, s->v.ifs.else_s);
        fprintf(g->out, ".Lend%d:\n", lbl);
        break;
    }

    case S_WHILE: {
        int lbl = cg_new_label(g);
        fprintf(g->out, ".Lwhile%d:\n", lbl);
        cg_emit_expr(g, s->v.wh.cond);
        fprintf(g->out, "    pop rax\n    cmp rax, 0\n    je .Lendwhile%d\n", lbl);
        cg_emit_stmt(g, s->v.wh.body);
        fprintf(g->out, "    jmp .Lwhile%d\n.Lendwhile%d:\n", lbl, lbl);
        break;
    }
    default: errorf("codegen: unsupported stmt\n");
    }
}

/** Finalizes the assembly file by emitting the .data section for strings. */
static void emit_literals(CG *g) {
    if (g->lit_count == 0) return;
    fprintf(g->out, "\nsection .data\n");
    for (Literal *L = g->lits; L; L = L->next) {
        fprintf(g->out, "literal_%d: db ", L->id);
        for (size_t i = 0; L->s[i]; i++)
            fprintf(g->out, "%u,", (unsigned int)(unsigned char)L->s[i]);
        fprintf(g->out, "0\n");
    }
}

/* ---------------------------------------------------------
   ENTRY POINT
   --------------------------------------------------------- */

int codegen_function(Function *f, const char *out_asm, const char *module_name, bool debug_borrow) {
    CG g = { .out = fopen(out_asm, "w"), .debug_borrow = debug_borrow };
    if (!g.out) return 1;

    emit_prologue(&g);
    cg_emit_stmt(&g, f->body);
    emit_epilogue(&g);
    emit_literals(&g);

    fclose(g.out);
    return 0;
}