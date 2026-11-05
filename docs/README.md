# RISC201 Technical Documentation

Welcome to the engineering documentation for the **RISC201 Pipelined Microprocessor Project** (Course: CSC-201 Computer Organization & Architecture).

---

## 1. Documentation Index

| Document | Primary Owner | Scope & Content |
| :--- | :--- | :--- |
| **[`architecture.md`](./architecture.md)** | **Garv** | **Hardware Architecture:** Datapath comparison (Single-Bus Sarangi 9.22 vs. Pipelined RISC201), 4-stage and 6-stage designs, 48-bit Horizontal vs. 24-bit Vertical Control ROMs, and micro-sequencer operations. |
| **[`pipeline.md`](./pipeline.md)** | **Garv** | **Pipeline & Hazards:** Pipeline register latches (`IfOfLatch`, `OfExLatch`, `ExMaLatch`), RAW data forwarding logic, load-use stall interlocks, and branch flush handling. |
| **[`diagrams/`](./diagrams/)** | **Team** | **Editable Architecture Diagrams:** Editable vector schematics in Draw.io format (`pipeline_datapath.drawio`) openable in VS Code and web browsers. |
| **[`isa.md`](./isa.md)** | **Saatvik** | **Instruction Set Architecture:** 34 opcodes, instruction formats (F1–F8), register conventions, and stack preprocessor macro definitions. |
| **[`memory_safety.md`](./memory_safety.md)** | **Paras** | **Memory Safety:** `StackGuard` bounds verification (`SP=r14`), alignment enforcement, shadow return stack, and exception framework. |
| **[`tui.md`](./tui.md)** | **Aditya** | **Simulator CLI & TUI Guide:** Step-by-step CPU stepping, interactive FTXUI debugger, register/memory inspection, and reverse execution history. |
| **[`plan.md`](./plan.md)** | **Team** | **Changelog & Requirements:** Development history, module division, and problem statement compliance checklist. |
| **[`references/`](./references/)** | **Academic Archive** | Course problem statement (`problemStatement.md`) and textbook reference material (`mcu.pdf`, `pipeline.pdf`). |

---

## 2. Directory Structure

```
Monolith/
├── control_unit/           # Microcode ROMs, Sequencer, Pipeline Datapath & Bundles
├── simulator_core/         # CPU Step Loop, Memory & RegFile, FTXUI Terminal UI
├── alu/ & fpu/             # Kogge-Stone Adder, Booth Multiplier, Divider, IEEE-754 FPU
├── memory_safety/          # Hardware StackGuard, Memory Firewall, Stack Visualizer
├── assembler_disassembler/ # Standalone Assembler (risc201asm) & Disassembler (risc201dis)
├── isa_spec/               # Canonical ISA definitions & opcode mappings
├── legacy/                 # Reference baseline single-bus models & assembler library
├── tests/                  # Cycle-accurate benchmarks, unit tests, and safety matrix
└── docs/                   # Engineering documentation & editable diagrams
```

---

## 3. Quickstart (GCC / MinGW)

```powershell
# 1. Configure CMake with MinGW Makefiles
cmake -B build -G "MinGW Makefiles"

# 2. Compile all targets
cmake --build build

# 3. Run all tests (43/43 tests)
.\build\risc201-tests.exe

# 4. Run CTest harnesses
ctest --test-dir build --output-on-failure

# 5. Launch the FTXUI Terminal Debugger
.\build\risc201-sim.exe --tui
```
