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


##########################################


fibonacci_base:
############## fix here ##################
# Write your code only within this block marked by '#'.
# This block handles the base cases of the Fibonacci function.
# Return the result in $v0 as appropriate.



##########################################