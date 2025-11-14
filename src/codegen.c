/* src/codegen.c
 * Cross-platform code generator for MyLang.
 * Emits NASM x86_64 assembly that works on:
 *   - Linux (SysV) using nasm -f elf64
 *   - Windows (MS ABI) using nasm -f win64
 *
 * Key points:
 *  - For Linux first arg in RDI, for Windows in RCX.
 *  - For Windows, allocate 32-byte shadow space before calling C functions.
 *  - String literals are emitted in .data as bytes.
 */

#include "../include/codegen.h"
#include "../include/ast.h"
#include "../include/common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef struct Literal {
    char *s;
    int id;
    struct Literal *next;
} Literal;

typedef struct VSlot {
    char name[MAX_IDENT];
    int offset;
    Type type;
    struct VSlot *next;
} VSlot;

typedef struct CG {
    FILE *out;
    int stack_size;
    VSlot *slots;
    int label_counter;
    bool debug_borrow;

    Literal *lits;
    int lit_count;
} CG;

/* ---------------- helpers ---------------- */

static VSlot* cg_find(CG *g, const char *name) {
    for (VSlot *v = g->slots; v; v = v->next)
        if (strcmp(v->name, name) == 0) return v;
    return NULL;
}

static void cg_add(CG *g, const char *name, Type t) {
    VSlot *v = xmalloc(sizeof(VSlot));
    strncpy(v->name, name, MAX_IDENT-1);
    v->name[MAX_IDENT-1] = 0;
    g->stack_size += 8;
    v->offset = -g->stack_size;
    v->type = t;
    v->next = g->slots;
    g->slots = v;
}

static int cg_new_label(CG *g) {
    return ++g->label_counter;
}

/* literal management */
static int cg_register_literal(CG *g, const char *s) {
    Literal *L = xmalloc(sizeof(Literal));
    L->s = strdup(s);
    L->id = ++g->lit_count;
    L->next = g->lits;
    g->lits = L;
    return L->id;
}

/* ---------------- emit helpers ---------------- */

static void emit_prologue(CG *g) {
    fprintf(g->out,
        "global main\n"
        "extern runtime_new_string\n"
        "extern runtime_print_int\n"
        "extern runtime_print_string\n"
        "extern runtime_clone_string\n"
        "\n"
        "section .text\n"
        "main:\n"
        "    push rbp\n"
        "    mov rbp, rsp\n"
    );
}

static void emit_epilogue(CG *g) {
    fprintf(g->out,
        "    mov eax, 0\n"
        "    mov rsp, rbp\n"
        "    pop rbp\n"
        "    ret\n"
    );
}

/* ---------------- codegen forward ---------------- */

static void cg_emit_expr(CG *g, Expr *e);

/* emit runtime_new_string call for a registered literal id
   Handles OS ABI differences for argument register & shadow space. */
static void cg_emit_string_literal(CG *g, int litid) {
#ifdef _WIN32
    /* Windows: arg1 -> RCX, need 32-byte shadow space */
    fprintf(g->out,
        "    ; create string literal_%d (win64)\n"
        "    lea rcx, [rel literal_%d]\n"
        "    sub rsp, 32\n"
        "    call runtime_new_string\n"
        "    add rsp, 32\n"
        "    push rax\n",
        litid, litid
    );
#else
    /* Linux SysV: arg1 -> RDI */
    fprintf(g->out,
        "    ; create string literal_%d (sysv)\n"
        "    lea rdi, [rel literal_%d]\n"
        "    call runtime_new_string\n"
        "    push rax\n",
        litid, litid
    );
#endif
}

/* choose print variant using e->type when available
   We assume argument value is pushed (top of stack). */
static void emit_print_call_for_expr(CG *g, Expr *arg) {
    /* pop value into temp reg and move into arg reg depending on OS */
    /* We'll use rax as temp then move to RCX/RDI as needed. */
    fprintf(g->out, "    pop rax\n");

#ifdef _WIN32
    /* move to RCX, allocate shadow space */
    fprintf(g->out,
        "    mov rcx, rax\n"
        "    sub rsp, 32\n"
    );
    if (arg && arg->type.kind == TY_STRING) {
        fprintf(g->out, "    call runtime_print_string\n");
    } else if (arg && arg->type.kind == TY_INT) {
        fprintf(g->out, "    call runtime_print_int\n");
    } else if (arg && arg->kind == E_STR_LIT) {
        fprintf(g->out, "    call runtime_print_string\n");
    } else {
        fprintf(g->out, "    call runtime_print_int\n");
    }
    fprintf(g->out, "    add rsp, 32\n");
#else
    /* Linux */
    fprintf(g->out,
        "    mov rdi, rax\n"
    );
    if (arg && arg->type.kind == TY_STRING) {
        fprintf(g->out, "    call runtime_print_string\n");
    } else if (arg && arg->type.kind == TY_INT) {
        fprintf(g->out, "    call runtime_print_int\n");
    } else if (arg && arg->kind == E_STR_LIT) {
        fprintf(g->out, "    call runtime_print_string\n");
    } else {
        fprintf(g->out, "    call runtime_print_int\n");
    }
#endif

    /* push dummy return value to keep expression semantics */
    fprintf(g->out, "    push 0\n");
}

/* ---------------- expression emitter ---------------- */

