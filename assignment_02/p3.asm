.data
    input:      
        .space 100              # space to store user input
    newline:    
        .asciiz "\n"
    prompt:     
        .asciiz "Enter postfix expression: "
#    .align 2                   # uncomment when handling two-digit numbers
    stack:      
        .space 64               # space to store integers

.text
.globl main

main:
    # print prompt
    li $v0, 4
    la $a0, prompt
    syscall

    # get user input
    li $v0, 8
    la $a0, input
    li $a1, 100
    syscall

####################################################
############# You can edit below ###################
# Write Freely