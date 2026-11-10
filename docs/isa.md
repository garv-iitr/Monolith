# RISC201 Instruction Set Architecture (ISA) Specification

**Course:** CSC-201 Computer Organization & Architecture  
**Architecture:** 32-bit Fixed-Length Load-Store RISC  
**Word Length:** 32 bits (4 bytes), byte-addressed, 4-byte word-aligned  
**Source Implementation:** [`isa_spec/isa.h`](file:///e:/CSC-201/Monolith/isa_spec/isa.h), [`assembler_disassembler/asm.cpp`](file:///e:/CSC-201/Monolith/assembler_disassembler/asm.cpp), [`assembler_disassembler/disasm.cpp`](file:///e:/CSC-201/Monolith/assembler_disassembler/disasm.cpp)  

---

## 1. Architectural Registers & State

The RISC201 architecture defines two distinct, isolated 16-register register files, condition flags, and a program counter:

```mermaid
graph TD
    subgraph GPR_Bank [" Integer Register File (16 x 32-bit GPRs) "]
        R0["r0: Constant 0 (Writes Discarded)"]
        R_GEN["r1 - r12: General-Purpose Operands & Scratch"]
        R13["r13: Macro Swap Temporary Register"]
        R14["r14 (sp): Hardware Stack Pointer (Initial: 0x20000)"]
        R15["r15 (ra): Subroutine Link / Return Address"]
    end

    subgraph FPR_Bank [" Floating-Point Register File (16 x 32-bit FPRs) "]
        F_GEN["f0 - f15: IEEE-754 Single-Precision Floating-Point Operands"]
    end

    subgraph FlagsBank [" Condition Code Flags (NZCV) "]
        FLAG_Z["Z: Zero"]
        FLAG_N["N: Negative"]
        FLAG_C["C: Carry / Borrow"]
        FLAG_V["V: Signed Overflow"]
    end
```

### Architectural Domain Isolation
To maintain clean hardware isolation between integer logic and floating-point arithmetic datapaths, direct register-to-register transfers between GPRs and FPRs are intentionally omitted from the instruction set. 

Data transfers between domains are explicitly conducted through memory or the stack using store and load pairs (e.g., storing an integer via `ST` followed by loading a float via `FLD`, or storing a float via `FST` followed by loading an integer via `LD`).

---

## 2. Canonical 34-Opcode Table

The opcode is encoded in the upper 6 bits `[31:26]` of every 32-bit instruction word:

### Class 0x0_: Arithmetic & Logic Unit (ALU) Operations
| Mnemonic | Opcode (Hex) | Opcode (Bin) | Operation | Flag Updates | Description |
| :--- | :---: | :---: | :--- | :---: | :--- |
| **NOP** | `0x00` | `000000` | None | None | No operation (bubble / pipeline delay) |
| **ADD** | `0x01` | `000001` | $R[rd] = R[rs1] + \text{Operand2}$ | ALU internal | 32-bit Kogge-Stone integer addition |
| **SUB** | `0x02` | `000010` | $R[rd] = R[rs1] - \text{Operand2}$ | ALU internal | 32-bit two's complement subtraction |
| **MUL** | `0x03` | `000011` | $R[rd] = R[rs1] \times \text{Operand2}$ | ALU internal | Radix-4 Booth signed multiplication (low 32b) |
| **DIV** | `0x04` | `000100` | $R[rd] = R[rs1] / \text{Operand2}$ | ALU internal | Signed non-restoring integer division |
| **MOD** | `0x05` | `000101` | $R[rd] = R[rs1] \pmod{\text{Operand2}}$ | ALU internal | Integer modulo / remainder |
| **AND** | `0x06` | `000110` | $R[rd] = R[rs1] \ \& \ \text{Operand2}$ | ALU internal | Bitwise logical AND |
| **OR**  | `0x07` | `000111` | $R[rd] = R[rs1] \ \| \ \text{Operand2}$ | ALU internal | Bitwise logical OR |
| **NOT** | `0x08` | `001000` | $R[rd] = \sim R[rs2]$ | ALU internal | Bitwise logical bit inversion (unary) |
| **XOR** | `0x09` | `001001` | $R[rd] = R[rs1] \oplus \text{Operand2}$ | ALU internal | Bitwise logical exclusive OR |
| **LSL** | `0x0A` | `001010` | $R[rd] = R[rs1] \ll \text{shift}$ | ALU internal | Logical Shift Left (zero-fill low bits) |
| **LSR** | `0x0B` | `001011` | $R[rd] = R[rs1] \gg \text{shift}$ | ALU internal | Logical Shift Right (zero-fill high bits) |
| **ASR** | `0x0C` | `001100` | $R[rd] = R[rs1] \ggg \text{shift}$ | ALU internal | Arithmetic Shift Right (sign-extended) |
| **CMP** | `0x0D` | `001101` | Discard $(R[rs1] - \text{Operand2})$ | `Z, N, C, V` (Committed) | Compare: updates architectural flags without writing `rd` |

> [!NOTE]
> **Architectural Condition Flag Gating:** While the 32-bit ALU and FPU compute condition status flags for all operations, architectural flags in the CPU status register are updated only when the Control Store asserts `set_flags = 1`. In RISC201, `set_flags` is specifically asserted for compare instructions (`CMP` and `FCMP`). This microarchitectural design preserves flags set by comparisons against corruption by intervening arithmetic or pointer manipulation prior to conditional branching.

### Class 0x1_: Data Movement Operations
| Mnemonic | Opcode (Hex) | Opcode (Bin) | Operation | Flag Updates | Description |
| :--- | :---: | :---: | :--- | :---: | :--- |
| **MOV** | `0x10` | `010000` | $R[rd] = \text{Operand2}$ | None | Register-to-register or immediate load |
| **LD**  | `0x11` | `010001` | $R[rd] = \text{Mem}[R[rs1] + \text{imm16}]$ | None | Load 32-bit word from memory into GPR |
| **ST**  | `0x12` | `010010` | $\text{Mem}[R[rs1] + \text{imm16}] = R[rd]$ | None | Store 32-bit word from GPR into memory |

### Class 0x2_: Control Flow & System Operations
| Mnemonic | Opcode (Hex) | Opcode (Bin) | Condition / Target | Description |
| :--- | :---: | :---: | :--- | :--- |
| **B**      | `0x20` | `100000` | Unconditional ($PC \leftarrow PC + \text{offset} \times 4$) | Unconditional PC-relative branch |
| **BEQ**    | `0x21` | `100001` | If $Z == 1$, branch | Branch if Equal |
| **BNE**    | `0x22` | `100010` | If $Z == 0$, branch | Branch if Not Equal |
| **BGT**    | `0x23` | `100011` | If $(N == V) \ \& \ (Z == 0)$, branch | Branch if Greater Than (signed) |
| **BLT**    | `0x24` | `100100` | If $(N \neq V)$, branch | Branch if Less Than (signed) |
| **CALL**   | `0x25` | `100101` | Push $PC + 4$ to stack; $PC \leftarrow \text{target}$ | Subroutine call with shadow stack mirror |
| **RET**    | `0x26` | `100110` | Pop return PC; verify vs. shadow stack | Subroutine return with integrity check |
| **BNDCHK** | `0x27` | `100111` | Check $R[rs1] + \text{imm16}$ vs. StackGuard | Explicit memory boundary validation |
| **HALT**   | `0x28` | `101000` | Stop clock generator | Halts execution pipeline |

### Class 0x3_: Floating-Point Unit (FPU) Operations
| Mnemonic | Opcode (Hex) | Opcode (Bin) | Operation | Flag Updates | Description |
| :--- | :---: | :---: | :--- | :---: | :--- |
| **FADD** | `0x30` | `110000` | $F[fd] = F[fs1] + F[fs2]$ | FPU internal | IEEE-754 single-precision float addition |
| **FSUB** | `0x31` | `110001` | $F[fd] = F[fs1] - F[fs2]$ | FPU internal | IEEE-754 single-precision float subtraction |
| **FMUL** | `0x32` | `110010` | $F[fd] = F[fs1] \times F[fs2]$ | FPU internal | IEEE-754 single-precision float multiplication |
| **FDIV** | `0x33` | `110011` | $F[fd] = F[fs1] / F[fs2]$ | FPU internal | IEEE-754 single-precision float division |
| **FCMP** | `0x34` | `110100` | Discard $(F[fs1] - F[fs2])$ | `Z, N` (Committed) | Float compare: updates condition flags |
| **FLD**  | `0x35` | `110101` | $F[fd] = \text{Mem}[R[rs1] + \text{imm16}]$ | None | Load 32-bit float from memory into FPR |
| **FST**  | `0x36` | `110110` | $\text{Mem}[R[rs1] + \text{imm16}] = F[fd]$ | None | Store 32-bit float from FPR into memory |
| **FMOV** | `0x37` | `110111` | $F[fd] = F[fs2]$ | None | Floating-point register-to-register move |

---

## 3. Instruction Encoding Formats

Every instruction is exactly 32 bits wide. Bits `[31:26]` hold the 6-bit opcode. Bits `[25:24]` specify the **Operand Mode**:
- `00`: Register Mode (source operand is a register `rs2` or `fs2`).
- `01`: Signed Immediate Mode (16-bit signed integer sign-extended to 32 bits).
- `10`: Unsigned Immediate Mode (16-bit unsigned integer zero-extended to 32 bits).
- `11`: High Immediate Mode (16-bit immediate shifted left by 16 bits: `imm16 << 16`).

### Format 1: ALU Three-Operand (R/I)
Used by: `ADD`, `SUB`, `MUL`, `DIV`, `MOD`, `AND`, `OR`, `XOR`, `LSL`, `LSR`, `ASR`
```
 31        26 25 24 23    20 19    16 15        12 11             0
+------------+-----+--------+--------+------------+----------------+
|   Opcode   | Mod |   rd   |  rs1   | rs2 [15:12]|    Unused (0)  |  (Mode = 00)
+------------+-----+--------+--------+------------+----------------+
|   Opcode   | Mod |   rd   |  rs1   |          imm16          |  (Mode = 01/10/11)
+------------+-----+--------+--------+-----------------------------+
```

### Format 2: ALU Two-Operand / Unary / Move
Used by: `NOT`, `MOV`
```
 31        26 25 24 23    20 19    16 15        12 11             0
+------------+-----+--------+--------+------------+----------------+
|   Opcode   | Mod |   rd   |  0000  | rs2 [15:12]|    Unused (0)  |  (Mode = 00)
+------------+-----+--------+--------+------------+----------------+
|   Opcode   | Mod |   rd   |  0000  |          imm16          |  (Mode = 01/10/11)
+------------+-----+--------+--------+-----------------------------+
```

### Format 3: Compare (No Destination Register)
Used by: `CMP`
```
 31        26 25 24 23    20 19    16 15        12 11             0
+------------+-----+--------+--------+------------+----------------+
|   Opcode   | Mod |  0000  |  rs1   | rs2 [15:12]|    Unused (0)  |  (Mode = 00)
+------------+-----+--------+--------+------------+----------------+
|   Opcode   | Mod |  0000  |  rs1   |          imm16          |  (Mode = 01/10/11)
+------------+-----+--------+--------+-----------------------------+
```

### Format 4: Memory Base + Offset
Used by: `LD`, `ST`, `FLD`, `FST`, `BNDCHK`
```
 31        26 25 24 23    20 19    16 15                          0
+------------+-----+--------+--------+-----------------------------+
|   Opcode   | 00  | rd/fd  |  rs1   |        imm16 (Signed)       |
+------------+-----+--------+--------+-----------------------------+
```
- For `LD`/`ST`, `rd` is a GPR (`r0`–`r15`). Base `rs1` is a GPR.
- For `FLD`/`FST`, `fd` is an FPR (`f0`–`f15`). Base `rs1` is a GPR.
- For `BNDCHK`, the `rd` field is `0000`. Base `rs1` is a GPR.

### Format 5: Branch / Subroutine Call (PC-Relative)
Used by: `B`, `BEQ`, `BNE`, `BGT`, `BLT`, `CALL`
```
 31        26 25                                                  0
+------------+-----------------------------------------------------+
|   Opcode   |               offset26 (Signed Word Offset)         |
+------------+-----------------------------------------------------+
```
Target address calculation:
$$\text{Target PC} = (\text{Instruction PC} + 4) + (\text{offset26} \times 4)$$
*(In the pipelined datapath, the fetch PC has advanced by $+4$ when the branch word is decoded in the `OF` stage, making branches relative to sequential PC $+4$.)*

### Format 6: Zero-Operand Instructions
Used by: `NOP`, `RET`, `HALT`
```
 31        26 25                                                  0
+------------+-----------------------------------------------------+
|   Opcode   |                     26 Zero Bits                    |
+------------+-----------------------------------------------------+
```

### Format 7: FPU Three-Register Arithmetic
Used by: `FADD`, `FSUB`, `FMUL`, `FDIV`
```
 31        26 25 24 23    20 19    16 15    12 11                 0
+------------+-----+--------+--------+--------+--------------------+
|   Opcode   | 00  |   fd   |  fs1   |  fs2   |      Unused (0)    |
+------------+-----+--------+--------+--------+--------------------+
```

### Format 8: FPU Compare & Move
Used by: `FCMP`, `FMOV`
```
 31        26 25 24 23    20 19    16 15    12 11                 0
+------------+-----+--------+--------+--------+--------------------+
|   FCMP     | 00  |  0000  |  fs1   |  fs2   |      Unused (0)    |
+------------+-----+--------+--------+--------+--------------------+
|   FMOV     | 00  |   fd   |  0000  |  fs2   |      Unused (0)    |
+------------+-----+--------+--------+--------+--------------------+
```

---

## 4. Stack Preprocessor Macro Expansions

The RISC201 assembler (`risc201asm`) includes built-in macro preprocessor expansion to support standard full-descending stack conventions (`sp = r14`, growing downward from `0x20000`):

### Integer Stack Macros
1. **Standard Push (`PUSH rx`):**
   ```armasm
   subi sp, sp, 4       ; Decrement stack pointer (4 bytes)
   st   rx, 0[sp]       ; Store word to top of stack
   ```
2. **Standard Pop (`POP rx`):**
   ```armasm
   ld   rx, 0[sp]       ; Load word from top of stack
   addi sp, sp, 4       ; Increment stack pointer (4 bytes)
   ```
3. **Safe Guarded Push (`SPUSH rx`):**
   ```armasm
   bndchk sp, -4        ; Hardware StackGuard overflow preflight check
   subi   sp, sp, 4     ; Decrement stack pointer
   st     rx, 0[sp]     ; Store word to top of stack
   ```
4. **Safe Guarded Pop (`SPOP rx`):**
   ```armasm
   bndchk sp, 4         ; Hardware StackGuard underflow preflight check
   ld     rx, 0[sp]     ; Load word from top of stack
   addi   sp, sp, 4     ; Increment stack pointer
   ```

### Floating-Point Stack Macros
5. **Floating-Point Push (`FPUSH fx`):**
   ```armasm
   subi sp, sp, 4       ; Decrement stack pointer (4 bytes)
   fst  fx, 0[sp]       ; Store float to top of stack
   ```
6. **Floating-Point Pop (`FPOP fx`):**
   ```armasm
   fld  fx, 0[sp]       ; Load float from top of stack
   addi sp, sp, 4       ; Increment stack pointer (4 bytes)
   ```
7. **Guarded Floating-Point Push (`SFPUSH fx`):**
   ```armasm
   bndchk sp, -4        ; Hardware StackGuard preflight check
   subi   sp, sp, 4     ; Decrement stack pointer
   fst    fx, 0[sp]     ; Store float to top of stack
   ```
8. **Guarded Floating-Point Pop (`SFPOP fx`):**
   ```armasm
   bndchk sp, 4         ; Hardware StackGuard underflow check
   fld    fx, 0[sp]     ; Load float from top of stack
   addi   sp, sp, 4     ; Increment stack pointer
   ```

### Constant Loading Macro
9. **32-bit Immediate Load (`LI rx, imm32`):**
   Expands to high-half load and low-half bitwise OR:
   ```armasm
   movh rx, (imm32 >> 16) & 0xFFFF ; High immediate (Mode 3)
   oru  rx, rx, (imm32 & 0xFFFF)   ; Unsigned low immediate (Mode 2)
   ```
