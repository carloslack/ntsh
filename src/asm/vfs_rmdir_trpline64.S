/*
 * Function trampoline demonstration
 *
 * Default detour for vfs_rmdir
 *
 * Copyright (c) 2020 Carlos Carvalho
 */
.data

.text

.extern k_vfs_rmdir

/**
 * original
 */
.globl orig
.type orig, @function
orig:
    /**
     * detour is done here
     * total: 6 bytes
     * gap: 3 bytes:
     *      push %ebp
     *      mov %esp, %ebp
     */
    pushq $hijack
    retq

    /**
     * jmp brings flow to here.
     * Some random code..
     *
     */
    xor %rbx,%rbx
    xor %rdx,%rdx
    movl 32(%esp), %ebx # first arg
    movl 36(%esp), %edx #second arg
    addl %edx, %ebx

    movl $1, %eax
    int $0x80

    leave
    ret

/**
 * intermediary gateway
 */
.globl vfs_gate
.type vfs_gate, @function
vfs_gate:
    /**
     * Here we restore the original
     * prologue plus it's 3 bytes we previously
     * stole and recovered after some reverse engineering:
     *      83 ec 08 [subl $0x18, %esp]
     */
    pushq %rbp
    movq %rsp, %rbp
    subq $8, %rsp

    #movl 32(%esp), %edi
    #movl 36(%esp), %esi

    leaq k_vfs_rmdir , %rcx
    leaq 0x6(%rcx), %rax

    jmpq *%rax


/**
 * hijack
 */
.globl hijack
.type hijack, @function
hijack:
    pushq %rbp
    movq %rsp, %rbp

    movq 8(%rsp), %rdi # first arg
    movq 12(%rsp), %rsi #second arg

    pushq %rsi #second arg
    pushq %rdi #first arg
    call vfs_gate
    addq $8, %rsp

    leave
    retq

