global main
extern runtime_new_string
extern runtime_print_int
extern runtime_print_string
extern runtime_clone_string

section .text
main:
    push rbp
    mov rbp, rsp
    ; create string literal_1 (win64)
    lea rcx, [rel literal_1]
    sub rsp, 32
    call runtime_new_string
    add rsp, 32
    push rax
    pop rax
    mov [rbp-8], rax
    mov rax, [rbp-8]
    push rax
    pop rax
    mov rcx, rax
    sub rsp, 32
    call runtime_print_string
    add rsp, 32
    push 0
    add rsp, 8
    mov rax, 5
    push rax
    pop rax
    mov [rbp-16], rax
    mov rax, [rbp-16]
    push rax
    pop rax
    mov rcx, rax
    sub rsp, 32
    call runtime_print_int
    add rsp, 32
    push 0
    add rsp, 8
    mov eax, 0
    mov rsp, rbp
    pop rbp
    ret

section .data
literal_1: db 72,101,108,108,111,33,0
