.data
    input:
        .space 100              # space to store user input
    newline:
        .asciiz "\n"
    prompt:
        .asciiz "Enter postfix expression: "
    .align 2                    # uncomment when handling two-digit numbers
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
    # stores 47, which is the boundary betwween digits and ops
    li $s7, 47
    # stores 32, which is the ascii value of space
    li $s6, 32
    # stores 10, which is the ascii value of \n
    li $s5, 10
    # custom stack's stack pointer
    la $s0, stack

# main evaluation driver code
evaluate_loop:
    # walk through input
    lbu $t0, 0($a0)

    # if char is \0, finalize
    beq $t0, $zero, finalize
    # if char is \n, finalize
    beq $t0, $s5, finalize

    # if char is space, exit early
    beq $t0, $s6, step
    # if char is \n, print value on stack, exit

    # '0' to '9' is 48 to 57
    # '+', '-', '*', '/' are 43, 45, 42, 47 (le 47)
    ble $t0, $s7, operation

number:
    # if is digit, tokenize number, store on stack
    addi $sp, $sp, -4
    sw $ra, 0($sp)

    jal readint # steps a0 the appropriate amount

    lw $ra, 0($sp)
    addi $sp, $sp, 4

    j stack_store

operation:
    # if is operation, execute, store result on stack
    # load previous two operands into t0, t1
    lw $a1, -4($s0)
    lw $a2, 0($s0)

    addi, $s0, $s0, -8
    # load operation from t0 to a3
    move $a3, $t0

    # compute
    addi $sp, $sp, -8
    sw $t0, 4($sp)
    sw $ra, 0($sp)

    jal compute_op

    lw $ra, 0($sp)
    lw $t0, 4($sp)
    addi $sp, $sp, 8

    j stack_store

stack_store:
    # store result in the stack
    addi $s0, $s0, 4
    sw $v0, 0($s0)
    j evaluate_loop

step:
    addi, $a0, $a0, 1
    j evaluate_loop

finalize:
    # load value in custom stack to t0
    lw $t0, 0($s0)
    # print int
    move $a0, $t0
    li $v0, 1
    syscall
    # print newline
    li $a0, 10
    li $v0, 11
    syscall
    jal exit


### clobbers t0-2, modifies a0, returns in v0
## given address in a0, step a0 until it sees a space
## store the number as a 32bit word in v0
readint:
    # init accumulator
    xor $v0, $v0, $v0
    # init multiplier
    li $t1, 10
    # init space holder
    li $t2, 32

readint_loop:
    # read char into t0,
    lbu $t0, 0($a0)
    # if char is space, exit
    beq $t0, $t2, readint_loop_done
    # if char is null, exit
    beq $t0, $zero, readint_loop_done
    # if char is \n, exit
    beq $t0, $s5, readint_loop_done
    # normalize char
    addi $t0, $t0, -48

    # multiply accumulator by multiplier
    mult $v0, $t1
    mflo $v0
    # add char to accumulator
    add $v0, $v0, $t0
    # iterate ptr
    addi $a0, $a0, 1
    j readint_loop

readint_loop_done:
    # return
    jr $ra


### clobbers t0, modifies a0, returns in v0
## given operand 1, operand 2, operation in a1, a2, a3
## compute result, return in v1
compute_op:
    # 43 is add
    xori $t0, $a3, 43
    beq $t0, $zero, do_add

    # 45 is sub
    xori $t0, $a3, 45
    beq $t0, $zero, do_sub

    # 42 is mul
    xori $t0, $a3, 42
    beq $t0, $zero, do_mul

    # 47 is div
    xori $t0, $a3, 47
    beq $t0, $zero, do_div

    # if we are here then raise an error
    li $a0, 5
    jal print_error_and_exit

do_add:
    add $v0, $a1, $a2
    j compute_op_done
do_sub:
    sub $v0, $a1, $a2
    j compute_op_done
do_mul:
    mult $a1, $a2
    mflo $v0
    j compute_op_done
do_div:
    div $a1, $a2
    mflo $v0
    j compute_op_done

compute_op_done:
    # step a0
    addi $a0, $a0, 1
    # return
    jr $ra


### clobbers, but doesnt matter
## exit program
exit:
    # exit program
    li $v0, 10
    syscall

################ debug functionality #################

### clobbers stuff but who cares
## given error code in a0, print ERROR: errcode\n
## and exit the program (used for critical errors)
print_error_and_exit:
    # save $a0 (error code) since syscall 4 will overwrite it
    move $t1, $a0

    # build "ERROR " string on stack
    addi $sp, $sp, -8

    # 'E' 'R' 'R' 'O'
    li $t0, 0x4f525245
    sw $t0, 0($sp)
    # 'R' ' ' '\0' pad
    li $t0, 0x00202052
    sw $t0, 4($sp)

    # print
    li $v0, 4 # syscall print_string
    move $a0, $sp
    syscall

    # restore error code and print it
    move $a0, $t1
    li $v0, 1 # syscall print_int
    syscall

    # print newline "\n"
    li $a0, 10 # ASCII '\n'
    li $v0, 11 # syscall print_char
    syscall

    jal exit


dbg_print_int:
    move $s0, $a0
    li $v0, 1
    move $a0, $s0
    syscall
    jr $ra


dbg_print_char:
    move $s0, $a0
    li $v0, 11
    move $a0, $s0
    syscall
    jr $ra

# DEBUGPRINT
addi $sp, $sp, -8
sw $s0, 4($sp)
sw $ra, 0($sp)
move $s4, $a0
move $a0, $a3
jal dbg_print_char
move $a0, $s4
lw $s0, 4($sp)
lw $ra, 0($sp)
addi $sp, $sp, 8
