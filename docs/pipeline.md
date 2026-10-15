# RISC201 Pipeline Specification

Concise hardware specification for the 4-stage and 6-stage pipelined datapaths.

---

## 1. Pipeline Stage Comparison

| Feature | 4-Stage Pipeline | 6-Stage Pipeline |
| :--- | :--- | :--- |
| **Stage Breakdown** | `[IF/OF] -> [EX] -> [MA] -> [RW]` | `[IF] -> [OF] -> [EX] -> [MEMCHK] -> [MA] -> [RW]` |
| **Front-End** | Unified Fetch + Decode | Split Fetch and Decode |
| **Memory Firewall** | Inline check in parallel with MA | Dedicated hardware stage (`MEMCHK`) before MA |
| **Branch Penalty** | 1 cycle (EX flushes IF/OF) | 2 cycles (EX flushes IF and OF) |
| **Load-Use Penalty**| 1 stall cycle | 2 stall cycles |
| **Clock Scaling** | Baseline frequency | Higher frequency (shorter stage critical path) |
| **Vector Diagram** | [`pipeline_4stage_architecture.svg`](pipeline_4stage_architecture.svg) | [`pipeline_microcode_architecture.svg`](pipeline_microcode_architecture.svg) |

---

## 2. Stage-by-Stage Functionality

### 4-Stage Variant
1. **`IF/OF` (Fetch & Decode):**
   - Fetches instruction from Memory at `PC`.
   - Microprogrammed Control Unit cracks opcode into control bundles.
   - Reads source operands (`rs1`, `rs2`) from Register File.
2. **`EX` (Execute):**
   - ALU executes operation (`ADD`, `SUB`, etc.) or calculates effective memory address (`base + offset`).
   - Branch comparator evaluates condition (`BEQ`, `BNE`, `BGT`, `BLT`).
3. **`MA` (Memory Access):**
   - Reads or writes Data Memory (`LD`, `ST`).
   - Inline stack guard checks `SP` boundary.
4. **`RW` (Register Writeback):**
   - Writes `ALU_OUT`, `MEM_DATA`, or return `PC` into `rd`.

### 6-Stage Variant
1. **`IF` (Instruction Fetch):** Latching `PC`, reading instruction cache/memory.
2. **`OF` (Operand Fetch & Decode):** CU opcode decoding, register file read, immediate expansion.
3. **`EX` (Execute):** ALU computation, effective address arithmetic, branch condition evaluation.
4. **`MEMCHK` (Memory Safety Firewall):** Paras's hardware unit validates effective address, stack limits, and word alignment. Suppresses write strobe if violated.
5. **`MA` (Memory Access):** Performs verified memory read or write.
6. **`RW` (Register Writeback):** Commits final result to Register File.

---

## 3. Pipeline Registers & Latches

```text
4-Stage Latches:   [PC] ──► [IF/OF_EX] ──► [EX_MA] ──► [MA_RW]
6-Stage Latches:   [PC] ──► [IF_OF] ──► [OF_EX] ──► [EX_CHK] ──► [CHK_MA] ──► [MA_RW]
```

### Control Signals Propagated Across Latches:
- **`ExControl`** (consumed in EX): `alu_op`, `operand_a_src`, `operand_b_src`, `set_flags`, `branch_cond`.
- **`ChkControl`** (consumed in MEMCHK): `memchk_enable`, `chk_mode`.
- **`MaControl`** (consumed in MA): `mem_read`, `mem_write`, `mem_width`.
- **`WbControl`** (consumed in RW): `reg_write_en`, `reg_write_src`, `dest_reg`.

---

## 4. Hazard Handling

1. **RAW Data Hazards:**
   - Forwarding Unit routes results directly from `EX/MA` and `MA/RW` to ALU inputs in `EX`.
2. **Load-Use Data Hazards:**
   - When an instruction in `EX` is a `LD` and its `rd` matches the `rs1` or `rs2` of the instruction in `OF`:
     - 4-Stage: Injects **1 bubble**; holds `IF/OF` and `PC`.
     - 6-Stage: Injects **2 bubbles**; holds `IF`, `OF`, and `PC`.
3. **Control Hazards (Branches):**
   - Branch outcome resolved in `EX`.
   - On taken branch: `PC` updated to branch target. Younger pipeline registers flushed.
4. **Microcode Expansion Hazard:**
   - Multi-uop instructions (`CALL`, `RET`, `SWAP`, `BNDCHK`) assert `freeze_if_of`.
   - Front-end stalls while $\mu\text{Sequencer}$ injects consecutive $\mu$-ops into `EX`.
