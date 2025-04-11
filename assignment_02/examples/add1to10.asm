.data

newline:
    .asciiz "\n"

.text
.globl main

main:
    li $t0, 1       # $t0 = i 
    li $t1, 10      # $t1 = 10 
    li $t2, 0       # $t2 = sum 

loop:                       # function/label
    add $t2, $t2, $t0       # sum += i
    addi $t0, $t0, 1        # i++
    ble $t0, $t1, loop      # if i <= 10, loop

    li $v0, 1               # print result
    move $a0, $t2
    syscall

    li $v0, 4               # print newline
    la $a0, newline
    syscall

    jr $ra                  # exit