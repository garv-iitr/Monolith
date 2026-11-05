# =============================================================================
# RISC201 test program: memory_ops.asm
# Exercises memory loads, stores, bounds checking, and stack operations:
#   MOV, LD, ST, BNDCHK, PUSH, POP, SPUSH, SPOP, HALT
# =============================================================================

main:
    # Base data region pointer
    mov  r13, 0x1000

    # 1. BNDCHK: Bounds check valid data address (0x1000 is inside data region)
    bndchk r0, 0x1000

    # 2. ST: Initialize 4-element source array at 0x1000: [10, 20, 30, 40]
    mov  r1, 10
    st   r1, 0[r13]
    mov  r1, 20
    st   r1, 4[r13]
    mov  r1, 30
    st   r1, 8[r13]
    mov  r1, 40
    st   r1, 12[r13]

    # 3. LD & ST: Block copy loop from 0x1000 to destination buffer at 0x1020
    mov  r2, 0             # Loop counter i = 0
copy_loop:
    cmp  r2, 4
    beq  copy_done
    lsl  r3, r2, 2         # Byte offset = i * 4
    add  r5, r13, r3       # Source address: 0x1000 + offset
    ld   r4, 0[r5]         # Load word
    add  r6, r5, 32        # Destination address: 0x1020 + offset
    st   r4, 0[r6]         # Store word
    add  r2, r2, 1         # Increment index
    b    copy_loop

copy_done:
    # Verify copied buffer elements
    ld   r4, 32[r13]       # mem[0x1020] must equal 10
    cmp  r4, 10
    bne  trap_fail
    ld   r4, 44[r13]       # mem[0x102C] must equal 40
    cmp  r4, 40
    bne  trap_fail

    # 4. Stack Operations with Standard Macros (PUSH / POP)
    mov  r7, 1234
    push r7
    pop  r8
    cmp  r8, 1234
    bne  trap_fail

    # 5. Safe Stack Operations with Bounds Checking (SPUSH / SPOP)
    mov  r9, 5678
    mov  r10, 9999
    spush r9               # Safe push with bounds check
    spush r10
    spop  r11              # Safe pop (LIFO: 9999 first)
    spop  r12              # Safe pop (1234 next)
    cmp  r11, 9999
    bne  trap_fail
    cmp  r12, 5678
    bne  trap_fail

    # All checks passed: write PASS sentinel to mem[0x1040]
    movu r1, 0xCAFE
    st   r1, 64[r13]
    halt

trap_fail:
    movu r1, 0xDEAD
    st   r1, 64[r13]
    halt
