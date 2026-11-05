# RISC201 memory-safety evaluation

## Verified implementation and measurements

The simulator uses byte addresses and 4-byte words in a 128 KiB (131,072-byte)
address space: 32,768 words total. The checked stack occupies
`0x1F000`–`0x1FFFF` (4,096 bytes / 1,024 words) and grows down from the
exclusive initial SP `0x20000`; the first push writes at `0x1FFFC`.
The CLI/assembler data image starts at `0x10000`. This reconciles the map
mismatch called out in the original integration note: the CPU memory and
StackGuard use the same 128 KiB address space, so `.data` at `0x10000` is
valid and does not overlap the reserved high stack range.

During CPU instruction execution, runtime datapath checks validate memory
addresses/alignment before checked reads and writes, and check stack operations
and SP changes. A detected fault records a fault code/address and halts
execution. CALL push checks run in both pipeline layouts; the 4-stage CALL
preflight occurs before the SP update, and the matrix verifies that an overflow
leaves SP and the target stack word unchanged.

The CPU also maintains a protected, internal shadow return stack. Each
successful CALL mirrors its saved return PC there; RET compares the ordinary
stack's return value against the shadow top before changing SP or transferring
control. A mismatch halts with `SHADOW_STACK_MISMATCH` and reports the stack
slot address. Shadow state is reset with the CPU and included in checkpoints,
so debugger reverse execution restores it along with the datapath and memory.
This protects return addresses (backward-edge integrity) only; it is not
complete CFI, and it is not an implementation equivalent to Intel CET or ARM
PAC.

The integrated CPU regression matrix exercises 17 cases under each of
4-stage horizontal, 4-stage vertical, 6-stage horizontal, and 6-stage vertical
execution: successful CALL/RET; CALL overflow; empty-stack RET underflow;
valid data-region ST/LD; store below current SP; misaligned stack load;
invalid and misaligned integer LD/ST; and invalid and misaligned FP FLD/FST.
It also tests corrupted and unmatched return addresses and nested CALL/RET.
Invalid stores verify the target word was not modified; the valid data
round-trip verifies both value preservation and that an adjacent stack word
is unchanged. The updated matrix passed `68/68` cases. These are executable
functional regression results, not
performance benchmarks or an exhaustive proof of memory safety.

The FP cases verify memory-address fault detection and, for FST, absence of a
memory write on fault; they do not assert floating-register contents. The
matrix establishes representative invalid-access and stack-exception
detection across those opcodes and configurations, including misaligned
addresses within the stack window. It does not establish pointer-provenance
enforcement, data-flow isolation, exhaustive coverage of all memory-capable
instruction paths, or fuzz-tested behavior.

## Presentation demo

From the repository root, build and run the test suite, then inspect the live
stack on demand:

```sh
cmake --build build
ctest --test-dir build --output-on-failure
printf 'stack 5\nquit\n' | ./build/risc201-sim
```

For an interactive visualizer, run `./build/risc201-sim`, execute a program
containing a CALL, then enter `stack 5` (or `stackviz 5`). The display reports
the SP, stack words, call-frame markers when present, and LIMIT/BASE bounds.

To render each instruction of a short program with a stack-only before/after
diff, create a CALL/RET example and use the bounded trace command:

```sh
printf '94000001\nA0000000\n98000000\n' > /tmp/risc201-call.hex
printf 'load /tmp/risc201-call.hex\nstack trace 8 5\nquit\n' | ./build/risc201-sim
```

`stack trace [steps] [window]` traces the currently loaded program, with
defaults of 64 instructions and 8 displayed words (maximums: 1,000 and 64).
Each instruction prints changed stack words, SP movement labeled push/pop,
then the current stack and live CALL FRAME markers. This is opt-in; ordinary
`step`/`run` output is unchanged. A trace that reaches its step cap leaves the
CPU runnable.

To demonstrate a deliberate runtime fault, write this two-word program to
`/tmp/risc201-bad-address.hex`:

```text
41100000
44210000
```

The first instruction sets `r1` to zero; the second loads through `r1`.
Then run:

```sh
printf 'load /tmp/risc201-bad-address.hex\nrun\nquit\n' | ./build/risc201-sim
```

The observed result is a halted CPU with `FAULT: bad memory access @ 0x0`.
The test matrix also verifies stack overflow/underflow and misaligned-access
faults by checking their fault codes and addresses.

## Remaining work

The on-demand and opt-in instruction-trace modes are verified. The trace
reports word-level stack changes and the runtime's CALL/RET boundaries; it is
not a timed animation, and it does not decode function names, source-level
frames, or local variables. Broader memory-safety coverage beyond the listed
regression matrix remains future work.
