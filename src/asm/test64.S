.data
.text
.globl func
.type func, @function

func:
    pushq %rbp
    movq %rsp, %rbp
    leave
    retq

.globl _start
_start:
    call func
    jmp bye
bye:
    mov $0, %bl
    mov $1, %al
    int $0x80
