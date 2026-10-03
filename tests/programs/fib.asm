# =============================================================================
# RISC201 test program 1: recursive Fibonacci
# =============================================================================
# fib(n) computed recursively with a full-descending stack (sp = r14):
#   fib(0) = 0, fib(1) = 1, fib(n) = fib(n-1) + fib(n-2)
#
# Registers: r1 = n (argument), r2 = result, r3 = scratch.
# Expected:   mem[0x100] = 55  (= fib(10)),  sp restored to 0x300.
#
# Opcodes used: mov, call, nop, st, halt, cmp, bgt, bne, b, ret, sub, ld, add
#
# Note on branches: the offset is PC-relative to the NEXT instruction, i.e.
#   target = (address of the branch + 4) + offset * 4
# The assembler resolves labels with this convention.
# =============================================================================

    mov  sp, #0x300       ; set up the stack
    mov  r1, #10          ; n = 10
    call fib               ; r2 = fib(10)
    nop
    st   r2, 0x100[r0]     ; store the result
    halt

fib:
    cmp  r1, #1           ; n <= 1 ?
    bgt  recurse
    bne  base0            ; n == 0
    mov  r2, #1           ; n == 1 -> result 1
    b    baseret
base0:
    mov  r2, #0           ; n == 0 -> result 0
baseret:
    ret

recurse:
    sub  sp, sp, #4       ; push n
    st   r1, 0[sp]
    sub  r1, r1, #1       ; r1 = n - 1
    call fib               ; r2 = fib(n-1)
    sub  sp, sp, #4       ; push fib(n-1)
    st   r2, 0[sp]
    ld   r1, 4[sp]        ; restore n
    sub  r1, r1, #2       ; r1 = n - 2
    call fib               ; r2 = fib(n-2)
    ld   r3, 0[sp]        ; r3 = fib(n-1)
    add  r2, r2, r3       ; r2 = fib(n-2) + fib(n-1)
    add  sp, sp, #8       ; pop both slots
    ret
