# RISC201 Microarchitectural Documentation

This directory contains the engineering specifications, microarchitectural analysis, and operational manuals for the **RISC201 Pipelined Microprogrammed Processor**, developed for **CSC-201: Computer Organization & Architecture**.

---

## Technical Documentation Sitemap

| Document | Focus & Scope | Key Topics Covered |
| :--- | :--- | :--- |
| **[`datapath_schematic.html`](./datapath_schematic.html)** | **Interactive Hardware Schematic** | Open directly in any web browser (zero build required). Switch between **4-Stage** and **6-Stage** views, inspect **48-bit Horizontal** vs. **24-bit Vertical** control words, filter signal domains, and click any block to view its C++ definition. |
| **[`architecture.md`](./architecture.md)** | **Processor Hardware Specification** | Datapath comparison (Single-Bus Sarangi 9.22 vs. Pipelined RISC201), 4-stage and 6-stage designs, 48-bit Horizontal vs. 24-bit Vertical Control ROMs, micro-sequencer for multi-cycle instructions, ALU, and IEEE-754 FPU. |
| **[`pipeline.md`](./pipeline.md)** | **Pipelining, Forwarding & Hazards** | Inter-stage latches (`IfOfLatch`, `OfExLatch`, `ExStageLatch`, `ChkMaLatch`, `MaRwLatch`), 3-level RAW data forwarding, load-use stall interlocks, speculative branch flush penalties, and micro-op streaming. |
| **[`isa.md`](./isa.md)** | **Instruction Set Architecture (ISA)** | Canonical 34-opcode table, 8 instruction formats (F1–F8), condition flags (`Z`, `N`, `C`, `V`), register conventions (`r0`–`r15`, `f0`–`f15`), and full-descending stack macro expansion rules (`PUSH`, `POP`, `SPUSH`, `SPOP`). |
| **[`memory_safety.md`](./memory_safety.md)** | **Memory Subsystem & StackGuard** | Physical memory map (128 KiB), hardware `StackGuard` bounds checking on `r14` (`sp`), dedicated `MEMCHK` firewall stage, shadow return stack protection, and runtime exception framework. |
| **[`tui.md`](./tui.md)** | **Interactive Debugger & FTXUI TUI** | Real-time terminal debugger manual, live stage latch visualization, register file inspector (GPRs & FPRs), memory/stack hex dumper, forward/backward stepping (`s`, `u`), and breakpoint management. |
| **[`plan.md`](./plan.md)** | **Development Log & Evolutionary History** | Chronicled microarchitectural evolution, why the single-bus datapath and modular assembler were archived in `legacy/`, design trade-offs, and requirement verification matrix. |
| **[`references/`](./references/)** | **Academic Problem Statement & Theory** | Original course problem statement ([`problemStatement.md`](./references/problemStatement.md)) and reference textbook excerpts on microprogrammed control units and pipelining. |

---

## Architectural Summary: The Evolutionary Milestone

The RISC201 processor codebase reflects a complete microarchitectural evolution:

1. **Initial Milestone (Single-Bus Serial Architecture):**  
   The project began with a textbook single-bus microprogrammed datapath (`legacy/datapath/datapath.cpp`), where all registers communicated across a single shared 32-bit bus. Because all data transfers were serialized, each instruction required 10+ clock cycles (CPI ~ 10.0). In tandem, a complex modular assembler library (`legacy/assembler/`) was constructed.
2. **Production Milestone (Pipelined Processor):**  
   To fulfill the course requirements for high-performance 4-stage and 6-stage pipelining with hazard resolution, forwarding, and branch speculative flushes, the team engineered [`control_unit/pipeline_processor`](control_unit/pipeline_processor.h). Instruction execution was parallelized with dedicated synchronous latches, dropping the CPI to **1.35–2.17** and delivering a **4.8x to 7.3x speedup**.
3. **Toolchain Streamlining:**  
   The modular assembler was superseded by the standalone two-pass tool [`assembler_disassembler/asm.cpp`](assembler_disassembler/asm.cpp), which eliminated in-process library complexity and provided direct CLI assembly to word-aligned hex images.
4. **Archival Retention in `legacy/`:**  
   The single-bus datapath and the modular assembler library were preserved in [`legacy/`](legacy) to maintain an authentic baseline for empirical cycle-count benchmarks and to provide a complete archival record of the engineering trade-offs made during development.

---

## Build & Test Quick Reference

Using [`CMakePresets.json`](../CMakePresets.json) works across **Windows (MSVC or Visual Studio IDE), macOS (Apple Clang), Linux (GCC/Clang)**, and **Windows MinGW**:

```bash
# 1. Cross-platform default build (Native compiler + Interactive FTXUI TUI)
cmake --preset default
cmake --build --preset default --parallel
ctest --preset default

# 2. Offline / zero-network headless build (CLI debugger, assembler, disassembler, tests)
cmake --preset headless
cmake --build --preset headless --parallel
ctest --preset headless
```

On macOS/Linux binaries live in `build/default/` (or `build/headless/`); on Windows MSVC they live in `build/default/Release/` (or `build/headless/Release/`):

```bash
# Launch the interactive FTXUI Terminal User Interface
./build/default/risc201-sim --tui        # macOS / Linux
.\build\default\Release\risc201-sim.exe --tui   # Windows MSVC
```
