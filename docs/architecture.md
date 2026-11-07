# RISC201 Microarchitecture & Hardware Specification

**Course:** CSC-201 Computer Organization & Architecture  
**Architecture:** 32-bit Load-Store RISC with Microprogrammed Control Store  
**Pipelines:** Synchronous 4-Stage and 6-Stage Datapaths with Dual Control ROMs  

---

## 1. System Overview & Architectural State

The RISC201 is a cycle-accurate 32-bit microprocessor designed for high instruction throughput, predictable memory safety, and programmable microcode control. It implements a strict load-store architecture: all computational operations (arithmetic, logic, shifts, floating-point math) operate exclusively on registers, while external memory is accessed solely through dedicated load (`LD`, `FLD`) and store (`ST`, `FST`) instructions.

```mermaid
graph TD
    subgraph RegState [" Architectural Register State "]
        GPR["16 General-Purpose Registers (32-bit)<br/>r0 (zero), r1-r12 (scratch), r13 (swap temp),<br/>r14 (sp / stack pointer), r15 (ra / link register)"]
        FPR["16 Floating-Point Registers (32-bit)<br/>f0 - f15 (IEEE-754 Single Precision)"]
        FLAGS["Condition Code Flags (NZCV)<br/>Zero (Z), Negative (N), Carry (C), Overflow (V)"]
        PC["Program Counter (32-bit)<br/>Word-aligned instruction pointer"]
    end

    subgraph MemoryState [" Memory Hierarchy (128 KiB / 32,768 Words) "]
        TEXT["Code / Reserved Low Memory: 0x00000 - 0x00FFF (4 KiB)"]
        DATA["Accessible Data Region: 0x01000 - 0x1EFFF (120 KiB, Static Data at 0x10000)"]
        STACK["Guarded Stack Window: 0x1F000 - 0x20000 (4 KiB Full-Descending, SP=0x20000)"]
    end
```

### Architectural State Elements
1. **General-Purpose Register File (GPR):**
   - 16 registers (`r0`–`r15`), each 32 bits wide.
   - `r0`: Hardwired to constant zero (`0x00000000`). Writes to `r0` are silently discarded.
   - `r1`–`r12`: General-purpose operand, scratch, and function parameter registers.
   - `r13`: Reserved macro swap temporary register used by assembler expansions.
   - `r14` (`sp`): Hardware stack pointer for the full-descending stack. Initialized to `0x20000`.
   - `r15` (`ra`): Subroutine return address link register populated by `CALL` and consumed by `RET`.
2. **Floating-Point Register File (FPR):**
   - 16 registers (`f0`–`f15`), each 32 bits wide, storing IEEE-754 single-precision floating-point numbers.
   - Distinct and isolated from the integer register file; transfers between integer and FP domains are conducted explicitly through memory or stack operations (`ST`/`FLD` or `FST`/`LD`).
3. **Status & Condition Flags:**
   - **`Z` (Zero):** Set if ALU or FPU result is zero.
   - **`N` (Negative):** Set if the sign bit (bit 31) of the result is 1.
   - **`C` (Carry / Borrow):** Set on unsigned carry out from addition or borrow during subtraction.
   - **`V` (Signed Overflow):** Set when signed addition or subtraction produces an overflow into the sign bit ($A_{\text{sign}} == B_{\text{sign}} \neq \text{Res}_{\text{sign}}$).
   - *Architectural Gating:* While the 32-bit ALU and FPU compute condition flags for all operations, flags are only committed to the condition register when `set_flags = 1`, which the Control Store ROM explicitly asserts for compare instructions (`CMP`, `FCMP`). This preserves condition flags across intermediate calculations prior to branch evaluation.
4. **Program Counter (`PC`):**
   - 32-bit byte address pointing to the instruction memory word. Instructions are 32 bits (4 bytes) wide and must be 4-byte aligned (`PC % 4 == 0`).

---

## 2. Microarchitectural Evolution: Single-Bus vs. Pipelined

