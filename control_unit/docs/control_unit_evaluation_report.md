# Comprehensive Control Unit Architectural Report
**Project Monolith — RISC201 Microprogrammed Control Unit**  
**Course:** CSC-201: Computer Organization and Architecture (IIT Roorkee)  
**Author / Control Unit Owner:** Garv  
**Target Audience:** Professor & Monolith Team (Garv, Atharva, Paraas, Aditya)  
**Date:** October 2026

---

## Table of Contents
1. [Executive Summary & High-Level Philosophy](#1-executive-summary--high-level-philosophy)
2. [The 4 MCU Design Configurations: Deep Comparative Analysis](#2-the-4-mcu-design-configurations-deep-comparative-analysis)
   - 2.1 [Horizontal vs. Vertical Microprogramming](#21-horizontal-vs-vertical-microprogramming)
   - 2.2 [4-Stage vs. 6-Stage Pipeline Layouts](#22-4-stage-vs-6-stage-pipeline-layouts)
   - 2.3 [The 2×2 Architecture Matrix](#23-the-22-architecture-matrix)
   - 2.4 [Instruction-by-Instruction Latency & Stage Table](#24-instruction-by-instruction-latency--stage-table)
3. [Textbook vs. Reality: Innovations Beyond Sarangi](#3-textbook-vs-reality-innovations-beyond-sarangi)
   - 3.1 [`mode[25:24]` 4-Way Expansion vs. Sarangi's 1-bit `I`](#31-mode2524-4-way-expansion-vs-sarangis-1-bit-i)
   - 3.2 [6-Bit Opcode Space (64 Opcodes)](#32-6-bit-opcode-space-64-opcodes)
   - 3.3 [FPU Subsystem Bus Integration](#33-fpu-subsystem-bus-integration)
   - 3.4 [Hardware Memory-Safety Stage (`MEMCHK`)](#34-hardware-memory-safety-stage-memchk)
   - 3.5 [Full-Descending Hardware Stack Architecture (`SP = r14`)](#35-full-descending-hardware-stack-architecture-sp--r14)
   - 3.6 [Dual-Target Microcode Compiler Infrastructure](#36-dual-target-microcode-compiler-infrastructure)
4. [Master Control Signals Specification](#4-master-control-signals-specification)
   - 4.1 [Complete Enumeration of All 33 Control Signals](#41-complete-enumeration-of-all-33-control-signals)
   - 4.2 [Internal Register Bank Map (21 Microcode-Visible Latches)](#42-internal-register-bank-map-21-microcode-visible-latches)
   - 4.3 [Signal Multiplexing & Timing Semantics](#43-signal-multiplexing--timing-semantics)
5. [Codebase Architecture & File-by-File Walkthrough](#5-codebase-architecture--file-by-file-walkthrough)
   - 5.1 [Directory Layout & Separation of Concerns](#51-directory-layout--separation-of-concerns)
   - 5.2 [Subsystem Breakdown: `common/`](#52-subsystem-breakdown-common)
   - 5.3 [Subsystem Breakdown: `datapath/`](#53-subsystem-breakdown-datapath)
   - 5.4 [Subsystem Breakdown: `horizontal/` & `vertical/`](#54-subsystem-breakdown-horizontal--vertical)
   - 5.5 [Subsystem Breakdown: `microcode/`](#55-subsystem-breakdown-microcode)
6. [Hardware Lifecycle: Step-by-Step Execution of a Micro-Cycle](#6-hardware-lifecycle-step-by-step-execution-of-a-micro-cycle)
7. [Identified Inefficiencies & Engineering Roadmap for Evaluation 2](#7-identified-inefficiencies--engineering-roadmap-for-evaluation-2)
   - 7.1 [Missing Microcode Routines (FPU & BNDCHK)](#71-missing-microcode-routines-fpu--bndchk)
   - 7.2 [Single-Bus Latency Bottleneck](#72-single-bus-latency-bottleneck)
   - 7.3 [Combinational Decode Wait Cycle Elimination](#73-combinational-decode-wait-cycle-elimination)
   - 7.4 [Micro-Branch Penalty Elimination](#74-micro-branch-penalty-elimination)
   - 7.5 [Team Integration & Header Namespace Alignment](#75-team-integration--header-namespace-alignment)

---

## 1. Executive Summary & High-Level Philosophy

The **Monolith Control Unit** is a dual-paradigm, pipeline-configurable Microprogrammed Control Unit (MCU) designed for the custom **RISC201** instruction set architecture. 

### Core Design Philosophy: *"Author Once, Target Four"*
Traditional computer architecture courses force students to choose either a hardwired control unit or a rigid microprogrammed unit, typically locked into a single layout. In Monolith, the control unit was designed with a decoupled, compiler-driven architecture:

$$\text{Symbolic Micro-Assembly DSL} \xrightarrow[\text{routine\_layout}]{\text{Linker / Placer}} \begin{cases} \xrightarrow{\text{h\_assembler}} \mathbf{Horizontal\ Control\ Memory}\ (65\text{b}) \xrightarrow{\text{Wirethrough}} \mathbf{Datapath} \\ \xrightarrow{\text{v\_assembler}} \mathbf{Vertical\ Control\ Memory}\ (45\text{b}) \xrightarrow{\mu\text{-Decoder}} \mathbf{Datapath} \end{cases}$$

Furthermore, the hardware execute loop can run in either **4-Stage** mode (classic RISC pipeline: Fetch/Decode $\to$ Execute $\to$ Memory Access $\to$ Register Write) or **6-Stage** mode (advanced fault-tolerant pipeline: Fetch $\to$ Decode $\to$ Execute $\to$ Memory Check $\to$ Memory Access $\to$ Register Write).

### Team Collaboration Boundaries
The project is strictly modularized across four core contributors:
- **Garv (Control Unit):** Owns the Micro-Sequencer, Control ROMs, Shared Bus, Decode Unit, Register File Unit, Memory Unit, and the Master Datapath Execute Loop.
- **Atharva (ALU & FPU):** Owns the integer Arithmetic Logic Unit (`alu/ALU.cpp`) and Floating Point Unit.
- **Paraas (Memory Safety):** Owns runtime bounds checking, stack protection, and visualizers (`memory_safety/`).
- **Aditya (Simulator Core & CLI):** Owns the CPU wrapper, debugger, interactive CLI, and TUI (`simulator_core/`).

---

## 2. The 4 MCU Design Configurations: Deep Comparative Analysis

The MCU supports four distinct configurations. All four run the identical microprogram semantically and yield bit-for-bit identical register and memory states at instruction completion.

```
                         CONTROL MEMORY ENCODING
                       Horizontal (77b)      Vertical (45b)
                   +-----------------------+----------------------+
          4-Stage  |  Horizontal 4-Stage   |   Vertical 4-Stage   |
PIPELINE           |  Fastest Clock,       |   Compact ROM,       |
LAYOUT             |  Minimal Latency      |   Standard Pipeline  |
                   +-----------------------+----------------------+
          6-Stage  |  Horizontal 6-Stage   |   Vertical 6-Stage   |
                   |  Hardware Safety,     |   Max Area Savings,  |
                   |  Zero Decode Delay    |   Full Memory Safety |
                   +-----------------------+----------------------+
```

### 2.1 Horizontal vs. Vertical Microprogramming

| Metric / Dimension | Horizontal Control Unit | Vertical Control Unit |
| :--- | :--- | :--- |
| **Control ROM Width** | **77 bits** (`HorizontalMicroWord`: 45 control signals + 12b imm + 10b target + 10b args) | **45 bits** (`VerticalMicroWord`: 3b op + 5b src + 5b dest + 12b imm + 10b target + 10b args) |
| **ROM Storage Savings** | Baseline ($1.0\times$) | **$41.6\%$ ROM bit reduction** ($0.584\times$) |
| **Decoding Circuitry** | **None** (`h_codec.h` is a direct wirethrough) | **Combinational $\mu$-Decoder** (`v_decoder.cpp`) |
| **Decoder Gate Delay** | **$T_{dec} = 0$ ns** | **$T_{dec} > 0$ ns** (multi-level gate delay) |
| **Critical Path** | $T_{clk} \ge T_{ROM\_access} + T_{bus} + T_{unit\_setup}$ | $T_{clk} \ge T_{ROM\_access} + T_{dec} + T_{bus} + T_{unit\_setup}$ |
| **Hardware Complexity** | Wider interconnect routing; larger silicon area | Smaller ROM footprint; extra combinational logic area |

- **Horizontal Microprogramming (Sarangi §9.8.2 scaled to Monolith):** Each bit in the control signal field corresponds directly to a physical control wire or grouped selector in the datapath. In Monolith's 30-register architecture, there are 45 physical control signal lines (30 read enable wires, 5 write-source selector lines, 2 transfer MUX lines, and 8 strobes/sequencer lines). There is no combinational decoding delay. It maximizes clock frequency at the expense of wider ROM memory cells.
- **Vertical Microprogramming (Sarangi §9.8.1):** Control signals are mutually exclusive within functional groups. The control word packs micro-operations into compact fields:
  $$\text{Word (45b)} = \text{op}[3\text{b}] \parallel \text{src}[5\text{b}] \parallel \text{dest}[5\text{b}] \parallel \text{imm}[12\text{b}] \parallel \text{target}[10\text{b}] \parallel \text{args}[10\text{b}]$$
  A combinational **$\mu$-Decoder** expands these 45 bits into the 45 physical control signals before driving the datapath.

### 2.2 4-Stage vs. 6-Stage Pipeline Layouts

The microprogrammed datapath tags every micro-step with an abstract `StageClass` (`FETCH`, `DECODE`, `EXECUTE`, `CHECK`, `MEMORY`, `WRITEBACK`, `SEQUENCE`). The selected layout maps this class to architectural stages via `stage_name()`:

$$\begin{aligned}
\mathbf{4\text{-}Stage\ Pipeline:} &\quad \mathbf{(IF/OF)} \longrightarrow \mathbf{EX} \longrightarrow \mathbf{MA} \longrightarrow \mathbf{RW} \\
\mathbf{6\text{-}Stage\ Pipeline:} &\quad \mathbf{IF} \longrightarrow \mathbf{OF} \longrightarrow \mathbf{EX} \longrightarrow \mathbf{MEMCHK} \longrightarrow \mathbf{MA} \longrightarrow \mathbf{RW}
\end{aligned}$$

- **In 4-Stage Mode:** Preamble cycles 0..3 are grouped as `IF/OF`. Memory access goes straight from `EX` into `MA`.
- **In 6-Stage Mode:** Cycle 0 is `IF`, cycles 1..3 are `OF`. For memory instructions (`LD`, `ST`, `CALL`, `RET`), a dedicated **`MEMCHK`** micro-cycle is inserted between `EX` and `MA`. This cycle passes `memchk_addr` to Paraas's Memory Safety unit with `MemChkOp::CHECK`. If an address is out of bounds or misaligned, the hardware raises an architectural fault *before* touching physical memory.

### 2.3 The 2×2 Architecture Matrix

| Feature | Horizontal 4-Stage | Horizontal 6-Stage | Vertical 4-Stage | Vertical 6-Stage |
| :--- | :--- | :--- | :--- | :--- |
| **Class in Code** | `MicroControlUnit<HorizontalMicroWord, HorizontalCodec>` | `MicroControlUnit<HorizontalMicroWord, HorizontalCodec>` | `MicroControlUnit<VerticalMicroWord, VerticalCodec>` | `MicroControlUnit<VerticalMicroWord, VerticalCodec>` |
| **Layout Parameter** | `StageLayout::FOUR_STAGE` | `StageLayout::SIX_STAGE` | `StageLayout::FOUR_STAGE` | `StageLayout::SIX_STAGE` |
| **Memory Word** | 77-bit unencoded | 77-bit unencoded | 45-bit encoded | 45-bit encoded |
| **Decoder Unit** | Bypassed | Bypassed | Active (`VerticalDecoder`) | Active (`VerticalDecoder`) |
| **`MEMCHK` Cycle** | Skipped | **Active** (+1 cycle on mem ops) | Skipped | **Active** (+1 cycle on mem ops) |
| **Best Used For** | High-performance integer kernels | High-speed safe computing | Area-constrained embedded systems | High-reliability mission-critical systems |

---

### 2.4 Instruction-by-Instruction Latency & Stage Table

The following table documents the exact micro-cycle counts and stage behaviors across all instruction families.

| Instruction Category | Assembly Syntax | Mode | Horiz 4-Stage | Horiz 6-Stage | Vert 4-Stage | Vert 6-Stage | Stage Flow (4-Stage vs. 6-Stage) |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Instruction Preamble** | *(Every Instruction)* | — | **4 cycles** | **4 cycles** | **4 cycles** | **4 cycles** | `IF/OF` vs. `IF -> OF` |
| **3-Operand ALU** | `ADD r1, r2, r3` | Reg | **13 cycles** | **13 cycles** | **13 cycles** | **13 cycles** | `IF/OF -> EX -> RW` vs. `IF -> OF -> EX -> RW` |
| | `ADD r1, r2, #imm` | Imm | **11 cycles** | **11 cycles** | **11 cycles** | **11 cycles** | `IF/OF -> EX -> RW` vs. `IF -> OF -> EX -> RW` |
| **2-Operand ALU** | `MOV r1, r2` / `NOT` | Reg | **11 cycles** | **11 cycles** | **11 cycles** | **11 cycles** | `IF/OF -> EX -> RW` vs. `IF -> OF -> EX -> RW` |
| | `MOV r1, #imm` / `NOT`| Imm | **9 cycles** | **9 cycles** | **9 cycles** | **9 cycles** | `IF/OF -> EX -> RW` vs. `IF -> OF -> EX -> RW` |
| **Comparison** | `CMP r1, r2` | Reg | **10 cycles** | **10 cycles** | **10 cycles** | **10 cycles** | `IF/OF -> EX` vs. `IF -> OF -> EX` (No RF writeback) |
| | `CMP r1, #imm` | Imm | **9 cycles** | **9 cycles** | **9 cycles** | **9 cycles** | `IF/OF -> EX` vs. `IF -> OF -> EX` (No RF writeback) |
| **Memory Load** | `LD rd, [rs1, #imm]` | Mem | **11 cycles** | **12 cycles** | **11 cycles** | **12 cycles** | `EX -> MA -> RW` vs. `EX -> MEMCHK -> MA -> RW` |
| **Memory Store** | `ST rd, [rs1, #imm]` | Mem | **11 cycles** | **12 cycles** | **11 cycles** | **12 cycles** | `EX -> MA` vs. `EX -> MEMCHK -> MA` |
| **Unconditional Branch**| `B offset` | Br | **6 cycles** | **6 cycles** | **6 cycles** | **6 cycles** | `IF/OF -> EX` vs. `IF -> OF -> EX` |
| **Conditional Branch** | `BEQ offset` (Taken) | Br | **7 cycles** | **7 cycles** | **7 cycles** | **7 cycles** | `IF/OF -> EX` vs. `IF -> OF -> EX` |
| | `BEQ offset` (Untaken)| Br | **6 cycles** | **6 cycles** | **6 cycles** | **6 cycles** | `IF/OF -> EX` vs. `IF -> OF -> EX` |
| **Subroutine Call** | `CALL offset` (SP push)| Br | **11 cycles** | **12 cycles** | **11 cycles** | **12 cycles** | `EX -> MA -> RW` vs. `EX -> MEMCHK -> MA -> RW` |
| **Subroutine Return** | `RET` (SP pop) | Sys | **11 cycles** | **12 cycles** | **11 cycles** | **12 cycles** | `EX -> MA -> RW` vs. `EX -> MEMCHK -> MA -> RW` |
| **Control / System** | `NOP` | Sys | **5 cycles** | **5 cycles** | **5 cycles** | **5 cycles** | Preamble + 1-cycle jump |
| | `HALT` | Sys | **5 cycles** | **5 cycles** | **5 cycles** | **5 cycles** | Preamble + 1-cycle halt assertion |

---

## 3. Textbook vs. Reality: Innovations Beyond Sarangi

While Sarangi’s *Basic Computer Architecture* (Chapter 9) provides the academic foundation for microprogrammed control, our implementation introduces six critical innovations required to build a real-world, industrial-grade processor:

```
+---------------------------------------------------------------------------------------------------------+
|                                    ARCHITECTURAL EVOLUTION MAP                                          |
+------------------------------------+--------------------------------------------------------------------+
| SARANGI TEXTBOOK BASELINE          | MONOLITH RISC201 IMPLEMENTATION                                    |
+------------------------------------+--------------------------------------------------------------------+
| 1-bit Immediate Flag 'I' (Bit 27)  | 2-bit 'mode' Field [25:24]: Register, Signed, Unsigned, High-Imm   |
| 4-bit / 5-bit Opcode (16 opcodes)  | 6-bit Opcode Field [31:26] (64 opcodes; 34 active ISA instructions) |
| Integer Only                       | Unified Internal FPU Bank (F0-F15, fpA, fpB, fpuResult, FCC)      |
| Unprotected Memory Access          | Hardware 6th Stage MEMCHK (Bounds Verification + Word Alignment)   |
| Simplified Register-Based Return   | Full-Descending Architectural Stack (SP = r14, Push/Pop Return PC) |
| Paper-Only Block Diagrams          | Dual-Target Micro-Assembly DSL & Two-Pass Concrete ROM Assemblers  |
+------------------------------------+--------------------------------------------------------------------+
```

### 3.1 `mode[25:24]` 4-Way Expansion vs. Sarangi's 1-bit `I`
In Sarangi (Section 9.5.2, Figure 9.18), Bit 27 is a single binary switch: $I=0$ (register) or $I=1$ (immediate).
- **Limitation:** In a 32-bit architecture, a single 16-bit immediate cannot load numbers into upper halves of registers, nor can it handle logical operations requiring zero extension.
- **Monolith Solution:** We replaced bit 27 with a **2-bit `mode` field** located at `ir[25:24]`:
  - `mode = 00`: Register-to-register operation ($I=0$).
  - `mode = 01`: Signed 16-bit immediate ($\text{sign-extended to 32 bits}$, $I=1$).
  - `mode = 10`: Unsigned 16-bit immediate ($\text{zero-extended to 32 bits}$, $I=1$).
  - `mode = 11`: High 16-bit immediate ($\text{imm16} \ll 16$, $I=1$) for constructing 32-bit constants in 2 instructions.
- The combinational `DecodeUnit` continuously computes:
  $$\text{out.I} = (\text{mode} \ne 0), \quad \text{out.immx} = \text{expand\_immediate}(\text{mode}, \text{imm16})$$
  This allows the microcode to evaluate `mbeq(IR::I, 1, L_IMM)` seamlessly regardless of the expansion type!

### 3.2 6-Bit Opcode Space (64 Opcodes)
Sarangi uses 4 bits or 5 bits for opcodes, limiting the processor to fewer than 20 operations. Monolith implements a 6-bit opcode in `ir[31:26]`, giving 64 possible opcodes. This accommodates standard ALU, multiply/divide, branches, memory accesses, floating-point instructions, and system controls.

### 3.3 FPU Subsystem Bus Integration
Sarangi’s datapath is strictly integer-only. Monolith extends the shared bus architecture with seven dedicated internal FPU interface registers:
- `fp_src`: 4-bit FPR index (F0–F15) being addressed.
- `fp_data`: 32-bit data latch to write into an FPR.
- `fp_val`: 32-bit data latch read out of an FPR.
- `fp_a`, `fp_b`: IEEE 754 single-precision operand latches.
- `fpu_result`: Output latch holding computed float results.
- `fcc`: Floating-point condition code latch (mirroring comparison results into architectural flags).

### 3.4 Hardware Memory-Safety Stage (`MEMCHK`)
Modern computer architecture demands runtime memory isolation. We introduced a dedicated hardware pipeline stage between Execute and Memory Access:
- In `SIX_STAGE` layout, memory-referencing microcode (`LD`, `ST`, `CALL`, `RET`) issues an explicit check:
  ```cpp
  mmov(IR::MEMCHK_ADDR, IR::ALU_RESULT, a_chk(MemChkOp::CHECK), SC::CHECK);
  ```
- Paraas's Memory Safety unit intercepts `memchk_addr`. If the address violates bounds or fails 4-byte alignment, the hardware aborts before issuing `MemOp::READ` or `MemOp::WRITE`, preventing segmentation faults and memory corruption.

### 3.5 Full-Descending Hardware Stack Architecture (`SP = r14`)
Textbooks frequently oversimplify subroutine calls by writing the return address into a single link register (`ra`). This fails when handling recursive functions.
- Monolith implements an industrial **Full-Descending Stack** backed by RAM using General Purpose Register 14 (`SP = r14`).
- `CALL offset`: Reads `SP`, decrements by 4 via the micro-adder, checks memory safety (`MEMCHK`), updates `SP`, writes the current `PC` into data memory at `MAR`, and branches to `branch_target`.
- `RET`: Reads `SP`, checks memory bounds, loads `mem[SP]` into `PC`, increments `SP` by 4 via the micro-adder, and writes the updated stack pointer back to register 14.
- This allows recursive calls (such as our 18,937-cycle recursive Fibonacci test) to execute with complete architectural integrity.

### 3.6 Dual-Target Microcode Compiler Infrastructure
In Sarangi, microcode tables are manually calculated on paper. In Monolith:
1. Microcode is authored symbolically using a C++ DSL (`micro_asm.h`).
2. `routine_layout.h` performs a two-pass link: Pass 1 assigns absolute $\mu\text{PC}$ addresses to statements and records local label offsets; Pass 2 resolves forward and backward branches.
3. Two separate assembler backends (`h_assembler.cpp` and `v_assembler.cpp`) consume this layout to generate the binary ROM matrices. Both share the exact same semantics engine (`micro_semantics()`), mathematically guaranteeing zero semantic divergence between horizontal and vertical units.

---

## 4. Master Control Signals Specification

### 4.1 Physical Control Signals (45 Lines)

Every micro-cycle, the control unit outputs **45 discrete physical control signal lines** (`kHorizontalControlSignalBits`):

```
+----+---------------+-------+---------------+-------------------------------------------------------------+
| #  | SIGNAL NAME   | WIDTH | DEFAULT VALUE | FUNCTIONAL DESCRIPTION                                      |
+----+---------------+-------+---------------+-------------------------------------------------------------+
| 1  | write_src     | 5b    | NONE (0)      | Selects 1 of 30 internal registers to drive the WRITE BUS.  |
| 2  | read_mask     | 30b   | 0 (No latch)  | One-hot latch mask for 30 internal registers on READ BUS.   |
| 3  | transfer_sel  | 2b    | WRITE_BUS (0) | 00: Write Bus -> Read Bus                                   |
|    |               |       |               | 01: Micro-Immediate (muimm) -> Read Bus                     |
|    |               |       |               | 10: Micro-Adder (write_bus + muimm) -> Read Bus             |
| 4  | fetch_ir      | 1b    | 0 (Inactive)  | Strobe: Loads instruction from Instruction Memory[PC] -> IR.|
| 5  | set_flags     | 1b    | 0 (Inactive)  | Enables ALU to latch and commit flags (Z, N, C, V, E, GT).  |
| 6  | fpu_set_fcc   | 1b    | 0 (Inactive)  | Commits Floating-Point Condition Code (FCC).                 |
| 7  | memchk_op     | 1b    | NOP (0)       | Commands Paraas's Memory Safety unit: 1 = CHECK.            |
| 8  | halt          | 1b    | 0 (Running)   | Asserts processor HALT state; disables sequencer clock.     |
| 9  | next_addr     | 3b    | INCREMENT (0) | Steers Micro-Sequencer:                                     |
|    |               |       |               | 000: INCREMENT   (μPC <- μPC + 1)                           |
|    |               |       |               | 001: MSWITCH     (μPC <- entry_table[opcode])               |
|    |               |       |               | 010: JUMP        (μPC <- jump_target)                       |
|    |               |       |               | 011: BRANCH_COND (μPC <- isMBranch ? target : μPC + 1)      |
|    |               |       |               | 100: FETCH       (μPC <- 0, preamble restart)               |
+----+---------------+-------+---------------+-------------------------------------------------------------+
Total Pure Control Signal Lines = 30 + 5 + 2 + 1 + 1 + 1 + 1 + 1 + 3 = 45 bits.
Horizontal Micro-Word Width = 45 (control lines) + 12 (imm) + 10 (target) + 10 (unit args) = 77 bits.
```

### 4.2 Internal Register Bank Map (30 Microcode-Visible Latches)

Mapped directly to indices 0..29 in `isa_spec/isa.h` (`kNumInternalRegs = 30`):

```
Bit  0: NONE            Bit  8: branch_target   Bit 16: alu_result      Bit 24: fp_b
Bit  1: PC              Bit  9: reg_src         Bit 17: mar             Bit 25: fpu_result
Bit  2: IR              Bit 10: reg_data        Bit 18: mdr             Bit 26: fcc
Bit  3: I               Bit 11: reg_val         Bit 19: ld_result       Bit 27: memchk_status
Bit  4: rd              Bit 12: A               Bit 20: fp_src          Bit 28: memchk_addr
Bit  5: rs1             Bit 13: B               Bit 21: fp_data         Bit 29: flags.N
Bit  6: rs2             Bit 14: flags.E         Bit 22: fp_val
Bit  7: immx            Bit 15: flags.GT        Bit 23: fp_a
```

### 4.3 Signal Multiplexing & Timing Semantics
- **Atomic Single-Cycle Forwarding:** When data is driven onto the write bus and latched into an internal register via the read bus during step $N$, the latched value is immediately accessible to functional units during the *same* cycle. This hardware property enables instruction fusing (e.g. `mmov(IR::B, IR::REG_VAL, a_alu(AluOp::ADD))` transfers `regVal` into register `B` and executes the addition in one clock cycle).
- **Condition Comparator (`isMBranch`):** Evaluated inside `SharedBus::eval()` as:
  $$\text{isMBranch} = (\text{write\_bus} == \text{mu\_imm})$$
  This comparator provides zero-overhead conditional micro-branching without needing a full ALU cycle.

---

## 5. Codebase Architecture & File-by-File Walkthrough

### 5.1 Directory Layout & Separation of Concerns

```
control_unit/
├── control_unit.h             <-- Public facade included by external modules
├── main.cpp                   <-- Self-contained CU test suite & demo
├── common/                    <-- Core sequencer, bus, registers, and signals
│   ├── control_memory.h
│   ├── control_signals.h
│   ├── internal_registers.h / .cpp
│   ├── microinstruction.h
│   ├── micro_control_unit.h
│   ├── micro_sequencer.h
│   ├── mux.h
│   └── shared_bus.h
├── datapath/                  <-- Hardware execution components
│   ├── datapath.h / .cpp
│   ├── decode_unit.h
│   ├── memory_unit.h / .cpp
│   └── register_file_unit.h
├── horizontal/                <-- Horizontal wirethrough codec
│   └── h_codec.h
├── vertical/                  <-- Vertical micro-decoder logic
│   ├── v_codec.h
│   └── v_decoder.h / .cpp
├── microcode/                 <-- DSL, program routines, and compilers
│   ├── micro_asm.h
│   ├── routine_layout.h
│   ├── microcode_program.h / .cpp
│   ├── h_assembler.h / .cpp
│   └── v_assembler.h / .cpp
└── docs/                      <-- Schematics and architecture guides
    ├── architecture.md
    └── diagrams/ (Collision-free Draw.io hardware schematics)
```

---

### 5.2 Subsystem Breakdown: `common/`

- [`common/control_signals.h`](file:///e:/25114035/CSC-201/Monolith/control_unit/common/control_signals.h): Declares the 33-signal bundle (`ControlSignals`), unit IDs, opcode enums (`AluOp`, `RegOp`, `MemOp`, `FpuOp`, `MemChkOp`), and the pipeline stage mapper `stage_name()`.
- [`common/internal_registers.h`](file:///e:/25114035/CSC-201/Monolith/control_unit/common/internal_registers.h) & [`common/internal_registers.cpp`](file:///e:/25114035/CSC-201/Monolith/control_unit/common/internal_registers.cpp): Declares and implements the 21 internal latches (`InternalRegisters`). Provides width-agnostic `.get(reg)` and `.set(reg, val)` helper accessors.
- [`common/shared_bus.h`](file:///e:/25114035/CSC-201/Monolith/control_unit/common/shared_bus.h): Implements the hardware shared bus: `write_bus`, micro-adder, equality comparator (`is_m_branch`), 3:1 transfer multiplexer, and `read_bus`.
- [`common/micro_sequencer.h`](file:///e:/25114035/CSC-201/Monolith/control_unit/common/micro_sequencer.h): Implements the $\mu\text{PC}$ next-address logic. Contains the 2:1 multiplexer (`M1`) steered by `is_m_branch` and the 4:1 `ufetch_mux` that selects between increment, direct jump, opcode dispatch table target (`mswitch`), or conditional branch.
- [`common/control_memory.h`](file:///e:/25114035/CSC-201/Monolith/control_unit/common/control_memory.h): Generic templated 1024-word Control ROM storage and opcode entry mapping table.
- [`common/microinstruction.h`](file:///e:/25114035/CSC-201/Monolith/control_unit/common/microinstruction.h): Defines the bitfield layout for `HorizontalMicroWord` (65 bits), `VerticalMicroWord` (45 bits), and `DecodedMicro`.
- [`common/mux.h`](file:///e:/25114035/CSC-201/Monolith/control_unit/common/mux.h): Clean parameterizable hardware multiplexer templates (`Mux2`, `Mux3`, `Mux4`, `Mux8`).
- [`common/micro_control_unit.h`](file:///e:/25114035/CSC-201/Monolith/control_unit/common/micro_control_unit.h): The master control engine. Orchestrates the fetch-decode-execute-advance cycle for both horizontal and vertical variants.

---

### 5.3 Subsystem Breakdown: `datapath/`

- [`datapath/datapath.h`](file:///e:/25114035/CSC-201/Monolith/control_unit/datapath/datapath.h) & [`datapath/datapath.cpp`](file:///e:/25114035/CSC-201/Monolith/control_unit/datapath/datapath.cpp): Connects the internal registers, shared bus, decode unit, register file, and memory unit. The `execute(sig, muimm)` function executes one physical micro-cycle.
- [`datapath/decode_unit.h`](file:///e:/25114035/CSC-201/Monolith/control_unit/datapath/decode_unit.h): Pure combinational logic continuously slicing `ir` into `rd`, `rs1`, `rs2`, `mode`, `immx`, and `branch_target`.
- [`datapath/register_file_unit.h`](file:///e:/25114035/CSC-201/Monolith/control_unit/datapath/register_file_unit.h): Architectural Register File containing the 16 general-purpose registers (`R0` through `R15`).
- [`datapath/memory_unit.h`](file:///e:/25114035/CSC-201/Monolith/control_unit/datapath/memory_unit.h) & [`datapath/memory_unit.cpp`](file:///e:/25114035/CSC-201/Monolith/control_unit/datapath/memory_unit.cpp): Unified 32-bit word-addressed memory backing both instruction fetches (`fetch(pc)`) and data access (`MAR` / `MDR` / `ld_result`).

---

### 5.4 Subsystem Breakdown: `horizontal/` & `vertical/`

- [`horizontal/h_codec.h`](file:///e:/25114035/CSC-201/Monolith/control_unit/horizontal/h_codec.h): Horizontal decoder replacement. Implements a zero-delay direct wire connection (`d.sig = w.sig`).
- [`vertical/v_codec.h`](file:///e:/25114035/CSC-201/Monolith/control_unit/vertical/v_codec.h): Routes vertical micro-words into the vertical decoder.
- [`vertical/v_decoder.h`](file:///e:/25114035/CSC-201/Monolith/control_unit/vertical/v_decoder.h) & [`vertical/v_decoder.cpp`](file:///e:/25114035/CSC-201/Monolith/control_unit/vertical/v_decoder.cpp): Combinational hardware $\mu$-Decoder. Extracts fields from the 45-bit word, sign-extends the 12-bit immediate, and unpacks signals via `micro_semantics()`.

---

### 5.5 Subsystem Breakdown: `microcode/`

- [`microcode/micro_asm.h`](file:///e:/25114035/CSC-201/Monolith/control_unit/microcode/micro_asm.h): The symbolic micro-assembly DSL and the single source of truth for micro-semantics (`mloadIR`, `mdecode`, `mswitch`, `mmov`, `mmovi`, `madd`, `mbeq`, `mb`, `mhalt`).
- [`microcode/routine_layout.h`](file:///e:/25114035/CSC-201/Monolith/control_unit/microcode/routine_layout.h): Two-pass microcode layout linker. Places preamble at 0..3, places routines back-to-back, and resolves routine-local branch targets (`L_IMM`, `L_RW`, `L_BRANCH`, `kPreamble`).
- [`microcode/microcode_program.h`](file:///e:/25114035/CSC-201/Monolith/control_unit/microcode/microcode_program.h) & [`microcode/microcode_program.cpp`](file:///e:/25114035/CSC-201/Monolith/control_unit/microcode/microcode_program.cpp): Contains the routine implementations for all core integer and control opcodes.
- [`microcode/h_assembler.h`](file:///e:/25114035/CSC-201/Monolith/control_unit/microcode/h_assembler.h) & [`microcode/h_assembler.cpp`](file:///e:/25114035/CSC-201/Monolith/control_unit/microcode/h_assembler.cpp): Compiles the linked microprogram into `ControlMemory<HorizontalMicroWord>`.
- [`microcode/v_assembler.h`](file:///e:/25114035/CSC-201/Monolith/control_unit/microcode/v_assembler.h) & [`microcode/v_assembler.cpp`](file:///e:/25114035/CSC-201/Monolith/control_unit/microcode/v_assembler.cpp): Compiles the linked microprogram into `ControlMemory<VerticalMicroWord>`.

---

## 6. Hardware Lifecycle: Step-by-Step Execution of a Micro-Cycle

Every clock tick in `MicroControlUnit::step()` executes through five synchronous hardware phases:

```
  +-------------------------------------------------------------------------+
  | 1. ROM FETCH: Fetch MicroWord from Control Memory at current microPC    |
  +-------------------------------------------------------------------------+
                                     │
                                     ▼
  +-------------------------------------------------------------------------+
  | 2. SIGNAL MATERIALIZATION:                                              |
  |    - Horizontal: Wirethrough (d.sig = w.sig, T_dec = 0)                 |
  |    - Vertical  : Micro-Decoder translates 45b word into ControlSignals  |
  +-------------------------------------------------------------------------+
                                     │
                                     ▼
  +-------------------------------------------------------------------------+
  | 3. OP-CODE DISPATCH CHECK:                                              |
  |    If next_addr == MSWITCH: switch_target = cm.entry(opcode_of(ir))     |
  +-------------------------------------------------------------------------+
                                     │
                                     ▼
  +-------------------------------------------------------------------------+
  | 4. DATAPATH EXECUTION:                                                  |
  |    a. Instruction Fetch: ir <- mem[pc] (if fetch_ir asserted)           |
  |    b. Combinational Decode: ir sliced into rd, rs1, rs2, mode, immx     |
  |    c. Write Bus Drive: write_bus <- regs[write_src]                     |
  |    d. Transfer MUX / Adder: read_bus <- MUX(write_bus, muimm, adder)    |
  |    e. Read Bus Latch: Latch read_bus into all regs matching read_mask   |
  |    f. Unit Tick: Register File, ALU (PASS_A/B), and Memory update       |
  +-------------------------------------------------------------------------+
                                     │
                                     ▼
  +-------------------------------------------------------------------------+
  | 5. SEQUENCER ADVANCE:                                                   |
  |    M1 MUX selects between (μPC + 1) and jump_target using isMBranch;    |
  |    μFetch MUX selects final next μPC (INCREMENT, JUMP, MSWITCH, COND)   |
  +-------------------------------------------------------------------------+
```

---

## 7. Identified Inefficiencies & Engineering Roadmap for Evaluation 2

To achieve full marks and deliver a production-quality processor, the team should address the following five areas:

### 7.1 Full 34-Opcode Implementation (Completed)
- **Status:** All 34 ISA opcodes (`ADD..ASR`, `NOT`, `MOV`, `CMP`, `LD`, `ST`, `B..BLT`, `CALL`, `RET`, `HALT`, `NOP`, `FADD`, `FSUB`, `FMUL`, `FDIV`, `FCMP`, `FLD`, `FST`, `FMOV`, `BNDCHK`) now have dedicated microcode routines in `control_unit/microcode/microcode_program.cpp`.
- **Verification:** All 16 unit tests in `risc201-tests.exe` (including Fibonacci, 2x2 Matrix Multiply + FPU kernel, and Bubble Sort + Checksum) pass with zero errors across all four MCU configurations.

### 7.2 Single-Bus Latency Bottleneck
- **Current State:** A single shared bus requires serialized transfers. For example, `ADD r1, r2, r3` requires:
  - Cycle 1: Read $rs1$ into $A$
  - Cycle 2: Read $rs2$ into $B$
  - Cycle 3: Compute $A+B$ and store into $rd$
  Total execution latency is 13 micro-cycles per instruction.
- **Future Improvement:** In a pipelined datapath or dual-bus architecture, $rs1$ and $rs2$ can be read simultaneously in a single cycle, reducing execution latency by $30\%$.

### 7.3 Combinational Decode Wait Cycle Elimination
- **Current State:** The preamble dedicates Cycle 1 (`mdecode`) as a pure wait cycle while the combinational decode unit slices `ir`.
- **Future Improvement:** Because the `DecodeUnit` is purely combinational, its outputs settle within nanoseconds after `ir` is latched. We can fuse `mloadIR` and `mdecode`, saving 1 micro-cycle on **every single instruction executed by the CPU** (a global $10\text{–}15\%$ throughput increase).

### 7.4 Micro-Branch Penalty Elimination
- **Current State:** Testing whether an instruction is an immediate form requires a conditional micro-branch (`mbeq I, 1, L_IMM`), which costs an extra sequencing step.
- **Future Improvement:** Introduce hardware sub-dispatch or direct MUX steer logic so the sequencer branches directly from `mswitch` into either the register variant or the immediate variant of the routine.

### 7.5 Team Integration & Header Namespace Alignment
- **Current State:** `simulator_core` and tests include headers using relative paths (e.g. `#include "control_unit.h"` or `#include "common/control_signals.h"`), requiring manual `-Icontrol_unit` include flags in CMake.
- **Target Fix:** Standardize all project includes to root-relative paths (e.g. `#include "control_unit/control_unit.h"`, `#include "control_unit/common/control_signals.h"`). This ensures seamless IDE IntelliSense and eliminates build issues across all teammates' platforms.
