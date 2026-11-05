# =============================================================================
# RISC201 test program: fpu_ops.asm
# Exercises all 8 Floating-Point Unit (FPU) instructions:
#   FLD, FST, FADD, FSUB, FMUL, FDIV, FCMP, FMOV
# =============================================================================

main:
    # Set data memory base pointer
    mov  r13, 0x1000

    # Initialize FP constants in data memory via integer stores:
    # 10.0f = 0x41200000
    li   r1, 0x41200000
    st   r1, 0[r13]
    # 4.0f  = 0x40800000
    li   r1, 0x40800000
    st   r1, 4[r13]
    # 2.0f  = 0x40000000
    li   r1, 0x40000000
    st   r1, 8[r13]

    # 1. FLD: Load single-precision floats into FPRs
    fld  f1, 0[r13]        # f1 = 10.0f
    fld  f2, 4[r13]        # f2 = 4.0f
    fld  f3, 8[r13]        # f3 = 2.0f

    # 2. FADD: 10.0f + 4.0f = 14.0f (0x41600000)
    fadd f4, f1, f2

    # 3. FSUB: 10.0f - 4.0f = 6.0f (0x40C00000)
    fsub f5, f1, f2

    # 4. FMUL: 4.0f * 2.0f = 8.0f (0x41000000)
    fmul f6, f2, f3

    # 5. FDIV: 10.0f / 2.0f = 5.0f (0x40A00000)
    fdiv f7, f1, f3

    # 6. FMOV: Register-to-register FP transfer (copy 14.0f to f8)
    fmov f8, f4

    # 7. FCMP: Float comparisons and conditional branches
    # Test 10.0 > 4.0 (Greater Than)
    fcmp f1, f2
    bgt  cmp1_ok
    b    trap_fail

cmp1_ok:
    # Test 4.0 < 10.0 (Less Than)
    fcmp f2, f1
    blt  cmp2_ok
    b    trap_fail

cmp2_ok:
    # Test 10.0 == 10.0 (Equal)
    fcmp f1, f1
    beq  cmp3_ok
    b    trap_fail

cmp3_ok:
    # 8. FST: Store FP results back to data memory
    fst  f4, 12[r13]       # mem[0x100C] = 14.0f
    fst  f5, 16[r13]       # mem[0x1010] = 6.0f
    fst  f6, 20[r13]       # mem[0x1014] = 8.0f
    fst  f7, 24[r13]       # mem[0x1018] = 5.0f
    fst  f8, 28[r13]       # mem[0x101C] = 14.0f

    # Write PASS sentinel to memory
    movu r1, 0xCAFE
    st   r1, 32[r13]       # mem[0x1020] = 0xCAFE (PASS)
    halt

trap_fail:
    movu r1, 0xDEAD
    st   r1, 32[r13]       # mem[0x1020] = 0xDEAD (FAIL)
    halt
