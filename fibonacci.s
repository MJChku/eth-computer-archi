.data
prompt1: .asciiz "Enter the sequence index\n"
prompt2: .asciiz "The Fibonacci value is:\n"
message: .asciiz "The Fibonacci value is:\n0"

.text

# Print prompt1
li $v0, 4
la $a0, prompt1
syscall

# Read integer
li $v0, 5
syscall

beq $v0, 0, equalToZero

# Call fibonacci
move $a0, $v0
jal fibonacci
move $a1, $v0 # save return value to a1

# Print prompt2
li $v0, 4
la $a0, prompt2
syscall

# Print result
li $v0, 1
move $a0, $a1
syscall

# Exit
li $v0, 10
syscall



## Function int fibonacci (int n)
fibonacci:
# Prologue
addi $sp, $sp, -12
sw $ra, 8($sp)
sw $s0, 4($sp)
sw $s1, 0($sp)
move $s0, $a0
li $v0, 1 # return value for terminal condition
ble $s0, 0x2, fibonacciExit # check terminal condition
addi $a0, $s0, -1 # set args for recursive call to f(n-1)
jal fibonacci
move $s1, $v0 # store result of f(n-1) to s1
addi $a0, $s0, -2 # set args for recursive call to f(n-2)
jal fibonacci
add $v0, $s1, $v0 # add result of f(n-1) to it
fibonacciExit:
# Epilogue
lw $ra, 8($sp)
lw $s0, 4($sp)
lw $s1, 0($sp)
addi $sp, $sp, 12
jr $ra
## End of function fibonacci

equalToZero:
li $v0, 4
la $a0, message
syscall
