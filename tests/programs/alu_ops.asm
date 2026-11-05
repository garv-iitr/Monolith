# =============================================================================
# RISC201 test program: alu_ops.asm
# Exercises all 14 Integer ALU instructions + Data Movement & System:
#   ADD, SUB, MUL, DIV, MOD, AND, OR, XOR, NOT, LSL, LSR, ASR, CMP, NOP,
#   MOV, ST, LD, HALT
# =============================================================================

main:
    # Set data base address at 0x1000
    mov  r13, 0x1000

    # 1. ADD: 15 + 27 = 42
    mov  r1, 15
    mov  r2, 27
    add  r3, r1, r2
    st   r3, 0[r13]        # mem[0x1000] = 42

    # 2. SUB: 100 - 58 = 42
    mov  r1, 100
    sub  r4, r1, 58
    st   r4, 4[r13]        # mem[0x1004] = 42

    # 3. MUL: 6 * 7 = 42
    mov  r1, 6
    mul  r5, r1, 7
    st   r5, 8[r13]        # mem[0x1008] = 42

    # 4. DIV: 420 / 10 = 42
    mov  r1, 420
    div  r6, r1, 10
    st   r6, 12[r13]       # mem[0x100C] = 42

    # 5. MOD: 142 % 100 = 42
    mov  r1, 142
    mod  r7, r1, 100
    st   r7, 16[r13]       # mem[0x1010] = 42

    # 6. AND: 0x3F & 0x2A = 42 (0x2A = 42)
    mov  r1, 0x3F
    and  r8, r1, 42
    st   r8, 20[r13]       # mem[0x1014] = 42

    # 7. OR: 0x28 | 0x02 = 42 (0x28 = 40, 0x02 = 2)
    mov  r1, 40
    or   r9, r1, 2
    st   r9, 24[r13]       # mem[0x1018] = 42

    # 8. XOR: 0x7E ^ 0x54 = 42 (126 ^ 84 = 42)
    mov  r1, 126
    xor  r10, r1, 84
    st   r10, 28[r13]      # mem[0x101C] = 42

    # 9. NOT: ~(-43) = 42
    # In 32-bit two's complement: -43 = 0xFFFFFFD5, NOT(0xFFFFFFD5) = 0x0000002A = 42
    sub  r1, r0, 43
    not  r11, r1
    st   r11, 32[r13]      # mem[0x1020] = 42

    # 10. LSL: 21 << 1 = 42
    mov  r1, 21
    lsl  r12, r1, 1
    st   r12, 36[r13]      # mem[0x1024] = 42

    # 11. LSR: 84 >> 1 = 42
    mov  r1, 84
    lsr  r2, r1, 1
    st   r2, 40[r13]       # mem[0x1028] = 42

    # 12. ASR: -84 >> 1 = -42
    sub  r1, r0, 84
    asr  r2, r1, 1
    st   r2, 44[r13]       # mem[0x102C] = -42

    # 13. CMP & NOP:
    nop
    cmp  r3, 42            # 42 == 42 -> Z flag set
    beq  cmp_ok
    mov  r1, 0             # Fail indicator
    st   r1, 48[r13]
    halt

cmp_ok:
    nop
    mov  r1, 1             # Pass indicator
    st   r1, 48[r13]       # mem[0x1030] = 1
    halt