static void cg_emit_expr(CG *g, Expr *e) {
    if (!e) return;

    switch (e->kind) {

    case E_INT_LIT:
        fprintf(g->out,
            "    mov rax, %ld\n"
            "    push rax\n",
            e->v.int_val
        );
        break;

    case E_STR_LIT: {
        /* register literal and emit create call */
        int id = cg_register_literal(g, e->v.str_val);
        cg_emit_string_literal(g, id);
        break;
    }

    case E_IDENT: {
        VSlot *v = cg_find(g, e->v.ident);
        if (!v)
            errorf("codegen: unknown identifier '%s' at %d:%d\n", e->v.ident, e->line, e->col);
        fprintf(g->out,
            "    mov rax, [rbp%+d]\n"
            "    push rax\n",
            v->offset
        );
        break;
    }

    case E_CALL: {
        /* evaluate args right-to-left */
        for (int i = e->v.call.nargs - 1; i >= 0; i--)
            cg_emit_expr(g, e->v.call.args[i]);

        const char *fn = e->v.call.name;

        if (strcmp(fn, "print") == 0) {
            Expr *arg = (e->v.call.nargs > 0) ? e->v.call.args[0] : NULL;
            emit_print_call_for_expr(g, arg);
            break;
        }

        if (strcmp(fn, "clone") == 0) {
            /* pop into temp then move to arg reg and call clone */
            fprintf(g->out, "    pop rax\n");
#ifdef _WIN32
            fprintf(g->out,
                "    mov rcx, rax\n"
                "    sub rsp, 32\n"
                "    call runtime_clone_string\n"
                "    add rsp, 32\n"
                "    push rax\n"
            );
#else
            fprintf(g->out,
                "    mov rdi, rax\n"
                "    call runtime_clone_string\n"
                "    push rax\n"
            );
#endif
            break;
        }

        errorf("codegen: unknown function '%s' at %d:%d\n", fn, e->line, e->col);
        break;
    }

    case E_ADDR:
    case E_MUTADDR: {
        if (!e->v.inner || e->v.inner->kind != E_IDENT)
            errorf("codegen: & expects identifier at %d:%d\n", e->line, e->col);
        VSlot *v = cg_find(g, e->v.inner->v.ident);
        if (!v)
            errorf("codegen: unknown var '%s' in & at %d:%d\n",
                   e->v.inner->v.ident, e->line, e->col);

        fprintf(g->out,
            "    lea rax, [rbp%+d]\n"
            "    push rax\n",
            v->offset
        );
        break;
    }

    default:
        errorf("codegen: unsupported expr kind %d at %d:%d\n", e->kind, e->line, e->col);
    }
}

/* ---------------- statement emitter ---------------- */

static void cg_emit_stmt(CG *g, Stmt *s) {
    if (!s) return;

    switch (s->kind) {
    case S_DECL: {
        cg_add(g, s->v.decl.name, s->v.decl.type);
        VSlot *v = cg_find(g, s->v.decl.name);
        if (!v) errorf("internal codegen error\n");

        if (s->v.decl.init) {
            cg_emit_expr(g, s->v.decl.init);
            fprintf(g->out,
                "    pop rax\n"
                "    mov [rbp%+d], rax\n",
                v->offset
            );
        } else {
            fprintf(g->out, "    mov qword [rbp%+d], 0\n", v->offset);
        }
        break;
    }

    case S_EXPR:
        cg_emit_expr(g, s->v.expr);
        /* drop result of expression statement */
        fprintf(g->out, "    add rsp, 8\n");
        break;

    case S_BLOCK:
        for (int i = 0; i < s->v.block.n; i++) cg_emit_stmt(g, s->v.block.stmts[i]);
        break;

    case S_IF: {
        int lbl = cg_new_label(g);
        cg_emit_expr(g, s->v.ifs.cond);
        fprintf(g->out,
            "    pop rax\n"
            "    cmp rax, 0\n"
            "    je .Lelse%d\n",
            lbl
        );
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
        fprintf(g->out,
            "    pop rax\n"
            "    cmp rax, 0\n"
            "    je .Lendwhile%d\n",
            lbl
        );
        cg_emit_stmt(g, s->v.wh.body);
        fprintf(g->out, "    jmp .Lwhile%d\n.Lendwhile%d:\n", lbl, lbl);
        break;
    }

    default:
        errorf("codegen: unsupported stmt kind %d\n", s->kind);
    }
}

/* ---------------- emit data section for literals ---------------- */

static void emit_literals(CG *g) {
    if (g->lit_count == 0) return;

    fprintf(g->out, "\nsection .data\n");
    /* We stored literals as a singly-linked list (newest first). Order doesn't matter. */
    for (Literal *L = g->lits; L; L = L->next) {
        fprintf(g->out, "literal_%d: db ", L->id);
        for (size_t i = 0; L->s[i]; i++) {
            unsigned char c = (unsigned char)L->s[i];
            fprintf(g->out, "%u,", (unsigned int)c);
        }
        fprintf(g->out, "0\n");
    }
}

/* ---------------- top level ---------------- */

int codegen_function(Function *f, const char *out_asm, const char *module_name, bool debug_borrow) {
    CG g = {0};
    g.out = fopen(out_asm, "w");
    if (!g.out) { perror("fopen"); return 1; }
    g.stack_size = 0;
    g.slots = NULL;
    g.label_counter = 0;
    g.debug_borrow = debug_borrow;
    g.lits = NULL;
    g.lit_count = 0;

    emit_prologue(&g);
    cg_emit_stmt(&g, f->body);
    emit_epilogue(&g);

    emit_literals(&g);

    fclose(g.out);
    return 0;
}