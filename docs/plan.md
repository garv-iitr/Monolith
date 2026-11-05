# RISC201 Development Log & Roadmap

**Course:** CSC-201 Computer Organization & Architecture  
**Project:** RISC201 Microprogrammed Pipelined Processor  
**Team:** Garv (Control Unit & Pipeline Lead), Aditya (Simulator Core & TUI), Atharva (ALU & FPU), Paras (Memory Safety & StackGuard), Saatvik (Assembler & Disassembler)

---

## 1. Subsystem Responsibilities

| Contributor | Subsystem | Key Components |
| :--- | :--- | :--- |
| **Garv (Lead)** | `control_unit/` | Pipeline datapath (4-stage and 6-stage), dual control ROMs (48b Horizontal vs. 24b Vertical), micro-sequencer for multi-cycle operations, forwarding unit, and hazard interlocks. |
| **Aditya** | `simulator_core/` | Single-step / multi-step execution loop, memory/register inspection hooks, reverse-execution debugger history, and FTXUI terminal user interface. |
| **Atharva** | `alu/`, `fpu/` | 32-bit integer ALU (Kogge-Stone adder, Booth multiplier, divider), condition flag updates (`Z`, `N`, `C`, `V`), and IEEE-754 single-precision floating-point unit. |
| **Paras** | `memory_safety/` | Hardware `StackGuard` bounds checking on `r14` (`sp`), dedicated `MEMCHK` firewall stage, shadow return stack, and runtime exception framework. |
| **Saatvik** | `assembler_disassembler/` | Standalone two-pass assembler (`risc201asm`), disassembler (`risc201dis`), and full-descending stack macro expansion engine. |

---

## 2. Chronological Development History

### Phase 0: Baseline Single-Bus Microprogrammed Architecture
- Built the initial processor following Sarangi Figure 9.22, where registers (`PC`, `IR`, `MAR`, `MDR`, `A`, `B`, `aluResult`, register file) communicate across a single shared internal bus.
- Verified functional correctness on test programs, but identified significant bus serialization (average CPI of 9.8 to 10.4).
- Preserved this baseline model under `legacy/` for direct performance benchmarking.

### Phase 1: Dual Control Memory Encodings
- Replaced hardwired control logic with programmable Control Memory ROMs:
  - **48-bit Horizontal ROM:** Uncompressed signals driving datapath muxes and control lines directly with zero decoding logic delay.
  - **24-bit Vertical ROM:** Compact field-encoded format with a combinational micro-decoder, reducing ROM storage area by 50% while achieving identical cycle counts.

### Phase 2: Pipelined Datapaths (4-Stage & 6-Stage)
- Eliminated shared bus serialization by introducing synchronous inter-stage registers:
  - **4-Stage:** `[IF] -> [OF] -> [EX] -> [MA/RW]` (Low latency, 1-cycle load-use stall, inline memory safety checks).
  - **6-Stage:** `[IF] -> [OF] -> [EX] -> [MEMCHK] -> [MA] -> [RW]` (Decoupled hardware memory firewall stage, 2-cycle load-use stall).

### Phase 3: Micro-Operation Sequencing
- Resolved the execution of multi-cycle operations (`CALL`, `RET`, `BNDCHK`) without stalling single-cycle instructions (`ADD`, `SUB`, `LD`):
  - Standard instructions execute as single-cycle operations (CPI = 1.0).
  - Multi-cycle instructions activate the `MicroSequencer`, which asserts `freeze_if_of = true` to hold the front-end while injecting atomic micro-operations down `OF/EX`.

### Phase 4: Forwarding Unit & Hazard Detection
- Implemented `ForwardingUnit` in `control_unit/hazard_unit.h` to route fresh results from `EX/MA_RW`, `CHK/MA`, and `MA/RW` back to ALU/FPU input muxes, eliminating RAW stalls (0-cycle delay).
- Implemented `HazardDetectionUnit` to detect load-use hazards and inject pipeline stall bubbles (1 bubble in 4-stage, 2 bubbles in 6-stage).
- Implemented speculative branch flushing: branch condition evaluated in `EX`; on branch taken, younger instructions in `IF/OF` and `OF/EX` are flushed (2-cycle branch penalty).

### Phase 5: Hardware Memory Safety Integration
- Integrated Paras's `StackGuard` into the memory access stage.
- Enforced exclusive stack window (`0x1F000`–`0x20000`, full-descending on `r14`/`sp`).
- Added illegal store suppression: violations trigger a `RuntimeException` before data memory write strobes can fire.
- Verified across an automated 56-case safety test matrix (14 scenarios x 4 configurations).

### Phase 6: Full Toolchain & Hardware FPU Integration
- Integrated Atharva's IEEE-754 hardware floating-point unit directly into Stage 3 (`EX`).
- Configured CMake to build Saatvik's standalone CLI tools (`risc201asm` and `risc201dis`).
- Preserved legacy in-process assembler library under `legacy/assembler/` so Aditya's interactive TUI debugger and existing test harnesses remain fully functional.
- Recreated architecture diagrams in editable Draw.io vector format (`docs/diagrams/pipeline_datapath.drawio`).

---

## 3. Course Requirements Checklist

| Requirement | Description | Implementation Details | Status |
| :-: | :--- | :--- | :---: |
| **1.1** | 4-stage and 6-stage RISC201 processor design & comparison | Implemented in `PipelineProcessor`. Quantitative benchmark comparison documented in `architecture.md`. | Completed |
| **1.2** | Microprogrammed Control Unit (Horizontal / Vertical model) | Both implemented: 48-bit Horizontal ROM vs. 24-bit Vertical ROM with micro-decoder. | Completed |
| **1.3** | Optimized ALU with arithmetic/logical operations | 32-bit Kogge-Stone Adder, Radix-4 Booth Multiplier, Divider, Condition Flags, and IEEE-754 FPU. | Completed |
| **2.1** | Two-pass assembler (`risc201asm`) | Standalone 2-pass assembler generating 32-bit hex word images, symbol table resolution, and origin directives. | Completed |
| **2.2** | Disassembler (`risc201dis`) | Standalone disassembler reconstructing assembly instructions from hex/binary word streams. | Completed |
| **2.3** | Interactive CLI simulator & pipeline inspection | Interactive debugger supporting instruction step (`s`), micro-step (`mi`), breakpoints (`b`), and reverse history. | Completed |
| **2.4** | Step-by-step CPU debugger with TUI | Full terminal interface using FTXUI (`risc201-sim --tui`). | Completed |
| **2.5** | Preprocessor macro expansion for full-descending stack | `PUSH`, `POP`, `SPUSH`, `SPOP` full-descending stack macros. | Completed |
| **2.6** | Stack boundary safety verification & runtime exceptions | Hardware `StackGuard` enforcing bounds on `SP=r14`, illegal store suppression, and exception vectors. | Completed |
| **2.7** | Active stack memory visualizer | Live ASCII stack frame renderer (`stack` and `stack trace` commands) showing frame markers. | Completed |
| **3** | Microcode execution in CPU simulator | Control Memory ROM tables directly drive execution; micro-PC state displayed step-by-step. | Completed |
| **4** | Optimized ALU class integrated into simulator | `risc201_alu` statically linked into simulator and pipeline execution datapath. | Completed |
| **5** | Comprehensive evaluation across test cases | Evaluated on Fibonacci, Matrix Multiply with FPU, and Bubble Sort across all pipeline/ROM modes. 43/43 tests passing. | Completed |
