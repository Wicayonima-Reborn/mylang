global main
extern runtime_new_string
extern runtime_print_int
extern runtime_print_string
extern runtime_clone_string

section .text
main:
    push rbp
    mov rbp, rsp
    ; create string literal_1
    lea rdi, [rel literal_1]
    call runtime_new_string
    push rax
    pop rax
    mov [rbp-8], rax
    mov eax, 0
    mov rsp, rbp
    pop rbp
    ret

section .data
literal_1: db 0
