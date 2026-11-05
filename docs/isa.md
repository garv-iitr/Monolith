# **RISC201 Instruction Set Architecture (ISA) Specification**

# Opcodes

## **0x0\_ : Arithmetic & Logic Unit (ALU) Operations** 

| Mnemonic | Opcode (Hex) | Opcode (Bin) | Operation |
| :---- | :---- | :---- | :---- |
| **NOP** | `0x00` | `000000` | No operation |
| **ADD** | `0x01` | `000001` | `R[rd] = R[rs] + R[rt]` (or imm) |
| **SUB** | `0x02` | `000010` | `R[rd] = R[rs] - R[rt]` (or imm) |
| **MUL** | `0x03` | `000011` | `R[rd] = R[rs] * R[rt]` (or imm) |
| **DIV** | `0x04` | `000100` | `R[rd] = R[rs] / R[rt]` |
| **MOD** | `0x05` | `000101` | `R[rd] = R[rs] % R[rt]` |
| **AND** | `0x06` | `000110` | `R[rd] = R[rs] & R[rt]` |
| **OR** | `0x07` | `000111` | `R[rd] = R[rs] | R[rt]` |
| **NOT** | `0x08` | `001000` | `R[rd] = ~R[rs]` |
| **XOR** | `0x09` | `001001` | `R[rd] = R[rs] ^ R[rt]` (or imm) |
| **LSL** | `0x0A` | `001010` | `R[rd] = R[rs] << shift` |
| **LSR** | `0x0B` | `001011` | `R[rd] = R[rs] >> shift` (Logical) |
| **ASR** | `0x0C` | `001100` | `R[rd] = R[rs] >> shift` (Arithmetic) |
| **CMP** | `0x0D` | `001101` | Update FLAGS (Z, N, C, V) |

## **0x1\_ : Data Movement Operations**

| Mnemonic | Opcode (Hex) | Opcode (Bin) | Operation |
| :---- | :---- | :---- | :---- |
| **MOV** | `0x10` | `010000` | `R[rd] = R[rs]` or imm |
| **LD** | `0x11` | `010001` | `R[rt] = Mem[R[rs] + imm]` |
| **ST** | `0x12` | `010010` | `Mem[R[rs] + imm] = R[rt]` |

## 0x2\_ : Control Flow & System Operations

| Mnemonic | Opcode (Hex) | Opcode (Bin) | Operation |
| :---- | :---- | :---- | :---- |
| **B** | `0x20` | `100000` | `PC = target address` |
| **BEQ** | `0x21` | `100001` | If Z==1, `PC = target` |
| **BNE** | `0x22` | `100010` | If Z==0, `PC = target` |
| **BGT** | `0x23` | `100011` | If N==0 & Z==0, `PC = target` |
| **BLT** | `0x24` | `100100` | If N==1, `PC = target` |
| **CALL** | `0x25` | `100101` | Push return PC to the program and shadow stacks, `PC = target` |
| **RET** | `0x26` | `100110` | Verify popped PC against the shadow stack, then return |
| **BNDCHK** | `0x27` | `100111` | Verify stack/address bounds |
| **HALT** | `0x28` | `101000` | Exit current execution context |

## **0x3\_ : Floating-Point Unit (FPU) Operations**

| Mnemonic | Opcode (Hex) | Opcode (Bin) | Operation |
| :---- | :---- | :---- | :---- |
| **FADD** | `0x30` | `110000` | `$f0 = $f1 + $f2` |
| **FSUB** | `0x31` | `110001` | `$f0 = $f1 - $f2` |
| **FMUL** | `0x32` | `110010` | `$f0 = $f1 * $f2` |
| **FDIV** | `0x33` | `110011` | `$f0 = $f1 / $f2` |
| **FCMP** | `0x34` | `110100` | Compares `$rs` and `$rt` (FP rules), updates standard Z/N Flags |
| **FLD** | `0x35` | `110101` | Load word into FPU register |
| **FST** | `0x36` | `110110` | Store word from FPU register |
| **FMOV** | `0x37` | `110111` | Move floating-point register |

## Macros

PUSH   
POP   
SPUSH   
SPOP 

# **Instruction Formats** 

32-bit fixed width, 6-bit opcode in \[31:26\], 16 GPRs and 16 FPRs (4-bit fields). Mode: `00` \= register, `01` \= signed imm, `10` \= unsigned imm, `11` \= high (imm16 \<\< 16).

