@ -----------------------------------------------------------------------------
@ Function: hamming
@ -----------------------------------------------------------------------------
@ Task: Compute the Hamming distance between two integer arrays.
@
@ Parameters:
@   R0 -> pointer to first array (xs)
@   R1 -> pointer to second array (ys)
@   R2 -> length of arrays (seqlen)
@
@ Return:
@   R0 -> Hamming distance (integer)
@
@ Registers used:
@   R3 -> counter (result)
@   R4 -> current element from xs
@   R5 -> current element from ys
@ -----------------------------------------------------------------------------

.global hamming
	
hamming:
	PUSH {R4, R5}
    MOV R3, #0

loop_start:
    @ Check if all elements processed
    CMP R2, #0
    BEQ loop_end

    @ Load current elements from both arrays
    LDR R4, [R0], #4
    LDR R5, [R1], #4

    @ Compare: if not equal, increase hamming distance
    CMP R4, R5
    ADDNE R3, R3, #1

    @ Decrease remaining length
    SUB R2, R2, #1
    
    @ Repeat loop
    B loop_start

loop_end:
    MOV R0, R3
    POP {R4, R5}

    @ Return to caller
	BX   LR

@ Test data	
.data
.equ VAL1, 1
.equ VAL2, 2	

@ Indicate to the linker that the code in this file does not need the stack
@ to be executable. (Recent versions of GNU ld warn if this is not present.)
.section .note.GNU-stack,"",%progbits
