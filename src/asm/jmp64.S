/*
 * x86-64 jmp to arbitrary address
 * Copyright (c) 2020 Carlos Carvalho
 */
.data
.text

.globl _start
_start:
    mov %rax, 0x1122334455667788
    mov %rax, %rax
    jmpq *%rax
