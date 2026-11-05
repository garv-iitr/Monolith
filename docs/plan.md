# RISC201 Architectural Evolution, Development Journey & Team Plan

**Course:** CSC-201 Computer Organization & Architecture  
**Project:** RISC201 Microprogrammed Pipelined Processor  
**Lead:** Garv (`control_unit/`) | **Team:** Aditya, Atharva, Paras, Saatvik  

---

## 1. Executive Summary & Team Ownership

The RISC201 project is a cycle-accurate, software-hardware co-designed processor supporting integer arithmetic, floating-point execution, hardware memory safety, dual microcode control stores, and dynamic micro-op cracking.

| Contributor | Subsystem | Core Ownership & Deliverables |
| :--- | :--- | :--- |
| **Garv (Lead)** | `control_unit/` | Method 3 Microprogrammed Control Unit, 48b Horizontal & 24b Vertical ROMs, $\mu\text{Sequencer}$, Pipeline Bundles, 4-Stage & 6-Stage Execution Engine, Hazard & Forwarding Units |
| **Aditya** | `simulator_core/` | Single-cycle / multicycle core execution, interactive TUI Debugger with FTXUI, reverse history |
| **Atharva** | `alu/` | ALU execution, Flag Generation (`Z`, `N`, `C`, `V`), Branch Comparator, IEEE-754 FPU Subsystem |
| **Paras** | `memory_safety/` | Hardware Stack Guard (`SP=r14`), 6-stage `MEMCHK` Firewall, Runtime Exceptions, ASCII Visualizer |
| **Saatvik** | `assembler_disassembler/` | 2-pass Assembler (`risc201asm`), Disassembler (`risc201dis`), Macro Expansion Engine |

---

## 2. Chronological Development Journey & Architectural Evolution

### Phase 0: Baseline Single-Bus Microprogrammed Architecture (Sarangi Fig 9.22)
- **Initial Implementation:** The project started with the classic textbook microprogrammed architecture (Sarangi Figure 9.22), where all internal registers (`PC`, `IR`, `MAR`, `MDR`, `A`, `B`, `aluResult`, `RegFile`) share a single internal bus.
- **The Bottleneck:** Register-transfer serialization caused high CPI ($10.18 - 10.38$):
  - Instruction Fetch: 4 serial bus transfers ($\text{PC}\to\text{MAR}\to\text{Mem}\to\text{MDR}\to\text{IR}$).
  - Operand Fetch: 2 transfers ($\text{IR}\to\text{RegFile}\to\text{A, B}$).
  - ALU Execution: 1 transfer ($\text{ALU}\to\text{aluResult}$).
  - Memory Access: 2 transfers ($\text{aluResult}\to\text{MAR}$, $\text{Bus}\to\text{MDR}\to\text{Mem}$).
  - Writeback: 1 transfer ($\text{MDR}\to\text{RegFile}$).
- **Outcome:** While functionally correct, IPC was capped at $\sim 0.10$. Preserved in `legacy/` as the baseline comparison benchmark.

### Phase 1: Dual Control Memory Encodings (Horizontal vs. Vertical ROM)
- **Objective:** Eliminate hardwired control logic in favor of programmable microcode control stores, providing both maximum performance and minimal silicon area.
- **Horizontal Control ROM (48-bit):**
  - Uncompressed direct-field control word driving all ALU, multiplexer, and memory strobes with zero combinational gate delay ($T_{\text{dec}} = 0\text{ ns}$).
- **Vertical Control ROM (24-bit + Combinational $\mu$-Decoder):**
  - Field-encoded format using `op_class`, `sub_op`, and `operand_mode`.
  - Achieved **$50.0\%$ silicon ROM area reduction** (24 bits vs 48 bits) with negligible decode logic ($T_{\text{dec}} \approx 1\text{ ns}$).

### Phase 2: Pipeline Microarchitecture Design (4-Stage vs. 6-Stage)
- **Objective:** Replace the single shared bus with dedicated, synchronous inter-stage pipeline registers (`IfOfLatch`, `OfExLatch`, `ExMaRwLatch`, `ExChkLatch`, `ChkMaLatch`, `MaRwLatch`).
- **4-Stage Datapath (`[IF] -> [OF] -> [EX] -> [MA/RW]`):**
  - Designed for minimal branch penalty (2 cycles) and high IPC in compact/embedded workloads.
  - Merged memory access and register writeback into `MA/RW`.
