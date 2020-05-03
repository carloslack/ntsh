/*
 * @src/asm/jmp64.S
 * .globl _start
 * _start:
 *     mov %rax, 0x1122334455667788
 *     mov %rax, %rax
 *     jmpq *%rax
 */
unsigned char JMP[] =   "\x48\xa3\x88\x77\x66\x55\x44"
                        "\x33\x22\x11"
                        "\x48\x89\xc0"
                        "\xff\xe0";
#define JMPLEN 15
