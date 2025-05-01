.data
msg_prompt:
    .asciiz "Enter n (positive integer): "
msg_result:
    .asciiz "Fibonacci Result: "
msg_error:
    .asciiz "Error: received negative integer\n"
newline:
    .asciiz "\n"


.text
.globl main

main:
    # user input
    li $v0, 4
    la $a0, msg_prompt
    syscall

    li $v0, 5
    syscall
    move $a0, $v0       # $a0 = n

    # go to fibonacci label
    jal fibonacci
    move $s0, $v0       # save result

    # print result
    li $v0, 4
    la $a0, msg_result
    syscall

    li $v0, 1
    move $a0, $s0
    syscall

    li $v0, 4
    la $a0, newline
    syscall

    li $v0, 10
    syscall


error:
    li $v0, 4
    la $a0, msg_error
    syscall

    # exit program
    li $v0, 10
    syscall


fibonacci:
############## fix here ##################
# Write your code only within this block marked by '#'.
# This is the body of the 'fibonacci' function.
# It is recommended to use the stack.
# Store the result in $v0.

    # $a0 is argument so first return if its 0 or 1

    # if a0 is < 0:

    blt $a0, $zero, error

    # else if 0 or 1:
    li $t0, 1
    ble $a0, $t0, fibonacci_base

    # else do fibonacci computation
    # return fibonacci(n-1) + fibonacci(n-2)

    addi $t1, $a0, -1
    addi $t2, $a0, -2

    # n-1
    addi $sp, $sp, -8
    sw $t2, 4($sp)
    sw $ra, 0($sp)

    move $a0, $t1
    jal fibonacci
    move $t1, $v0

    lw $ra, 0($sp)
    lw $t2, 4($sp)
    addi $sp, $sp, 8

    # n-2
    addi $sp, $sp, -8
    sw $t1, 4($sp)
    sw $ra, 0($sp)

    move $a0, $t2
    jal fibonacci
    move $t2, $v0

    lw $ra, 0($sp)
    lw $t1, 4($sp)
    addi $sp, $sp, 8

    add $v0, $t1, $t2

    jr $ra


##########################################


fibonacci_base:
############## fix here ##################
# Write your code only within this block marked by '#'.
# This block handles the base cases of the Fibonacci function.
# Return the result in $v0 as appropriate.

    # if 0 < input then =1, else 0
    slt $v0, $zero, $a0
    jr $ra

##########################################