- **6-Stage Datapath (`[IF] -> [OF] -> [EX] -> [MEMCHK] -> [MA] -> [RW]`):**
  - Designed for high-frequency clock scaling and physical hardware firewall isolation.
  - Inserts a dedicated `MEMCHK` stage between `EX` and `MA`, completely isolating address arithmetic from memory read/write strobes.

### Phase 3: Method 3 Hybrid $\mu$-Op Cracking Engine
- **The Architectural Problem:** Pure RISC pipelines cannot natively execute multi-cycle operations (`CALL`, `RET`, `BNDCHK`) without complex external hardware state machines. Conversely, traditional microprogrammed pipelines serialize single-cycle operations (`ADD`, `SUB`, `LD`).
- **The Method 3 Solution:**
  1. **RISC Fast-Path:** Standard single-cycle instructions (`ADD`, `SUB`, `AND`, `LD`, `ST`, branches) decode in 1 cycle with $\text{CPI} = 1.0$.
  2. **CISC Cracking Path:** Complex instructions trigger the `MicroSequencer`, which asserts `freeze_if_of = true` to hold the front-end while dynamically injecting atomic $\mu$-ops into `OF/EX`:
     - `CALL`: $\mu\text{-op } 0$ (Bounds Check & `SP = SP - 4`) $\to$ $\mu\text{-op } 1$ (`Mem[SP] = PC + 4; PC = Target`).
     - `RET`: $\mu\text{-op } 0$ (`RA = Mem[SP]`) $\to$ $\mu\text{-op } 1$ (`SP = SP + 4; PC = RA`).

### Phase 4: Forwarding Unit & Hazard Detection Unit
- **RAW Data Hazard Resolution:** `ForwardingUnit` checks destination registers in downstream pipeline latches (`EX/MA_RW`, `CHK/MA`, `MA/RW`) and routes fresh results directly to ALU operand muxes in `EX` (0-cycle stall).
- **Load-Use Interlocks:** `HazardDetectionUnit` detects when an instruction in `OF` depends on a load instruction currently in `OF/EX`:
  - 4-Stage: Injects 1 stall bubble.
  - 6-Stage: Injects 2 stall bubbles until memory read completes in `MA`.
- **Branch Penalty Resolution:** Branches evaluate condition codes in `EX`. On branch taken, younger speculative instructions in `IF/OF` and `OF/EX` are flushed (2-cycle branch penalty).

### Phase 5: Hardware Memory Safety Subsystem Integration
- **Collaboration with Paras (`memory_safety/`):**
  - Integrated `StackGuard` into the pipeline's memory access checking path.
  - Enforced exclusive stack window `0x1F000`–`0x1FFFF` growing down from `0x20000` (`SP=r14`).
  - Added illegal store suppression: violations trigger a `RuntimeException` before data memory write strobes can fire.
- **Verification:** Built an exhaustive 56-case regression matrix across all 4 configurations (4-Stage Horizontal, 4-Stage Vertical, 6-Stage Horizontal, 6-Stage Vertical), achieving **56/56 passing tests**.

### Phase 6: Codebase Decluttering & Architectural Verification
- Rebuilt `control_unit/` into strictly **10 flat, self-contained files** (zero subfolders).
- Preserved baseline simulation code under `legacy/` so teammate test harnesses remain intact.
- Verified cycle counts across recursive Fibonacci, Matrix Multiply, and Bubble Sort benchmarks, demonstrating up to **7.40x speedup** over the single-bus baseline.
- Proved **$100\%$ cycle-count and instruction parity** between Horizontal (48b) and Vertical (24b) control stores.

---

## 3. Academic Requirements & Compliance Matrix

This matrix maps each requirement from the CSC-201 Course Problem Statement to our implementation:

