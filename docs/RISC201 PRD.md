# **Product Requirements Document**

## **RISC201 Processor Design & Simulation Suite**

**Course:** CSC-201 Computer Organization & Architecture, Autumn 2026 **Tech Stack:** C++ (C++17 or later recommended) **Team Size:** 5

---

## **1\. Overview**

We are building a complete hardware/software co-design project centered on a custom instruction set architecture, **RISC201**. The deliverable is not just an ISA on paper — it's a full toolchain and simulation environment:

* Two variants of the processor datapath (4-stage and 6-stage pipeline)  
* A microprogrammed control unit (horizontal and/or vertical)  
* A custom ALU  
* An assembler \+ disassembler  
* An interactive CLI simulator with a full debugger  
* Stack safety tooling (preprocessor macros, boundary checks, runtime exceptions)  
* An ASCII visualizer for stack memory state

All five workstreams share one core architectural contract (defined below in §2), so that every member's module plugs into the same simulator without integration surprises later.

---

## **2\. Architecture Decisions (Locked Parameters)**

These are fixed up front so all five members design against the same contract:

| Parameter | Value |
| ----- | ----- |
| Word size | 32-bit |
| General-purpose registers | 16 (R0–R15) |
| Register width | 32-bit each |
| Special registers | PC (32-bit), SP (32-bit, full-descending stack), FLAGS (Z, N, C, V) |
| Pipeline variant A | 4-stage: IF → ID/OF → EX → WB (MEM folded into EX/WB) |
| Pipeline variant B | 6-stage: IF → ID → OF → EX → MEM → WB |
| Instruction width | 32-bit fixed-width encoding |
| Instruction formats | R-type (reg-reg-reg), I-type (reg-reg-immediate), J-type (opcode \+ 26-bit address/offset) |
| Control unit | Microprogrammed, Control Memory (CM) driven, microPC-based sequencing; both horizontal and vertical encodings to be implemented and compared |
| Stack convention | Full-descending (SP decrements before push, points to last valid item) |
| Memory model | Flat, word-addressable simulated memory (size TBD by Core Engine \+ Memory lead jointly, suggest 4K words minimum) |

> **Note:** Exact opcode table, addressing modes, and the full instruction list must be jointly finalized by Atharva (ALU/datapath), Garv (control unit), and Saatvik (assembler) in Week 1, since all three depend on it. This is Milestone 0 below.

---

## **3\. Repository Structure**

Since the repo already exists, each section should live in its own module directory with a consistent interface boundary so integration doesn't require rewrites:

/risc201  
  /isa\_spec/         \# shared header: opcode enums, instruction formats, register defs  
  /assembler/         \# Saatvik — two-pass assembler \+ preprocessor  
  /disassembler/       \# Saatvik — binary/hex → assembly  
  /simulator\_core/      \# Aditya — CLI, execution engine, debugger  
  /alu/             \# Atharva — ALU class, datapath, pipeline stage logic  
  /control\_unit/       \# Garv — microcode, control memory, microPC  
  /memory\_safety/       \# Paraas — stack guards, exceptions, ASCII visualizer  
  /tests/            \# shared, per-module \+ integration tests  
  /docs/             \# design docs, ISA spec doc, eval reports

Branch convention suggestion: `feature/<module-name>/<short-desc>`, PRs into `dev`, merges into `main` only at milestone checkpoints (see §5).

**Critical shared artifact:** `/isa_spec/isa.h` — the opcode table, instruction encoding, and register enum. This file is co-owned by Saatvik, Atharva, and Garv and must be frozen after Milestone 0 (changes after that require a team sync, since three modules depend on it directly and two more depend on it transitively).

---

## **4\. Timeline & Milestones (from the assignment brief)**

| Date | Event |
| ----- | ----- |
| Sept 9 & 16 | Doubt clarification sessions |
| **Sept 30** | First Evaluation (Part 1\) |
| **Oct 14** | First Evaluation (Part 2\) |
| **Nov 10–11** | Final Evaluation |

We'll structure internal milestones around these three checkpoints, plus a Milestone 0 design sprint before Sept 30\.

---

## **5\. Per-Member Deliverables & Milestones**

### **5.0 Milestone 0 — ISA & Interface Freeze (before Sept 30, \~first week)**

*Joint deliverable — Saatvik, Aditya, Atharva, Garv, Paraas all participate*

* Finalize `isa.h`: full opcode table, instruction formats, addressing modes  
* Agree on memory size, exception codes, and the "module interface contract" (function signatures each module exposes to the simulator core)  
* Deliverable: `docs/isa_spec.md` \+ `isa_spec/isa.h` committed to `dev`

---

