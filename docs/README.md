# RISC201 Technical Documentation Directory

Welcome to the central technical documentation repository for the **RISC201 Pipelined Microprogrammed Processor Project** (Course: CSC-201 Computer Organization & Architecture).

---

## 1. Document Index & Architecture Map

| Document | Primary Owner | Scope & Content |
| :--- | :--- | :--- |
| **[`architecture.md`](./architecture.md)** | **Garv (Lead)** | **Master Hardware Specification:** Classic microprogrammed datapath (Sarangi Fig 9.22) vs RISC201 pipelined datapath, Method 3 Hybrid $\mu$-Op Cracking, 48-bit Horizontal vs 24-bit Vertical ROM bitfields, $\mu$-Decoder logic, and the Verified Evaluation Matrix. |
| **[`pipeline.md`](./pipeline.md)** | **Garv (Lead)** | **Pipeline In-Depth Specification:** 4-Stage (`[IF]->[OF]->[EX]->[MA/RW]`) vs 6-Stage (`[IF]->[OF]->[EX]->[MEMCHK]->[MA]->[RW]`), inter-stage synchronous latches (`IfOfLatch`, `OfExLatch`, `ExMaRwLatch`, etc.), data forwarding unit, load-use hazard interlocks, and branch resolution penalties. |
| **[`isa.md`](./isa.md)** | **Saatvik / Shared** | **Instruction Set Architecture:** Complete 34 opcodes across 4 categories (ALU, Data Movement, Control Flow/System, FPU), 8 bit-level instruction formats, and stack preprocessor macros (`PUSH`, `POP`, `SPUSH`, `SPOP`). |
| **[`memory_safety.md`](./memory_safety.md)** | **Paras** | **Hardware Memory Safety Subsystem:** `StackGuard` bounds verification (`SP=r14`), runtime memory protection against stack corruption, 56/56 passing safety test cases, and ASCII visualizer demonstration. |
| **[`tui.md`](./tui.md)** | **Aditya** | **Simulator CLI & TUI Guide:** Step-by-step CPU execution, interactive debugger using FTXUI, register/memory/micro-PC inspection panes, and reverse-execution history. |
| **[`plan.md`](./plan.md)** | **Team** | **Development Journey, Roadmap & Academic Compliance:** Chronological engineering progress history, milestone deliverables, ownership matrix, and Assignment Problem Statement Compliance Checklist. |
| **[`references/`](./references/)** | **Academic Archive** | Course problem statement (`problemStatement.md`) and theoretical reference PDFs (`mcu.pdf`, `pipeline.pdf`). |

---

## 2. Subsystem Team Ownership

```
Monolith/
├── control_unit/           # Lead: Garv      - Microcode ROMs, Sequencer, Pipeline Datapath & Bundles
├── simulator_core/         # Lead: Aditya    - CPU Step Loop, Memory & RegFile, FTXUI Terminal UI
├── alu/                    # Lead: Atharva   - Kogge-Stone Adder, Booth Multiplier, Divider, FPU
├── memory_safety/          # Lead: Paras     - Hardware StackGuard, MEMCHK Firewall, Stack Visualizer
├── assembler_disassembler/ # Lead: Saatvik   - 2-pass Assembler (risc201asm), Disassembler (risc201dis)
├── isa_spec/               # Shared          - Canonical ISA definitions & opcode mappings
├── legacy/                 # Reference       - Preserved baseline single-bus simulation models
├── tests/                  # Shared          - Cycle-accurate benchmarks, unit tests, and safety matrix
└── docs/                   # Unified         - Central engineering documentation repository
```

---

## 3. Build & Test Quickstart (Standard GCC / MinGW)

The project builds standardly using **GCC with MinGW Makefiles** (no external package manager or Ninja required):

```powershell
# 1. Configure CMake with MinGW Makefiles (FTXUI fetched automatically into build/_deps)
cmake -B build -G "MinGW Makefiles"

# 2. Compile all targets with GCC
cmake --build build

# 3. Run the comprehensive test suite (43/43 tests, including benchmarks)
.\build\risc201-tests.exe

# 4. Run all CTest harnesses
ctest --test-dir build --output-on-failure

# 5. Launch the interactive FTXUI Terminal Debugger
.\build\risc201-sim.exe --tui
```

---

## 4. Branch Integration & Contributions

- **Garv:** Rebuilt `control_unit/` with 10 canonical files, Method 3 cracking, dual ROMs (48b Horizontal vs 24b Vertical), and 4-Stage / 6-Stage pipelined execution engine.
- **Atharva & Saatvik:** As recent commits are pulled into `docs/` and their respective modules (`alu/` and `assembler_disassembler/`), their documents integrate directly into this clean indexing hierarchy.
