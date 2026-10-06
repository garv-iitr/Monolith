main:
    mov  sp, 0x300       ; set up the stack (optional here, but good practice)
    mov  r1, 12          ; n = 12
    call factorial       ; r2 = factorial(12)
    st   r2, 0x100[r0]   ; store the result in memory at address 0x100
    halt

factorial:
    mov  r2, 1           ; r2 will hold the result, initialize to 1

fact_loop:
    cmp  r1, 1           ; compare n to 1
    blt  fact_done       ; if n < 1 (e.g. n=0), we're done (0! = 1)
    
    mul  r2, r2, r1      ; result = result * n
    sub  r1, r1, 1       ; n = n - 1
    b    fact_loop       ; repeat loop

fact_done:
    ret                  ; return to caller, result in r2
