# RISC201 Team Plan & Integration Procedures

Master roadmap, status audit, missing items, and step-by-step procedures for all group members.

---

## 1. Group Roles & Responsibilities

| Contributor | Subsystem | Core Ownership |
| :--- | :--- | :--- |
| **Garv (Lead)** | `control_unit/` | Method 3 Hybrid Control Unit, Horizontal (77b) & Vertical (45b) ROMs, $\mu\text{Sequencer}$, Pipeline Bundles |
| **Aditya** | `simulator_core/` | 4-stage & 6-stage CPU Pipelines, Hazard/Forwarding Unit, Cycle-by-cycle Run Loop, TUI Debugger |
| **Atharva** | `alu/` | ALU execution, Flag Generation (`Z`, `N`, `C`, `V`), Branch Comparator, FPU Subsystem |
| **Paras** | `memory_safety/` | Hardware Stack Guard (`SP=r14`), 6-stage `MEMCHK` Firewall, Runtime Exceptions, ASCII Visualizer |
| **Saatvik** | `assembler_disassembler/` | 2-pass Assembler (`risc201asm`), Disassembler (`risc201dis`), Macro Expander, Opcode Verification |

---

## 2. Audit: Current Status vs. What's Missing

### Garv (`control_unit/`)
- **Status:** Baseline microcode definitions and Horizontal/Vertical assemblers functional; Method 3 architecture finalized.
- **What is Missing:**
  1. Reorganize directory into modular structure (`include/`, `src/`, `microcode/`).
  2. Emit stage-decoupled control signal bundles (`ExControl`, `ChkControl`, `MaControl`, `WbControl`).
  3. Complete microcode routines for all 34 opcodes (FPU ops, `BNDCHK`).
  4. Implement `freeze_if_of` pipeline stall handshake in $\mu\text{Sequencer}$ for multi-uop cracking (`CALL`, `RET`, `SWAP`, `BNDCHK`).
  5. Provide `IControlUnit` header interface for Aditya's CPU pipeline.

### Aditya (`simulator_core/`)
- **Status:** Basic `Memory`, `RegisterFile`, single-cycle CPU step loop, and test harnesses functional.
- **What is Missing:**
  1. True inter-stage pipeline latches (`IF_OF`, `OF_EX`, `EX_MA`, `MA_RW`, plus `MEMCHK` latches for 6-stage).
  2. RAW Data Forwarding Unit (routing `EX/MA` and `MA/RW` outputs to `EX` stage inputs).
  3. Load-Use hazard detector (inserting 1-cycle bubble in 4-stage, 2-cycle bubble in 6-stage).
  4. Pipeline freeze handling (holding `PC` and `IF_OF` when Garv's CU asserts `freeze_if_of`).
  5. Branch flush mechanism in `EX` stage (flushing younger stages on taken branch).
  6. Interactive TUI implementation using FTXUI.

### Atharva (`alu/`)
- **Status:** Basic arithmetic/logic functions implemented in `alu/ALU.cpp`.
- **What is Missing:**
  1. Strict flag generation (`Z`, `N`, `C`, `V`) on arithmetic and comparison instructions (`CMP`, `ADD`, `SUB`, etc.).
  2. Branch Comparator Unit: logic to evaluate condition flags against `BEQ`, `BNE`, `BGT`, `BLT`.
  3. Exception hook: signal `DIV_BY_ZERO` on divide/modulo by zero.
  4. FPU arithmetic operations (`FADD`, `FSUB`, `FMUL`, `FDIV`, `FCMP`) if FPU feature flag is active.

### Paras (`memory_safety/`)
- **Status:** Standalone `StackGuard` and ASCII visualizer demo working.
- **What is Missing:**
  1. 6-stage `MEMCHK` hardware interface: function `check_address(addr, mode, sp)` invoked during stage 4 before memory writes.
  2. Full exception hook wiring with `Memory` in `simulator_core` (`STACK_OVERFLOW`, `STACK_UNDERFLOW`, `MISALIGNED_ACCESS`).
  3. Connect ASCII Stack Visualizer to Aditya's per-cycle `StateView` snapshots.

### Saatvik (`assembler_disassembler/`)
- **Status:** Complete 2-pass assembler and disassembler operational.
- **What is Missing:**
  1. Confirm output hex word format exactly matches Aditya's loader (`%08x`, 32-bit words, one per line).
  2. Assembler macros for stack operations: expand `PUSH`, `POP`, `SPUSH`, `SPOP` into atomic store/load + SP adjustments.
  3. Verify round-trip assembly and execution on all 4 benchmark programs (`fac.asm`, `fib.asm`, `matmul.asm`, `sort.asm`).

---

## 3. Step-by-Step Procedures

### Step 1: Interface Alignment
- Garv publishes `control_signals.h` containing decoupled stage structs (`ExControl`, `ChkControl`, `MaControl`, `WbControl`).
- Atharva publishes `AluResult` with `{value, zero, negative, carry, overflow}` and branch evaluation function.
- Paras publishes `MemChkResult` for stage 4.

### Step 2: Datapath Pipeline Implementation
- Aditya replaces single-cycle loop with explicit pipeline register advancement:
  - 4-Stage: `step_4stage()` updating `[IF/OF] -> [EX] -> [MA] -> [RW]`.
  - 6-Stage: `step_6stage()` updating `[IF] -> [OF] -> [EX] -> [MEMCHK] -> [MA] -> [RW]`.
- Connects Forwarding Unit and Load-Use Hazard Unit.

### Step 3: Microcode Integration & Cracking
- Garv wires the $\mu\text{Sequencer}$ into `OF`:
  - 1-cycle ops emit signals directly to `OF_EX`.
  - Multi-cycle ops assert `freeze_if_of` and stream $\mu$-ops until completed.

### Step 4: Safety & Exception Verification
- Paras verifies stack boundary traps on recursive `fac.asm`.
- Aditya's debugger halts CPU safely and displays fault address and exception code.

### Step 5: TUI & Evaluation Matrix
- Aditya links FTXUI interface displaying pipeline stage occupancy, stalls, and register diffs.
- Team executes 4 benchmark programs across the 4 configurations (4-Stage vs 6-Stage $\times$ Horizontal vs Vertical) to collect CPI and cycle counts.
