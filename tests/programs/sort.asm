# =============================================================================
# RISC201 test program 3: bubble sort + checksum
# =============================================================================
# Bubble-sorts [5,2,9,1,7,3,8,4] at 0x300 in place, ascending, using the
# xor-swap trick (no temporaries).  Then runs a checksum pass:
#   bndchk r0, #0x300        validate the array range (memchk)
#   sum of sorted array = 39
#   mod  39 % 10 = 9         -> 0x320 = 9
#   div  39 / 10 = 3, and 0xFF = 3, or 0x100 = 0x103,
#   lsr  4 -> 16, asr 1 -> 8, not -> 0xFFFFFFF7  -> 0x324 = 0xFFFFFFF7
#
# Expected: 0x300..0x31C = [1,2,3,4,5,7,8,9], 0x320 = 9, 0x324 = 0xFFFFFFF7
#
# Opcodes used: mov, cmp, beq, blt, bgt, b, lsl, ld, st, sub, add, xor,
#               bndchk, mod, div, and, or, lsr, asr, not, halt
# =============================================================================

    mov  r1, #7            ; outer i = n - 1
outer:
    cmp  r1, #0
    beq  checksum
    blt  checksum
    mov  r2, #0            ; inner j = 0
inner:
    cmp  r2, r1            ; j < i ?
    beq  inner_end
    bgt  inner_end
    mov  r3, r2
    lsl  r3, r3, #2        ; &a[j]
    ld   r4, 0x300[r3]     ; a[j]
    add  r3, r3, #4
    ld   r5, 0x300[r3]     ; a[j+1]
    cmp  r4, r5
    bgt  swap              ; a[j] > a[j+1]
    b    noswap
swap:
    sub  r3, r3, #4        ; &a[j]
    ld   r4, 0x300[r3]
    add  r6, r3, #4        ; &a[j+1]
    ld   r5, 0x300[r6]
    xor  r4, r4, r5        ; xor-swap, no temporary
    xor  r5, r5, r4
    xor  r4, r4, r5
    st   r4, 0x300[r3]
    st   r5, 0x300[r6]
noswap:
    add  r2, r2, #1
    b    inner
inner_end:
    sub  r1, r1, #1
    b    outer

checksum:
    bndchk r0, #0x300      ; memchk the array range (6-stage CHECK)
    mov  r6, #0            ; sum = 0
    mov  r7, #0            ; index = 0
csum_loop:
    cmp  r7, #8
    beq  csum_done
    mov  r8, r7
    lsl  r8, r8, #2
    ld   r4, 0x300[r8]
    add  r6, r6, r4        ; sum += a[index]
    add  r7, r7, #1
    b    csum_loop
csum_done:
    mod  r6, r6, #10       ; 39 % 10 = 9
    st   r6, 0x320[r0]
    mov  r6, #39
    div  r6, r6, #10       ; 3
    and  r6, r6, #0xFF     ; 3
    or   r6, r6, #0x100    ; 0x103
    lsr  r6, r6, #4        ; 16
    asr  r6, r6, #1        ; 8
    not  r6, r6            ; 0xFFFFFFF7
    st   r6, 0x324[r0]
    halt
