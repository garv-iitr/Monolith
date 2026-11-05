# =============================================================================
# RISC201 test program 1: recursive Fibonacci
# =============================================================================
# fib(n) computed recursively with a full-descending stack (sp = r14):
#   fib(0) = 0, fib(1) = 1, fib(n) = fib(n-1) + fib(n-2)
#
# Registers: r1 = n (argument), r2 = result, r3 = scratch.
# Expected:   mem[0x10000] = 55  (= fib(10)),  sp restored to 0x20000.
#
# Opcodes used: mov, call, nop, st, halt, cmp, bgt, bne, b, ret, sub, ld, add
#
# Note on branches: the offset is PC-relative to the NEXT instruction, i.e.
#   target = (address of the branch + 4) + offset * 4
# The assembler resolves labels with this convention.
# =============================================================================

# n = 10
    mov  r1, 10
# r2 = fib(10)
    call fib
    nop
# store the result at data base
    li   r4, 0x10000
    st   r2, 0[r4]
    halt

fib:
# n <= 1 ?
    cmp  r1, 1
    bgt  recurse
# n == 0
    bne  base0
# n == 1 -> result 1
    mov  r2, 1
    b    baseret
base0:
# n == 0 -> result 0
    mov  r2, 0
baseret:
    ret

recurse:
# push n
    sub  sp, sp, 4
    st   r1, 0[sp]
# r1 = n - 1
    sub  r1, r1, 1
# r2 = fib(n-1)
    call fib
# push fib(n-1)
    sub  sp, sp, 4
    st   r2, 0[sp]
# restore n
    ld   r1, 4[sp]
# r1 = n - 2
    sub  r1, r1, 2
# r2 = fib(n-2)
    call fib
# r3 = fib(n-1)
    ld   r3, 0[sp]
# r2 = fib(n-2) + fib(n-1)
    add  r2, r2, r3
# pop both slots
    add  sp, sp, 8
    ret