**Format 1: ALU three-operand (R/I)**

| Bits \[31:26\] | Bits \[25:24\] | Bits \[23:20\] | Bits \[19:16\] | Bits \[15:0\] | Form |
| :---: | :---: | :---: | :---: | :---: | ----- |
| `opcode` | `00` | `rd` | `rs1` | `rs2 [15:12] | unused=0 [11:0]` | register form |
| `opcode` | `mode` | `rd` | `rs1` | `imm16` | mode \= 01 / 10 / 11 |

**Instructions:** ADD, SUB, MUL, DIV, MOD, AND, OR, XOR, LSL, LSR, ASR

**Format 2: ALU two-operand, no rs1 (unary/move)**

| Bits \[31:26\] | Bits \[25:24\] | Bits \[23:20\] | Bits \[19:16\] | Bits \[15:0\] | Form |
| :---: | :---: | :---: | :---: | :---: | ----- |
| `opcode` | `00` | `rd` | `0000` | `rs2 [15:12] | unused=0 [11:0]` | register form |
| `opcode` | `mode` | `rd` | `0000` | `imm16` | immediate form |

**Instructions:** NOT, MOV

**Format 3: Compare, no rd**

| Bits \[31:26\] | Bits \[25:24\] | Bits \[23:20\] | Bits \[19:16\] | Bits \[15:0\] | Form |
| :---: | :---: | :---: | :---: | :---: | ----- |
| `opcode` | `00` | `0000` | `rs1` | `rs2 [15:12] | unused=0 [11:0]` | register form |
| `opcode` | `mode` | `0000` | `rs1` | `imm16` | immediate form |

**Instructions:** CMP

**Format 4: Memory (base \+ offset)**

| Bits \[31:26\] | Bits \[25:24\] | Bits \[23:20\] | Bits \[19:16\] | Bits \[15:0\] |
| :---: | :---: | :---: | :---: | :---: |
| `opcode` | `00` | `rd` | `rs1` | `imm16 (signed)` |

**Instructions:**

* LD `rd, imm[rs1]` and ST `rd, imm[rs1]` (`rd` is a GPR)  
* FLD `fd, imm[rs1]` and FST `fd, imm[rs1]` (`fd` is an FPR, base `rs1` is a GPR)  
* BNDCHK `rs1, #imm` (the `rd` field is 0000\)

**Format 5: Branch / call (offset)**

| Bits \[31:26\] | Bits \[25:0\] |
| :---: | :---: |
| `opcode` | `offset (26 bits, signed)` |

The offset is in words, PC-relative. **Instructions:** B, BEQ, BNE, BGT, BLT, CALL

**Format 6: No operands**

| Bits \[31:26\] | Bits \[25:0\] |
| :---: | :---: |
| `opcode` | `26 zero bits` |

**Instructions:** NOP, RET, HALT

CALL maintains a protected, simulator-internal copy of each return address.
RET compares the value read from the ordinary stack with the shadow-stack top
before transferring control or incrementing SP. A mismatch (including a RET
without a corresponding CALL) halts with a `shadow stack mismatch` runtime
fault. Stack bounds faults, such as RET at the empty stack, remain
`STACK_UNDERFLOW`. This is return-address integrity for backward edges; it is
not complete control-flow integrity and does not model Intel CET or ARM PAC.

**Format 7: FPU three-register**

| Bits \[31:26\] | Bits \[25:24\] | Bits \[23:20\] | Bits \[19:16\] | Bits \[15:12\] | Bits \[11:0\] |
| :---: | :---: | :---: | :---: | :---: | :---: |
| `opcode` | `00` | `fd` | `fs1` | `fs2` | `unused=0` |

**Instructions:** FADD, FSUB, FMUL, FDIV

**Format 8: FPU compare / move**

| Bits \[31:26\] | Bits \[25:24\] | Bits \[23:20\] | Bits \[19:16\] | Bits \[15:12\] | Bits \[11:0\] | Description |
| :---: | :---: | :---: | :---: | :---: | :---: | ----- |
| `FCMP` | `00` | `0000` | `fs1` | `fs2` | `unused=0` | compare, no destination |
| `FMOV` | `00` | `fd` | `0000` | `fs2` | `unused=0` | move, rs1 unused |

**Instructions:** FCMP, FMOV