### A. The Baseline Single-Bus Architecture (`legacy/`)
In the initial project milestone, the team implemented a classic single-bus microprogrammed datapath (`legacy/datapath/datapath.cpp`, `legacy/common/shared_bus.h`, `legacy/common/internal_registers.h`) following the canonical design described in computer organization textbooks (e.g. Sarangi Figure 9.22).

In this single-bus model:
- All functional units and registers (`PC`, `IR`, `MAR`, `MDR`, `A`, `B`, `aluResult`, and the register file) were connected to a **single shared 32-bit internal bus**.
- Only one register could drive the bus in any given clock cycle, and only one destination could latch data from it.
- As a consequence, every macro-instruction required a long sequence of micro-operations:
  1. `PC -> MAR`, `MemRead`, `MDR -> Bus -> IR`, `PC = PC + 4` (Fetch: 4 cycles)
  2. `IR -> RegFile -> A`, `IR -> RegFile -> B` (Decode: 2 cycles)
  3. `A, B -> ALU -> aluResult` (Execute: 1 cycle)
  4. `aluResult -> MAR`, `Bus -> MDR -> DataMem` (Memory: 2 cycles)
  5. `MDR / aluResult -> Bus -> RegFile` (Writeback: 1 cycle)
- **Bottleneck:** Every instruction required 10 to 11 clock cycles. The average CPI across standard benchmarks was **9.8 to 10.4**, severely limiting throughput.

