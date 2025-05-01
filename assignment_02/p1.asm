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

# do reverse
    li $t2, 0x0A # load \n to $t2
    li $s0, -1 # initialize length val
    move $t1, $a0 # copy ptr to t1
strlen_loop:
    lb $t0, 0($t1) # $t0 holds value of buffer[$t1]
    addi $t1, $t1, 1 # increment ptr
    addi $s0, $s0, 1 # increment length
    bne $t0, $t2, strlen_loop # if the letter we read is \n leave

# overwrite \n in the buffer with a null
    add $t0, $a0, $s0
    sb $zero, 0($t0)

# start reversing
    # reverse while lptr < rptr
    # init ptrs
    move $t0, $a0
    add $t1, $a0, $s0
    # dec rptr by 1 bc index = size - 1
    addi $t1, $t1, -1
    # gcc style while strlen_loop
    # negative invariant
    bge $t0, $t1, reverse_done
reverse_loop:
    # loop body
    # swap at lptr and rptr
    lb $t2, 0($t0)
    lb $t3, 0($t1)
    sb $t2, 0($t1)
    sb $t3, 0($t0)

    # inc lptr, dec rptr
    addi $t0, $t0, 1
    addi $t1, $t1, -1
    # check invariant
    blt $t0, $t1, reverse_loop
reverse_done:
    # done, yay
# print the reversed string
    li $v0, 4
    la $a0, msg_result
    syscall

    li $v0, 4
    la $a0, buffer  # buffer now holds reversed, 0-terminated string
    syscall
#####################################

finish:
    li $v0, 4
    la $a0, newline
    syscall

    # exit program
    li $v0, 10
    syscall