| # | Course Requirement | RISC201 Implementation | Status |
| :-: | :--- | :--- | :---: |
| **1.1** | Design 4-stage and 6-stage RISC201 processors and compare behavior | Full cycle-accurate 4-Stage (`[IF]->[OF]->[EX]->[MA/RW]`) and 6-Stage (`[IF]->[OF]->[EX]->[MEMCHK]->[MA]->[RW]`) implemented in `PipelineProcessor`. Quantitative comparison across 3 benchmarks documented in `architecture.md`. | **EXCEEDED** |
| **1.2** | Microprogrammed Control Unit (Horizontal or Vertical model) | Dual Control Memories implemented: First-principles 48-bit Horizontal ROM vs. 24-bit Vertical ROM with combinational $\mu$-decoder. | **EXCEEDED** (Both Built) |
| **1.3** | Optimized ALU with arithmetic/logical operations | 32-bit Kogge-Stone Adder, Radix-4 Booth Multiplier, Divider, Condition Flags (`Z`, `N`, `C`, `V`), and IEEE-754 FPU. | **COMPLETE** |
| **2.1** | Two-pass Assembler (`risc201asm`) | 2-pass assembler generating 32-bit hex word images, symbol resolution, and origin directive handling. | **COMPLETE** |
| **2.2** | Disassembler (`risc201dis`) | Reconstructs assembly source from binary/hex images. | **COMPLETE** |
| **2.3** | Interactive CLI Simulator & Pipeline State Inspection | Interactive debugger supporting instruction step (`s`), micro-step (`mi`), breakpoints (`b`), register/memory dumps, and reverse history. | **COMPLETE** |
| **2.4** | Interactive CLI Debugger with FTXUI TUI | Full-screen terminal UI (`risc201-sim --tui`) with multi-pane source view, register inspect, micro-PC stepper, and stack viewer. | **COMPLETE** |
| **2.5** | Preprocessor Macro Expansion for Full-Descending Stack | `PUSH`, `POP`, `SPUSH`, `SPOP` macros expanding to hardware stack operations. | **COMPLETE** |
| **2.6** | Stack Boundary Safety Verification & Runtime Exceptions | Hardware `StackGuard` enforcing bounds on `SP=r14`, illegal store suppression, and exception vectors (`STACK_OVERFLOW`, `STACK_UNDERFLOW`, `MISALIGNED_ACCESS`). | **COMPLETE** |
| **2.7** | Active Stack Memory Visualizer | Live ASCII stack frame renderer (`stack` and `stack trace` commands) showing frame markers and LIMIT/BASE boundaries. | **COMPLETE** |
| **3** | Direct microprogrammed control logic in CPU simulator | Control Memory ROM tables directly drive execution; micro-PC state displayed step-by-step. | **COMPLETE** |
| **4** | Optimized ALU class integrated into simulator | `risc201_alu` statically linked into simulator and pipeline execution datapath. | **COMPLETE** |
| **5** | Comprehensive Evaluation across benchmarks, configurations, and encodings | Verified across Recursive Fibonacci ($N=10$), Matrix Multiply + FPU ($2\times2$), and Bubble Sort + Checksum. 43/43 unit tests and 4/4 CTest harnesses passing. | **COMPLETE** |

---

## 4. Teammate Interface Contracts & Integration Guide

### Clean Subsystem Interfaces
```
                 ┌────────────────────────────────────────────────────────┐
                 │                  PipelineProcessor                     │
                 │                 (control_unit/)                        │
                 └───────────┬────────────────────────────┬───────────────┘
                             │                            │
             Stage 3 (EX)    ▼            Stage 4 (MEMCHK)▼
               ┌───────────────────────┐        ┌───────────────────────┐
               │       ALU / FPU       │        │      StackGuard       │
               │        (alu/)         │        │    (memory_safety/)   │
               │   [Lead: Atharva]     │        │     [Lead: Paras]     │
               └───────────────────────┘        └───────────────────────┘
                             ▲                            ▲
                             │                            │
                 ┌───────────┴────────────────────────────┴───────────────┐
                 │             simulator_core / risc201-sim               │
                 │                   [Lead: Aditya]                       │
                 └────────────────────────────────────────────────────────┘
```

1. **Aditya (`simulator_core/`):**
   - Calls `PipelineProcessor` as the primary execution engine.
   - Uses `--stages 4|6` and `--encoding horizontal|vertical` CLI flags to configure the datapath.
2. **Atharva (`alu/`):**
   - When pulling commits into `alu/`, ensure exported methods `ALU::execute()` and FPU operations match the `AluOp` and `FpuOp` enums in `control_unit/cu_types.h`.
3. **Paras (`memory_safety/`):**
   - `StackGuard::validate_stack_access()` and `StackGuard::validate_memory_access()` plug directly into `step_4stage()` and `step_6stage()`.
4. **Saatvik (`assembler_disassembler/`):**
   - Output hex format `%08x` matches `PipelineProcessor::load_program()`.
   - Text base defaults to `0x00000000`, data base to `0x00010000`.
