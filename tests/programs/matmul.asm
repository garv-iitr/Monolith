# =============================================================================
# RISC201 test program 2: 2x2 matrix multiply + FPU kernel
# =============================================================================
# Integer part:  C = A x B  (2x2, row-major)
#   A at 0x200 = [1 2; 3 4],  B at 0x220 = [5 6; 7 8]
#   C at 0x240 = [19 22; 43 50]
#
# FPU part (f1 = 1.5 at 0x260, f2 = 2.25 at 0x264):
#   fadd f3,f1,f2 = 3.75    -> 0x270 = 0x40700000
#   fsub f4,f1,f2 = -0.75   -> 0x274 = 0xBF400000
#   fmul f5,f1,f2 = 3.375   -> 0x278 = 0x40580000
#   fdiv f6,f1,f2 = 2/3     -> 0x27C = 0x3F2AAAAB
#   fcmp f1,f2  (f1 < f2)   -> takes the blt path -> 0x280 = 7
#   fmov f7,f2  = 2.25      -> 0x284 = 0x40100000
#
# Opcodes used: mov, xor, mul, add, lsl, ld, st, cmp, beq, blt, b,
#               fld, fst, fadd, fsub, fmul, fdiv, fcmp, fmov, bgt, halt
# =============================================================================

# i = 0
    mov  r1, 0
i_loop:
    cmp  r1, 2
    beq  done
    blt  i_body
    b    done
i_body:
# j = 0
    mov  r2, 0
j_loop:
    cmp  r2, 2
    beq  j_end
    blt  j_body
    b    j_end
j_body:
# sum = 0
    xor  r4, r4, r4
# k = 0
    mov  r3, 0
k_loop:
    cmp  r3, 2
    beq  k_end
    blt  k_body
    b    k_end
k_body:
    mov  r5, r1
# i * 2
    mul  r5, r5, 2
# i * 2 + k
    add  r5, r5, r3
# byte offset of A[i][k]
    lsl  r5, r5, 2
    mov  r6, r3
# k * 2
    mul  r6, r6, 2
# k * 2 + j
    add  r6, r6, r2
# byte offset of B[k][j]
    lsl  r6, r6, 2
# A[i][k]
    ld   r7, 0x200[r5]
# B[k][j]
    ld   r8, 0x220[r6]
    mul  r7, r7, r8
# sum += A[i][k] * B[k][j]
    add  r4, r4, r7
    add  r3, r3, 1
    b    k_loop
k_end:
    mov  r9, r1
# i * 2
    mul  r9, r9, 2
# i * 2 + j
    add  r9, r9, r2
# byte offset of C[i][j]
    lsl  r9, r9, 2
# C[i][j] = sum
    st   r4, 0x240[r9]
    add  r2, r2, 1
    b    j_loop
j_end:
    add  r1, r1, 1
    b    i_loop

done:
# f1 = 1.5
    fld  f1, 0x260[r0]
# f2 = 2.25
    fld  f2, 0x264[r0]
    fadd f3, f1, f2
    fst  f3, 0x270[r0]
    fsub f4, f1, f2
    fst  f4, 0x274[r0]
    fmul f5, f1, f2
    fst  f5, 0x278[r0]
    fdiv f6, f1, f2
    fst  f6, 0x27C[r0]
# f1 < f2 -> N = 1
    fcmp f1, f2
# not taken
    bgt  fbad
# taken
    blt  fgood
    b    fbad
fgood:
    mov  r10, 7
    st   r10, 0x280[r0]
    b    fcont
fbad:
    mov  r10, 99
    st   r10, 0x280[r0]
fcont:
    fmov f7, f2
    fst  f7, 0x284[r0]
    halt

