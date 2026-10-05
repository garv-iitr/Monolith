# NOTES.md — RISC201 Assembler/Disassembler Design Notes

This file records every design assumption, ambiguity resolution, and
implementation note for the RISC201 assembler and disassembler.
Simulator teammates should read this carefully.

---

## 1. CALL / RET Semantics (Important for Simulator)

> **Note on Encoding vs. Execution:**
> This design choice affects only how a CPU simulator executes `CALL` and `RET` at runtime, not how the assembler encodes them. The assembler's encoding is unchanged: `CALL` is encoded as a Format 5 instruction (opcode 0x25, same layout as `B`), and `RET` is encoded as a Format 6 instruction with no operands (opcode 0x26).

### Option A: Hardware & Professor's Specification (Default)
- **CALL label**:
  - Pushes the return address (address of the `CALL` instruction + 4) onto the stack:
    `sp = sp - 4`, then `Mem[sp] = return address`.
  - Jumps to `label` (PC+4 relative 26-bit word offset, same as `B`).
  - The stack is full-descending with 4-byte words.
  - Register `r15` (`ra`) is just an ordinary register alias with no hardware role.
- **RET**:
  - Pops the return address from the stack into `PC`:
    `PC = Mem[sp]`, then `sp = sp + 4`.

### Option B: Sarangi / SimpleRISC Style (Alternative)
- **CALL label**:
  - Sets `ra` (r15) = address of the `CALL` instruction + 4.
  - Jumps to `label` (PC+4 relative 26-bit word offset).
  - There is no hardware stack push or pop.
- **RET**:
  - Sets `PC = ra` (r15).
  - No stack pop occurs.
- **Hardware Team Difference:**
  - Option B differs from the hardware team's implementation and the professor's specification, which both use the hardware stack (Option A).

---

## 2. Branch Offset Convention

Branch offset is **PC+4 relative**. This means:

```
offset = (target_byte_address − (branch_byte_address + 4)) / 4
```

A branch to the next instruction has offset = 0.
A branch to itself (infinite loop) has offset = -1.

This matches standard pipelined CPU architectures where PC has already moved to the next instruction (PC+4) when the branch executes.

---

## 3. `li` Pseudo-Instruction

`li rd, VALUE32` always expands to **exactly two instructions**:

```
mov rd, (VALUE32 >> 16)h    ; load high 16 bits (mode=11, high form)
or  rd, rd, (VALUE32 & 0xFFFF)u   ; OR in low 16 bits (mode=10, unsigned)
```

This two-instruction expansion is **unconditional** regardless of the value.
Pass-1 sizing is therefore fixed at 8 bytes (2 words) per `li`.

If `VALUE32` is a label, its address is resolved in pass 2 before splitting
into hi/lo halves. If the hi16 part is zero (i.e., label < 0x10000), the
`mov rd, 0x0000h` instruction still emits; the simulator can optimize.

---

## 4. Format4 Mode Bits

The memory instructions (LD, ST, FLD, FST, BNDCHK) always have bits
[25:24] = 00 in the encoding. The 16-bit offset is always treated as a
**signed** two's-complement value by the hardware. The assembler enforces
range -32768..32767 for the offset.

---

## 5. Immediate Notation (Sarangi Style)

| Syntax       | Mode | Bits [25:24] | Value range          |
|-------------|------|--------------|----------------------|
| `5` or `-3` | 01   | signed imm16 | -32768..32767        |
| `5u`        | 10   | unsigned imm16 | 0..65535           |
| `0x1234h`   | 11   | high form    | 0..65535 (raw 16-bit)|

The `h` suffix is always a high-form suffix (even when the base is `0x`).
For example, `0xABCDh` stores 0xABCD in bits [15:0] and the simulator
shifts it to bits [31:16] at execution time.

The `#` prefix on an immediate is optional and is silently stripped.

---

## 6. Section Defaults and CLI Overrides

- Default `.text` base: `0x00000000`
- Default `.data` base: `0x00010000`
- Override with `--text-base` and `--data-base` on the CLI.

If the assembler detects that text and data sections overlap (using the
final location counter values), it reports an error and produces no output.

---

## 7. Adding a New Instruction

1. Open `isa.cpp`.
2. Add ONE row to `kISATable[]`:
   ```cpp
   { "MYINSTR", 0x3E, FmtId::F1 },
   ```
3. If the new instruction uses an existing format (F1–F8), no other
   files need changing.
4. If a genuinely new encoding layout is needed:
   a. Add `FN` to the `FmtId` enum in `isa.hpp`.
   b. Create `FormatN` class declaration in `formats.hpp`.
   c. Implement all five virtual methods in `formats.cpp`.
   d. Add a `case FmtId::FN:` to `InstructionFormat::create()`.

---

## 8. Adding a New Macro

1. Open `macros.cpp`.
2. Add ONE row to `kMacroTable[]`:
   ```cpp
   { "MYMACRO", RegKind::GPR, 2,
     { "instr1 ...",
       "instr2 ...", nullptr, nullptr } },
   ```
