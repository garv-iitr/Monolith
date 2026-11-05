# RISC201 memory map

The simulator uses byte addresses and 4-byte words in a 128 KiB address space:

| Range | Use |
| --- | --- |
| `0x0000`–`0x0FFF` | Reserved low memory (including program images) |
| `0x1000`–`0x1EFFF` | Data (the CLI/assembler data image starts at `0x10000`) |
| `0x1F000`–`0x1FFFF` | Stack, growing down |

`0x20000` is the exclusive top of memory. It is a valid initial stack pointer,
but not a valid address to read or write. The first push from that pointer
stores at `0x1FFFC`. The last physical word is also at `0x1FFFC`.

`MemoryUnit` defaults to all 32,768 words and still supports explicitly smaller
memories. It provides physical storage bounds; low memory remains accessible
for instruction fetch and image loading. `StackGuard` bounds checked data
ranges below the stack and validates stack-pointer alignment and push/pop
boundaries. It rejects ranges that cross the data/stack boundary and uses
subtraction-based checks to avoid address addition overflow.

The assembler/CLI convention places `.data` at `0x10000`; that range is inside
the data region and below the reserved top-of-memory stack region. The
control-unit assemblers encode microcode and are separate from the program
assembler.

See [`docs/memory-safety-evaluation.md`](../docs/memory-safety-evaluation.md)
for the measured CPU configuration matrix, presentation demo commands, and
the bounded per-instruction `stack trace` mode.