### **5.1 @Saatvik Sachdeva — The System Translator (Assembler & Preprocessor)**

**Covers assignment items:** 2a, 2b, 2e

**Scope:**

* Two-pass assembler: Pass 1 builds symbol table (labels, constants), Pass 2 resolves addresses and emits binary/hex machine code  
* Disassembler: parses binary/hex word strings back into RISC201 assembly mnemonics  
* Stack preprocessor: macro expansion supporting full-descending stack operations (`PUSH`, `POP`, `PUSHM`/`POPM` for multi-register, `CALL`/`RET` frame setup)

**Milestone — Sept 30 (First Eval, Part 1):**

* Pass 1 of assembler complete: symbol table construction, label resolution, syntax validation with line-numbered error messages  
* Grammar/EBNF spec for RISC201 assembly syntax documented  
* Deliverable: working Pass 1 \+ design doc for macro preprocessor

**Milestone — Oct 14 (First Eval, Part 2):**

* Pass 2 complete: full binary \+ hex output generation  
* Disassembler complete and round-trip tested (assemble → disassemble → matches original semantics)  
* Stack preprocessor macros implemented and unit tested independently  
* Deliverable: CLI tool `risc201-asm` and `risc201-disasm` runnable standalone

**Milestone — Nov 10–11 (Final Eval):**

* Full integration with Core Engine (assembler output loads directly into simulator)  
* Error handling hardened (malformed input, undefined labels, macro misuse)  
* At least 5 sample assembly programs (including recursive function calls using stack macros) assembled, disassembled, and verified  
* Contribute to final integration test suite and demo script

---

### **5.2 @Aditya Taggar — The Core Engine (CLI Simulator & Debugger)**

**Covers assignment items:** 2c, 2d, item 3 (CLI extension for microPC display)

**Scope:**

* Interactive CLI to simulate register file state and instruction execution pipeline  
* Step-by-step CPU execution simulator (single-step, run-to-completion, run-to-breakpoint)  
* Interactive CLI debugger (breakpoints, watchpoints, register/memory inspection, step/continue/reset commands)  
* Extend CLI to display microPC step-by-step state once control unit is integrated

**Milestone — Sept 30:**

* Core fetch-decode-execute loop working for the 4-stage pipeline with a minimal instruction subset (ALU ops, load/store, unconditional jump)  
* Basic CLI shell: load program, run, print register file  
* Deliverable: `risc201-sim` binary with basic run/step/registers commands

**Milestone — Oct 14:**

* Full instruction set support (both pipelines: 4-stage and 6-stage, switchable via flag)  
* Debugger features complete: breakpoints, step, watch, memory dump, register dump  
* Pipeline hazard visualization (stall/bubble display) for the 6-stage variant  
* Deliverable: functioning debugger demo with at least one program showing pipeline hazards

**Milestone — Nov 10–11:**

* Microcode integration: CLI displays microPC state, control memory word, and control signals per cycle  
* Exception handling hooks wired in (receives signals from Memory Safety module and halts/reports correctly)  
* Comparative report: 4-stage vs 6-stage behavior (cycle counts, hazard frequency) for shared test programs  
* Deliverable: final integrated simulator binary \+ comparison report section

---

### **5.3 @Atharva Bhure — The ALU Architect (Datapath & Algorithms)**

**Covers assignment items:** 1a (datapath design), 1c, item 4, item 5 (ALU-side evaluation)

**Scope:**

