main:
    mov  r13, 0x102C
    mov  r1, 42
    st   r1, 0[r13]
    ld   r2, 0[r13]
    nop
    mov  r3, 50
    add  r3, r3, 2
    sub  r4, r3, 10
    mul  r5, r4, 2
    div  r6, r5, 2
    mod  r7, r6, 40
    and  r8, r6, 42
    or   r9, r8, 1
    xor  r10, r9, 43
    not  r11, r10
    lsl  r12, r4, 1
    lsr  r1, r12, 1
    sub  r2, r0, 84
    asr  r2, r2, 1
    cmp  r1, 42
    beq  step_beq
    b    trap_fail
step_beq:
    cmp  r1, 0
    bne  step_bne
    b    trap_fail
step_bne:
    cmp  r1, 10
    bgt  step_bgt
    b    trap_fail
step_bgt:
    cmp  r2, r1
    blt  step_blt
    b    trap_fail
step_blt:
    call sub_test
    cmp  r1, 999
    bne  trap_fail
    li   r1, 0x41200000
    st   r1, 20[r13]
    li   r1, 0x40800000
    st   r1, 24[r13]
    fld  f1, 20[r13]
    fld  f2, 24[r13]
    fadd f3, f1, f2
    fsub f4, f1, f2
    fmul f5, f2, f1
    fdiv f6, f1, f2
    fmov  f7, f3
    fst   f7, 28[r13]
    fcmp f1, f2
    bgt  step_fcmp
    b    trap_fail
step_fcmp:
    movu r1, 0xCAFE
    st   r1, 32[r13]
    halt
sub_test:
    mov  r1, 999
    ret
trap_fail:
    movu r1, 0xDEAD
    st   r1, 32[r13]    
    halt
