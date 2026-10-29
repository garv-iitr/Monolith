# RISC201 Architecture & Module Contracts

Unified architectural contract for the 4-stage and 6-stage pipelined RISC201 processor with microprogrammed control unit.

## 1. Module Ownership

| Module | Owner | Deliverable & Interface |
| :--- | :--- | :--- |
| `control_unit/` | **Garv (Lead)** | Method 3 Microprogrammed Control Unit (Horizontal/Vertical ROMs, $\mu\text{Sequencer}$, $\mu\text{Op}$ Cracker, Pipeline Control Bundles) |
| `simulator_core/` | **Aditya** | CPU run-loop, Inter-stage Latches, Hazard Detection & Forwarding Unit, Interactive CLI/TUI Debugger |
| `alu/` | **Atharva** | 32/64-bit parameterized ALU, condition flags (`Z`, `N`, `C`, `V`), Branch Comparator, FPU extension |
| `memory_safety/` | **Paras** | Hardware Stack Guard (Full-descending `SP=r14`), Dedicated `MEMCHK` Stage (6-stage), ASCII Stack Visualizer |
| `assembler_disassembler/` | **Saatvik** | 2-pass Assembler (`risc201asm`), Disassembler (`risc201dis`), Instruction Encoder/Decoder |
| `isa_spec/` | **Shared** | Canonical RISC201 ISA definitions, 34 opcodes, instruction formats |

---

## 2. Pipeline Configurations

The processor supports two software-selectable hardware pipeline configurations:

### A. 4-Stage Classic RISC Pipeline
```
[ IF/OF ] --------> [ EX ] --------> [ MA ] --------> [ RW ]
Fetch + Decode       Execute + ALU     Memory Access   Register Writeback
(Merged Front-End)   Branch Eval       Inline Stack Guard
```
- **Front-End:** Unified Fetch + Decode in Stage 1.
- **Branch Penalty:** 1 cycle on taken branch (resolved in EX).
- **Load-Use Penalty:** 1 cycle stall bubble.
- **Stack Safety:** Inline bounds check executed in parallel with MA.
- **Schematic:** [`docs/pipeline_4stage_architecture.svg`](pipeline_4stage_architecture.svg)

### B. 6-Stage Fault-Tolerant Super-Pipelined Architecture
```
[ IF ] ------> [ OF ] ------> [ EX ] ------> [ MEMCHK ] ------> [ MA ] ------> [ RW ]
Fetch           Decode +      Execute +      Hardware Memory     Data Memory    Register
                Reg Read      Branch Eval    Safety Firewall     Access         Writeback
```
- **Safety Stage:** Dedicated `MEMCHK` stage between EX and MA (validates stack boundaries, alignment, and segment permissions before asserting memory write-enable).
- **Branch Penalty:** 2 cycles on taken branch (resolved in EX; flushes IF and OF).
- **Load-Use Penalty:** 2 cycles stall bubble.
- **Clock Frequency:** Higher clock frequency scaling due to isolated stage critical paths.
- **Schematic:** [`docs/pipeline_microcode_architecture.svg`](pipeline_microcode_architecture.svg)

---

## 3. Control Unit: Method 3 (Hybrid $\mu$-Op Cracking)

The Control Unit operates in the **OF (Decode)** stage and cracks incoming RISC instructions into pipeline control signals:

```
Instruction (from IF)
       │
       ▼
┌──────────────┐      Single-cycle RISC op (ADD, SUB, LD, ST, etc.)
│ Opcode Class │ ────────────────────────────────────────────────────────► Direct 1-Cycle Pipeline Emission
└──────┬───────┘
       │ Multi-cycle complex op (CALL, RET, SWAP, BNDCHK)
       ▼
┌──────────────┐
│ µSequencer   │ ──► Asserts `freeze_if_of` (holds PC and IF/OF latches)
│ State Machine│ ──► Sequentially emits µ-op bundle stream into EX stage until completed
└──────────────┘
```

### Dual Control Memory Modes
1. **Horizontal Mode:** 77-bit wide unencoded Control Memory word directly driving datapath wires without combinational decode delay ($T_{dec} = 0$).
2. **Vertical Mode:** 45-bit compact encoded Control Memory word expanded via combinational $\mu$-Decoder ($41.6\%$ ROM storage reduction).

### Inter-Stage Control Signal Bundles
Control signals are partitioned into stage-specific bundles propagated along pipeline registers:
- `ExControl`: `alu_op`, `operand_a_src`, `operand_b_src`, `branch_cond`, `set_flags`
- `ChkControl`: `memchk_enable`, `chk_mode` (Stack / Heap / Boundary)
- `MaControl`: `mem_read_en`, `mem_write_en`, `mem_width`
- `WbControl`: `reg_write_en`, `reg_write_src` (ALU_OUT / MEM_DATA / PC_PLUS4)

---

## 4. Hazard & Exception Contracts

1. **Data Hazards (RAW):** Forwarding paths from `EX/MEM` and `MEM/WB` to `EX` input muxes.
2. **Load-Use Hazard:** Detected when `EX.mem_read_en == true` and `EX.rd == OF.rs1 || OF.rs2`. Injects a 1-cycle bubble (4-stage) or 2-cycle bubble (6-stage).
3. **Control Hazards (Branch/Jump):** Branch evaluated in EX stage. If taken, target PC is loaded and younger stages are flushed (`bubble = true`).
4. **Micro-Op Hazard:** When $\mu\text{Sequencer}$ cracks multi-cycle instructions, `freeze_if_of = true` freezes Stage 1/2 while successive $\mu$-ops drain into Stage 3 (EX).
5. **Exceptions:** `RuntimeException` (`DIV_BY_ZERO`, `STACK_OVERFLOW`, `STACK_UNDERFLOW`, `BAD_MEM_ACCESS`, `ILLEGAL_INSTR`). In 6-stage, `MEMCHK` halts execution before illegal writes reach Data Memory.

---

## 5. Build & Verification

```bash
# Configure and build
cmake -B build -G Ninja
cmake --build build

# Run all test suites
ctest --test-dir build --output-on-failure
```
