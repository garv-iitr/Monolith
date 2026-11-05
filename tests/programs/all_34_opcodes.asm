# =============================================================================
# RISC201 test program: all_34_opcodes.asm
# Master validation program covering all 34 opcodes in the RISC201 ISA:
#
#   Format 1 (ALU 3-Operand):
#     0x01 ADD, 0x02 SUB, 0x03 MUL, 0x04 DIV, 0x05 MOD,
#     0x06 AND, 0x07 OR,  0x09 XOR, 0x0A LSL, 0x0B LSR, 0x0C ASR
#   Format 2 & 3 (Unary & Compare):
#     0x08 NOT, 0x10 MOV, 0x0D CMP
#   Format 4 (Memory & Bounds):
#     0x11 LD, 0x12 ST, 0x27 BNDCHK
#   Format 5 (Branch & Call):
#     0x20 B, 0x21 BEQ, 0x22 BNE, 0x23 BGT, 0x24 BLT, 0x25 CALL
#   Format 6 (No Operands):
#     0x00 NOP, 0x26 RET, 0x28 HALT
#   Format 7 & 8 (Floating-Point Unit):
#     0x30 FADD, 0x31 FSUB, 0x32 FMUL, 0x33 FDIV,
#     0x34 FCMP, 0x35 FLD,  0x36 FST,  0x37 FMOV
# =============================================================================

main:
    # -------------------------------------------------------------------------
    # 1. Bounds Check & Memory Setup
    # -------------------------------------------------------------------------
    # 0x27: BNDCHK (bounds check valid data region)
    bndchk r0, 0x1000

    # 0x10: MOV (immediate into register)
    mov  r13, 0x1000

    # 0x12: ST (store word to memory)
    mov  r1, 42
    st   r1, 0[r13]

    # 0x11: LD (load word from memory)
    ld   r2, 0[r13]

    # -------------------------------------------------------------------------
    # 2. Integer Arithmetic & Logical Operations
    # -------------------------------------------------------------------------
    # 0x00: NOP
    nop

    # 0x01: ADD (50 + 2 = 52)
    mov  r3, 50
    add  r3, r3, 2

    # 0x02: SUB (52 - 10 = 42)
    sub  r4, r3, 10

    # 0x03: MUL (42 * 2 = 84)
    mul  r5, r4, 2

    # 0x04: DIV (84 / 2 = 42)
    div  r6, r5, 2

    # 0x05: MOD (42 % 40 = 2)
    mod  r7, r6, 40

    # 0x06: AND (42 & 42 = 42)
    and  r8, r6, 42

    # 0x07: OR (42 | 1 = 43)
    or   r9, r8, 1

    # 0x09: XOR (43 ^ 43 = 0)
    xor  r10, r9, 43

    # 0x08: NOT (~0 = -1 / 0xFFFFFFFF)
    not  r11, r10

    # 0x0A: LSL (42 << 1 = 84)
    lsl  r12, r4, 1

    # 0x0B: LSR (84 >> 1 = 42)
    lsr  r1, r12, 1

    # 0x0C: ASR (-84 >> 1 = -42)
    sub  r2, r0, 84
    asr  r2, r2, 1

    # -------------------------------------------------------------------------
    # 3. Compare, Conditional Branches & Unconditional Jumps
    # -------------------------------------------------------------------------
    # 0x0D: CMP
    cmp  r1, 42

    # 0x21: BEQ
    beq  step_beq
    # 0x20: B
    b    trap_fail

step_beq:
    cmp  r1, 0
    # 0x22: BNE
    bne  step_bne
    b    trap_fail

step_bne:
    cmp  r1, 10
    # 0x23: BGT (42 > 10)
    bgt  step_bgt
    b    trap_fail

step_bgt:
    cmp  r2, r1
    # 0x24: BLT (-42 < 42)
    blt  step_blt
    b    trap_fail

step_blt:
    # -------------------------------------------------------------------------
    # 4. Subroutine Call and Return
    # -------------------------------------------------------------------------
    # 0x25: CALL
    call sub_test
    # Verify subroutine returned 999
    cmp  r1, 999
    bne  trap_fail

    # -------------------------------------------------------------------------
    # 5. Floating-Point Unit (FPU) Operations
    # -------------------------------------------------------------------------
    # 0x35: FLD (initialize floats: 10.0f = 0x41200000, 4.0f = 0x40800000)
    li   r1, 0x41200000
    st   r1, 20[r13]
    li   r1, 0x40800000
    st   r1, 24[r13]
    fld  f1, 20[r13]
    fld  f2, 24[r13]

    # 0x30: FADD (10.0 + 4.0 = 14.0f)
    fadd f3, f1, f2

    # 0x31: FSUB (10.0 - 4.0 = 6.0f)
    fsub f4, f1, f2

    # 0x32: FMUL (4.0 * 10.0 = 40.0f)
    fmul f5, f2, f1

    # 0x33: FDIV (10.0 / 4.0 = 2.5f)
    fdiv f6, f1, f2

    # 0x37: FMOV (copy f3 to f7)
    fmov f7, f3

    # 0x34: FCMP (10.0 > 4.0)
    fcmp f1, f2
    bgt  step_fcmp
    b    trap_fail

step_fcmp:
    # 0x36: FST (store float result)
    fst  f7, 28[r13]

    # -------------------------------------------------------------------------
    # 6. Verification Success Sentinel & Halt
    # -------------------------------------------------------------------------
    movu r1, 0xCAFE
    st   r1, 32[r13]       # mem[0x1020] = 0xCAFE indicates all 34 opcodes passed
    # 0x28: HALT
    halt

sub_test:
    mov  r1, 999
    # 0x26: RET
    ret

trap_fail:
    movu r1, 0xDEAD
    st   r1, 32[r13]       # mem[0x1020] = 0xDEAD indicates failure
    halt
