# =============================================================================
# RISC201 test program: factorial
# =============================================================================

main:
# use the stack initialized by the simulator
# n = 12
    mov  r1, 12
# r2 = factorial(12)
    call factorial
# store the result in memory at the data base
    li   r3, 0x10000
    st   r2, 0[r3]
    halt

factorial:
# r2 will hold the result, initialize to 1
    mov  r2, 1

fact_loop:
# compare n to 1
    cmp  r1, 1
# if n < 1, done
    blt  fact_done
    
# result = result * n
    mul  r2, r2, r1
# n = n - 1
    sub  r1, r1, 1
# repeat loop
    b    fact_loop

fact_done:
# return to caller, result in r2
    ret

