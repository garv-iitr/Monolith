# =============================================================================
# RISC201 test program: control_flow.asm
# Exercises control flow, conditional branches, calls, and returns:
#   B, BEQ, BNE, BGT, BLT, CALL, RET, NOP, HALT
# =============================================================================

main:
    # Initialize base registers
    mov  r13, 0x1000       # Data memory result base
    mov  r1, 10
    mov  r2, 20
    sub  r3, r0, 20        # r3 = -20

    # 1. Unconditional Branch B
    b    test_b
    b    trap_fail

test_b:
    nop

    # 2. BEQ (Branch if Equal)
    cmp  r1, 10
    beq  test_beq_taken
    b    trap_fail

test_beq_taken:
    cmp  r1, r2
    beq  trap_fail         # 10 != 20, must not branch

    # 3. BNE (Branch if Not Equal)
    cmp  r1, r2
    bne  test_bne_taken
    b    trap_fail

test_bne_taken:
    cmp  r1, 10
    bne  trap_fail         # 10 == 10, must not branch

    # 4. BGT (Branch if Greater Than - Signed)
    cmp  r2, r1            # 20 > 10
    bgt  test_bgt_pos_taken
    b    trap_fail

test_bgt_pos_taken:
    cmp  r1, r2            # 10 not > 20
    bgt  trap_fail

    cmp  r1, r3            # 10 > -20 (signed check)
    bgt  test_bgt_signed_taken
    b    trap_fail

test_bgt_signed_taken:
    cmp  r3, r1            # -20 not > 10
    bgt  trap_fail

    # 5. BLT (Branch if Less Than - Signed)
    cmp  r1, r2            # 10 < 20
    blt  test_blt_pos_taken
    b    trap_fail

test_blt_pos_taken:
    cmp  r2, r1            # 20 not < 10
    blt  trap_fail

    cmp  r3, r1            # -20 < 10 (signed check)
    blt  test_blt_signed_taken
    b    trap_fail

test_blt_signed_taken:
    cmp  r1, r3            # 10 not < -20
    blt  trap_fail

    # 6. CALL and RET (Nested Subroutine Calls)
    # Start accumulator r4 at 100
    mov  r4, 100
    call func_outer
    # func_outer adds 30, so r4 should now be 130
    cmp  r4, 130
    beq  pass
    b    trap_fail

func_outer:
    # Outer subroutine calls inner subroutine
    add  r4, r4, 10        # r4 = 110
    call func_inner
    # Back from inner: r4 should be 120
    add  r4, r4, 10        # r4 = 130
    ret

func_inner:
    # Inner subroutine
    add  r4, r4, 10        # r4 = 120
    ret

pass:
    movu r5, 0xCAFE
    st   r5, 0[r13]        # mem[0x1000] = 0xCAFE (PASS)
    halt

trap_fail:
    movu r5, 0xDEAD
    st   r5, 0[r13]        # mem[0x1000] = 0xDEAD (FAIL)
    halt