### B. The Production Synchronous Pipelined Architecture
To satisfy the course requirement for high-performance pipelined processing, the team developed [`control_unit/pipeline_processor`](file:///e:/CSC-201/Monolith/control_unit/pipeline_processor.h). The shared bus was eliminated in favor of **dedicated, point-to-point inter-stage pipeline registers** (`IfOfLatch`, `OfExLatch`, `ExStageLatch`, `ChkMaLatch`, `MaRwLatch`). 

By allowing multiple instructions to execute concurrently across different hardware stages, the processor starts a new instruction every clock cycle in the absence of hazards, achieving a sustained CPI of **1.35 to 2.17** and delivering a **4.8x to 7.3x speedup** over the single-bus baseline.

```mermaid
graph LR
    subgraph SingleBus [" Legacy Single-Bus Baseline (CPI ~ 10.0) "]
        BUS["Shared 32-bit Internal Data Bus"]
        MAR["MAR"] <--> BUS
        MDR["MDR"] <--> BUS
        PC_L["PC"] <--> BUS
        IR_L["IR"] <--> BUS
        ALU_L["ALU"] <--> BUS
        RF_L["RegFile"] <--> BUS
    end

    subgraph PipelinedDatapath [" Production Pipelined Architecture (CPI ~ 1.4 - 2.1) "]
        IF_P["[IF] Fetch"] -->|"IfOfLatch"| OF_P["[OF] Decode"]
        OF_P -->|"OfExLatch"| EX_P["[EX] Execute"]
        EX_P -->|"ExStageLatch"| MA_P["[MA] Memory"]
        MA_P -->|"MaRwLatch"| RW_P["[RW] Writeback"]
    end
```

---

## 3. Pipeline Configurations: 4-Stage vs. 6-Stage

The processor supports two runtime-selectable pipeline configurations (`--stages 4` and `--stages 6`):

### 1. 4-Stage Datapath (`[IF] -> [OF] -> [EX] -> [MA/RW]`)
Designed for low instruction latency and compact control logic:
- **Fetch (`IF`):** Fetches 32-bit instruction from memory at `PC`; computes sequential target `PC + 4`.
- **Decode & Operand Fetch (`OF`):** Decodes instruction word; accesses Control Store ROM; reads source registers (`rs1`, `rs2` or `fs1`, `fs2`); extracts immediate values; checks load-use hazard interlocks.
- **Execute (`EX`):** Integer ALU or IEEE-754 FPU operation; effective address computation (`base + offset`); branch condition evaluation; multi-level RAW data forwarding.
- **Memory Access & Writeback (`MA/RW`):** Combined stage that performs data memory access (`LD`, `ST`, `FLD`, `FST`), executes inline `StackGuard` checks, and writes back computational results to destination registers (`rd` or `fd`).
- **Hazard Characteristics:**
  - **Branch Penalty:** 2 cycles (evaluated in `EX`; flushes `IF/OF` and `OF/EX`).
  - **Load-Use Stall:** 1 bubble cycle (data available at the end of `MA/RW` and forwarded immediately).

### 2. 6-Stage Datapath (`[IF] -> [OF] -> [EX] -> [MEMCHK] -> [MA] -> [RW]`)
Designed for higher operating clock frequencies and hardware-isolated memory safety:
- **Fetch (`IF`):** Instruction memory read and `PC` increment.
- **Decode (`OF`):** Instruction decode, micro-sequencer dispatch, and register read.
- **Execute (`EX`):** ALU and FPU execution; branch condition resolution; effective memory address calculation.
- **Memory Check (`MEMCHK`):** Dedicated hardware firewall stage validating the effective address against `StackGuard` bounds, alignment rules (`addr % 4 == 0`), and segment permissions *before* memory bus strobes are asserted.
- **Memory Access (`MA`):** Physical data memory read or write strobe assertion.
- **Register Writeback (`RW`):** Commits loaded memory data or arithmetic results to the destination register file (`GPR` or `FPR`).
- **Hazard Characteristics:**
  - **Branch Penalty:** 2 cycles.
  - **Load-Use Stall:** 2 bubble cycles (memory access occurs in Stage 5 `MA`; loaded data is forwarded from `MA/RW` directly to `EX`).

---

## 4. Microprogrammed Control Unit

Rather than complex, inflexible hardwired random logic, the RISC201 utilizes a **Microprogrammed Control Unit** where control signals are stored in Control Memory ROM tables. The processor supports two distinct control store architectures that achieve identical cycle-by-cycle execution parity:

```mermaid
flowchart TD
    subgraph ControlStoreSubsystem [" Control Store Architecture "]
        OP["Instruction Opcode [31:26]"] --> DEC_SEL{"Encoding Mode"}
        
        DEC_SEL -->|"Horizontal (48-bit)"| H_ROM["Horizontal ROM<br/>128 x 48 bits"]
        H_ROM -->|"Direct Uncompressed Signals"| SIGS["Pipeline Control Bundle<br/>(EX, CHK, MA, WB Signals)"]
        
        DEC_SEL -->|"Vertical (24-bit)"| V_ROM["Vertical ROM<br/>128 x 24 bits"]
        V_ROM -->|"Packed Bitfields"| V_DEC["Combinational Micro-Decoder<br/>(Class & Sub-Op Expand)"]
        V_DEC -->|"100% Identical Decoded Bundle"| SIGS
    end
```

### A. 48-bit Horizontal Control ROM
Directly sets all datapath control lines with zero combinational decoding delay:
- Uncompressed bitfields route directly to ALU input muxes, FPU controllers, memory strobes, branch logic, and writeback targets.
- Organization: 128 words deep, 48 bits wide.
- Zero decode latency: signals are immediately available upon ROM access.

### B. 24-bit Vertical Control ROM
Compresses control signals into functional bitfields, decoded by a fast combinational micro-decoder:
- Organization: 128 words deep, 24 bits wide (**50% silicon ROM area reduction**).
- Bitfield structure:
  - `op_class` (3 bits): Major instruction class (ALU, FPU, Memory, Branch, System, Flow).
  - `sub_op` (4 bits): Specific operation within class.
  - `operand_mode` (3 bits): Source operand multiplexer routing.
  - `mem_action` (2 bits): None, Read, Write, CheckOnly.
  - `wb_target` (3 bits): None, ALU, Memory, PC+4, FPU.
  - `is_fpu_domain` (1 bit): Routes register write to FPR vs GPR.
  - `set_flags` (1 bit): Enables status flag updates.
  - `freeze_if_of` (1 bit): Front-end stall control for multi-uop sequences.
  - `is_multi_uop` / `is_last_uop` (2 bits): Micro-sequencer bounds.
  - `halt` (1 bit): Clock termination control.
  - `next_uop_step` (3 bits): Sequencer micro-step index.
- **Empirical Parity:** The vertical control store achieves **100% exact cycle-for-cycle parity** with the horizontal store across all benchmark programs.

---

## 5. Micro-Sequencer & Multi-Cycle Micro-Operations

While typical RISC instructions (`ADD`, `SUB`, `AND`, `LD`, `ST`, branches) execute as single-cycle operations, complex instructions (`CALL`, `RET`, `BNDCHK`) require multi-step microcode routines:

1. When a multi-step instruction is decoded in `OF`, the `MicroSequencer` asserts `freeze_if_of = true`.
2. The `PC` and `IF/OF` pipeline registers hold their values while the sequencer injects atomic micro-operations into `OF/EX`:
   - **`CALL` step 0:** Bounds check stack limits; decrement stack pointer (`sp = sp - 4`).
   - **`CALL` step 1:** Store return address to memory (`Mem[sp] = PC + 4`), mirror `PC + 4` to shadow return stack, and redirect `PC = target`.
   - **`RET` step 0:** Read return address from stack (`ra = Mem[sp]`), pop and verify against shadow return stack.
   - **`RET` step 1:** Increment stack pointer (`sp = sp + 4`), redirect `PC = ra`.
3. When `is_last_uop = true`, `freeze_if_of` is deasserted and normal pipelined instruction flow resumes.

---

## 6. Control Word Bitfield Layouts

### 48-bit Horizontal Word Layout
```
Bit Range | Field Name     | Function / Encoding
----------|----------------|----------------------------------------------------
[3:0]     | alu_op         | NOP=0, ADD=1, SUB=2, MUL=3, DIV=4, MOD=5, AND=6,
          |                | OR=7, NOT=8, XOR=9, LSL=10, LSR=11, ASR=12, CMP=13
[5:4]     | op_a_src       | 0=RS1, 1=IMM, 2=PC, 3=ZERO
[7:6]     | op_b_src       | 0=RS2, 1=IMM, 2=ZERO
[8]       | set_flags      | 1 = update condition flags (Z, N, C, V)
[11:9]    | branch_cond    | 0=EQ, 1=NE, 2=GT, 3=LT
[12]      | is_branch      | 1 = branch instruction
[13]      | is_call        | 1 = subroutine call
[14]      | is_ret         | 1 = subroutine return
[15]      | is_fpu         | 1 = route to FPU domain
[18:16]   | fpu_op         | FADD=1, FSUB=2, FMUL=3, FDIV=4, FCMP=5, FMOV=6
[19]      | chk_enable     | 1 = activate StackGuard memory check
[21:20]   | chk_mode       | 0=STACK_BOUNDS, 1=MEMORY_ADDR, 2=BNDCHK
[22]      | mem_read       | 1 = assert data memory read strobe
[23]      | mem_write      | 1 = assert data memory write strobe
[24]      | ma_is_fpu      | 1 = floating-point memory transaction
[25]      | reg_write_en   | 1 = write result back to register file
[28:26]   | reg_write_src  | 0=ALU_OUT, 1=MEM_DATA, 2=PC_PLUS4, 3=FPU_OUT, 4=PASS_B
[29]      | wb_is_fpu      | 1 = write back to FPR (f0..f15), 0 = GPR (r0..r15)
[30]      | freeze_if_of   | 1 = freeze IF/OF latch (stall front-end)
[31]      | is_multi_uop   | 1 = microcode sequence active
[32]      | is_last_uop    | 1 = final micro-operation of instruction
[33]      | halt           | 1 = halt processor clock
[47:34]   | next_micro_pc  | Micro-sequencer target address
```

### 24-bit Vertical Word Layout
```
Bit Range | Field Name     | Function / Encoding
----------|----------------|----------------------------------------------------
[2:0]     | op_class       | 0=ALU, 1=FPU, 2=MEM, 3=BRANCH, 4=SYSTEM, 5=FLOW
[6:3]     | sub_op         | Operation code within selected class
[9:7]     | operand_mode   | Joint source multiplexer configuration
[11:10]   | mem_action     | 0=None, 1=Read, 2=Write, 3=CheckOnly
[14:12]   | wb_target      | 0=None, 1=ALU, 2=MEM, 3=PC+4, 4=FPU
[15]      | is_fpu_domain  | 0=Integer GPR, 1=Floating-Point FPR
[16]      | set_flags      | 1 = update condition flags
[17]      | freeze_if_of   | 1 = freeze front-end fetch/decode latches
[18]      | is_multi_uop   | 1 = multi-uop sequence active
[19]      | is_last_uop    | 1 = final micro-operation
[20]      | halt           | 1 = halt processor clock
[23:21]   | next_uop_step  | Micro-step index (0-7)
```

---

## 7. Execution Units & Subsystems

### 1. Integer ALU (`alu/ALU.cpp`)
- **Arithmetic:** 32-bit Kogge-Stone parallel prefix adder (`ADD`, `SUB`) for fast carry propagation; Radix-4 Booth multiplier (`MUL`) for signed multiplication; non-restoring divider (`DIV`, `MOD`).
- **Logic:** Bitwise `AND`, `OR`, `XOR`, `NOT`.
- **Barrel Shifter:** Logical Shift Left (`LSL`), Logical Shift Right (`LSR`), Arithmetic Shift Right (`ASR` preserving sign bit).
- **Flag Evaluator:** Evaluates Zero (`Z`), Negative (`N`), Carry (`C`), and Overflow (`V`) according to two's complement rules.

### 2. Floating-Point Unit (FPU) (`fpu/fpu.cpp`)
- **Format:** Standard IEEE-754 single-precision (1-bit sign, 8-bit exponent with bias 127, 23-bit mantissa with implicit leading 1).
- **Operations:**
  - `FADD`, `FSUB`: Exponent difference alignment, mantissa addition/subtraction, leading-zero normalization, and round-to-nearest-even.
  - `FMUL`: Exponent addition with bias subtraction, 24x24-bit mantissa multiplication, normalization.
  - `FDIV`: Exponent subtraction with bias addition, 24-bit mantissa division.
  - `FCMP`: Floating-point comparison setting condition flags `Z` and `N`.
  - `FMOV`: Fast FPR-to-FPR register move.
  - `FLD`, `FST`: Memory load and store to/from `f0`–`f15`.

### 3. Memory Subsystem & StackGuard Firewall (`memory/`, `memory_safety/`)
- **Capacity:** 128 KiB physical memory space (32,768 32-bit words).
- **Alignment:** Strict 4-byte word alignment enforced on all memory accesses (`addr % 4 == 0`). Misaligned accesses raise an `MISALIGNED_ACCESS` exception.
- **Memory Map:**
  - `0x00000`–`0x00FFF`: Reserved low memory & code/program images (4 KiB).
  - `0x01000`–`0x1EFFF`: Accessible data memory (120 KiB, with static data default at `0x10000`).
  - `0x1F000`–`0x20000`: Bounded hardware stack window (4,096 bytes / 1,024 words, exclusive top at `0x20000`).
- **StackGuard Protection:** Hardware boundary validator monitoring `r14` (`sp`):
  - Prevents stack overflow below `0x1F000` (`STACK_OVERFLOW`).
  - Prevents stack underflow above `0x20000` (`STACK_UNDERFLOW`).
  - Blocks illegal memory stores directed below the active stack pointer (`STACK_OVERFLOW` / `BAD_MEM_ACCESS`).
- **Shadow Return Stack:** Dedicated hardware return stack mirroring return addresses on `CALL` and verifying return addresses on `RET`. Any return address overwrite raises a `SHADOW_STACK_MISMATCH` fault, preventing return-oriented attacks.
