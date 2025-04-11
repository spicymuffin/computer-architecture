.data
hello:
    .asciiz "Hello World!\n"


.text
.globl main


main:
    li $v0, 4
    la $a0, hello
    syscall

    jr $ra