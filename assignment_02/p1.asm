.data
buffer:
    .space 100
msg_prompt:
    .asciiz "Enter string: "
msg_result:
    .asciiz "Reverse string: "
newline:
    .asciiz "\n"


.text
.globl main


main:
    # print prompt
    li $v0, 4
    la $a0, msg_prompt
    syscall

    # get user input (string)
    li $v0, 8
    la $a0, buffer
    li $a1, 100
    syscall

##############Fix Here###############
# Write your code only within this block marked by '#'
# DO NOT modify any code outside this section



#####################################

finish:
    li $v0, 4
    la $a0, newline
    syscall

    # exit program
    li $v0, 10
    syscall