3. Use `%0` in template lines to substitute the macro's register operand.
4. Max 4 expansion lines (increase `lines[]` array size in `MacroDef` if needed).

---

## 9. Disassembler Label Naming

The disassembler does NOT attempt to fold macro-expanded instruction
sequences back into macro names. It prints the raw instructions.

Synthetic labels for branch targets use the format `L_XXXXXXXX` where
`XXXXXXXX` is the 8-digit upper-case hex byte address of the target.
External targets (outside the image) still get labels with a comment.

---

## 10. Register Classes

- GPRs: r0..r15. Aliases: sp=r14, ra=r15 (case-insensitive).
- FPRs: f0..f15. No aliases.
- Wrong register class (GPR where FPR expected, or vice versa) is a
  hard error caught in pass 1 (macro operands) and pass 2 (instructions).

---

## 11. `.space` Directive

`.space N` reserves N bytes initialised to zero. If N is not a multiple
of 4, the section's location counter advances by N bytes exactly, which
may leave subsequent code/data misaligned. The assembler does NOT auto-pad.
Use `.org` to realign if needed. (Simplest interpretation chosen.)

---

## 12. LSL / LSR / ASR Shift Amount

The third operand of shift instructions is encoded like any other Format1
immediate. Following the specification ("warn if an immediate shift is
outside 0..31"), shift amounts outside 0..31 are allowed to encode into the
16-bit immediate field, allowing simulator/hardware testing of edge cases.

---

## 13. `.org` Padding and Word Alignment

`.org addr` requires `addr` to be word-aligned (divisible by 4) and non-negative.
In addition:
- `.org` cannot move the location counter backwards within a section.
- When `.org` advances the location counter, the assembler pads the intervening
  gap with 32-bit zero words so that instructions and branch targets land at the
  exact memory addresses specified in the assembly code.

---

## 14. Format5 Target Must Be Word-Aligned

The assembler verifies that the branch target address is divisible by 4.
A misaligned target is an error.

---

## 15. Output File Naming

| Flag        | .text file | .data file (if non-empty)    |
|-------------|------------|------------------------------|
| `--format hex` | `<stem>.hex` | `<stem>_data.hex`        |
| `--format bin` | `<stem>.bin` | `<stem>_data.bin`        |
| `--format raw` | `<stem>.raw` | `<stem>_data.raw`        |

---

## 16. Two-Pass Algorithm Overview

**Pass 1:**
- Lines are tokenised and comment-stripped.
- Leading `label:` is extracted and recorded in the SymbolTable.
- `.equ` constants are evaluated immediately (value must be a literal or
  an earlier `.equ`).
- Macros and `li` are expanded to their constituent instruction records.
- The location counter advances 4 bytes per instruction (including each
  macro-expanded or `li`-expanded instruction), plus directive sizes.
- Mnemonic existence and operand count are validated.

**Pass 2:**
- The expanded instruction list is walked in order.
- `.equ` names in operand tokens are replaced by their numeric values.
- Branch labels are resolved to word offsets; range and alignment are checked.
- `li` high/low halves are computed from the resolved 32-bit value.
- Every instruction is encoded via its `Format::parseOperands()` +
  `Format::encode()` methods.
- All errors are collected (no early exit); if any errors exist, no output
  files are written and the process exits non-zero.

---

## 17. Header Whitelist Compliance

To strictly satisfy the hard constraint of allowed headers:
`<iostream> <fstream> <sstream> <iomanip> <string> <vector> <map> <cstdint> <cstdlib> <stdexcept> <algorithm> <cctype> <memory>`
`<functional>` is NOT used anywhere in the codebase. Branch label resolution in the
disassembler is implemented via a lightweight abstract interface `ILabelResolver`
defined in `formats.hpp`.

---

## 18. Disassembler Unsigned Immediate Fidelity

Unsigned immediates (`mode == 2`, suffix `u`) and high-form immediates (`mode == 3`,
suffix `h`) in Format 1, 2, and 3 instructions preserve their raw unsigned 16-bit
value during disassembly. They are NOT sign-extended to negative numbers, ensuring
exact round-trip reassembly fidelity (e.g. `50000u` disassembles as `50000u` rather
than `-15536u`).

---

## 19. BNDCHK Reserved Bits Validation

The specification specifies that for BNDCHK (`opcode = 0x27`), the `rd` field
(bits [23:20]) must be `0000`. The disassembler verifies that `rd == 0`, and treats
any instruction with opcode 0x27 and `rd != 0` as `.word 0xXXXXXXXX ; invalid encoding`.

---

## 20. `.equ` in Memory Operands and Unary Negation

The assembler supports `.equ` constants in all immediate contexts:
- Memory operands: `ld r1, MY_CONST[r2]`, `st r1, -MY_CONST[sp]`
- Unary negation: `mov r1, -MY_CONST`, `bndchk sp, -DELTA`
- Data directives: `.word MY_CONST, -MY_CONST`

---
*End of NOTES.md*