* Co-design the 4-stage and 6-stage RISC201 datapaths (with Garv and Aditya)  
* ALU class supporting arithmetic (add, sub, mul, div — at least one algorithmic implementation e.g. Booth's or restoring division if time allows), logical (AND/OR/XOR/NOT), and shift/rotate operations  
* Flag generation (Zero, Negative, Carry, Overflow) per operation

**Milestone — Sept 30:**

* Datapath block diagrams for both pipeline variants (draw.io/hand-drawn, digitized)  
* ALU class skeleton with basic arithmetic \+ logic ops (add, sub, and, or, xor, shift) and flag computation  
* Deliverable: `alu.h/.cpp` with unit tests for each op, datapath diagrams in `/docs`

**Milestone — Oct 14:**

* Full ALU op set complete including multiply/divide algorithm(s)  
* ALU integrated into Core Engine's execute stage for both pipeline variants  
* Edge case handling: overflow, divide-by-zero exception raised correctly  
* Deliverable: ALU fully wired into simulator, passing integration tests with Aditya's core

**Milestone — Nov 10–11:**

* Performance/behavior comparison of ALU algorithm choices (e.g., naive vs. optimized multiplication) if multiple were implemented  
* Final correctness test suite: every ALU op validated against hand-computed expected results including edge cases (max int, overflow, negative shifts)  
* Contribute to final 4-stage vs 6-stage comparative evaluation (§5.2) from the datapath/ALU angle  
* Deliverable: ALU test report \+ finalized datapath documentation

---

### **5.4 @Garv — The Microcode Engineer (Control Unit)**

**Covers assignment items:** 1b, item 3 (core microcode implementation)

**Scope:**

* Microprogrammed control unit design: both horizontal and vertical microinstruction encodings  
* Control Memory (CM) implementation storing microcode routines per opcode  
* microPC sequencing logic (including branching in microcode for conditional instructions)

**Milestone — Sept 30:**

* Control unit design doc: microinstruction format (horizontal fields vs. vertical encoded fields), CM organization, microPC addressing scheme  
* Microcode routine written (by hand/table) for at least 3 instruction types (e.g., ADD, LOAD, BRANCH)  
* Deliverable: design doc \+ CM table draft in `/docs`

**Milestone — Oct 14:**

* Horizontal microprogrammed control unit fully implemented and driving the simulator's control signals (replacing any hardwired stub logic)  
* Full microcode routine set for the entire instruction set  
* Deliverable: `control_unit.h/.cpp` (horizontal) integrated with Core Engine and ALU

**Milestone — Nov 10–11:**

* Vertical (encoded) microprogrammed control unit implemented as a second mode, selectable at runtime  
* Comparative analysis: horizontal vs. vertical — control memory size, decode latency/complexity, ease of modification  
* microPC state fully exposed to Aditya's CLI debugger (per-cycle display)  
* Deliverable: both CU variants working \+ comparison write-up

---

### **5.5 @Paraas — Memory Security & Pipeline Analyst (Evaluation Lead)**

**Covers assignment items:** 2f, 2g, item 5 (overall test/evaluation coordination)

**Scope:**

* Stack boundary safety verification (stack overflow/underflow detection)  
* Runtime exception framework (memory protection against stack corruption, invalid memory access, division by zero — coordinating exception codes with Aditya and Atharva)  
* ASCII stack memory visualizer (live rendering of stack frames during execution)  
* Own the overall test-case evaluation across configurations (4-stage vs 6-stage, horizontal vs vertical CU, different assembly programs)

**Milestone — Sept 30:**

* Exception framework design: exception codes, propagation mechanism (how a fault in memory access halts/reports through the simulator)  
* Basic stack overflow/underflow detection logic implemented as a standalone module  
* Deliverable: design doc for exception framework \+ working boundary-check module with unit tests

**Milestone — Oct 14:**

* Full runtime exception framework integrated with Core Engine (any illegal access halts execution and reports a clear error via the CLI)  
* ASCII stack visualizer v1: renders stack contents, SP position, and frame boundaries after each instruction or on demand  
* Deliverable: integrated exception handling \+ working visualizer demo

**Milestone — Nov 10–11:**

* Visualizer polished: shows push/pop animations (frame-by-frame ASCII diffs), function call frames clearly delineated  
* Full evaluation matrix executed: run the shared test-program suite across {4-stage, 6-stage} × {horizontal CU, vertical CU}, collecting cycle counts, exception cases triggered, and correctness results  
* Deliverable: final evaluation report (tables/charts) — this is the primary artifact presented at final evaluation alongside the working demo

---

## **6\. Integration Checkpoints**

Because five independently-built modules must function as one system, schedule two dedicated integration sessions (whole team, in person or call):

1. **Around Oct 7** (between the two First Eval dates): sanity-check that assembler output → simulator load → ALU execute → control unit sequencing all connect without format mismatches.  
2. **Around Nov 3** (one week before Final Eval): full dry run of the final demo — assemble a test program, run it under both pipelines and both CU modes, trigger at least one deliberate stack fault, and confirm the visualizer and debugger both reflect it correctly.

---

## **7\. Risks & Open Items**

* **Shared header lock-in:** if `isa.h` changes after Milestone 0, it cascades to Saatvik, Atharva, and Garv simultaneously — treat any post-freeze change as a team-sync event, not a solo edit.  
* **Multiply/divide algorithm scope** for the ALU is ambitious if time-constrained — Atharva should confirm by Oct 14 whether both will be implemented or just one, and flag early if descoping is needed.  
* **Vertical microcode** is comparatively more complex to encode/decode — Garv should timebox the design in Milestone 0 so it doesn't slip past Oct 14\.  
* **CR coordination for team Excel sheet / TA conflict resolution** — administrative, not technical, but noted since it's in the assignment brief and someone (likely a CR) needs to own it separately from this PRD.

---

