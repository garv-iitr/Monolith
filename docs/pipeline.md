# RISC201 Pipeline Specification

Hardware specification for the 4-stage and 6-stage pipelined datapaths using Method 3 (Hybrid Micro-Op Cracking).

---

## 1. Pipeline Stage Comparison

| Feature | 4-Stage Pipeline | 6-Stage Pipeline |
| :--- | :--- | :--- |
| **Stage Breakdown** | `[IF] -> [OF] -> [EX] -> [MA/RW]` | `[IF] -> [OF] -> [EX] -> [MEMCHK] -> [MA] -> [RW]` |
| **Front-End Architecture** | Decoupled Fetch (`IF`) and Decode (`OF`) | Identical Decoupled Fetch (`IF`) and Decode (`OF`) |
| **Back-End Architecture** | Combined Memory Access & Register Writeback (`MA/RW`) | Decoupled Safety Firewall (`MEMCHK`), Memory (`MA`), and Writeback (`RW`) |
| **Memory Firewall** | Inline hardware check in parallel with `MA/RW` | Dedicated hardware pipeline stage (`MEMCHK`) before `MA` |
| **Branch Penalty** | 2 cycles (EX flushes IF and OF) | 2 cycles (EX flushes IF and OF) |
| **Load-Use Penalty**| 1 stall cycle (load completes and writes back in MA/RW) | 2 stall cycles (load completes in MA, writes back in RW) |
| **Clock Frequency ($F_{\text{max}}$)** | Baseline frequency (MA + RegFile write critical path) | Higher frequency (short, decoupled stage critical paths) |
| **Optimal Use Case** | Low-latency embedded / compact footprint | High-frequency execution with hardware memory safety firewall |

---

## 2. Stage-by-Stage Functionality

### 4-Stage Variant (`[IF] -> [OF] -> [EX] -> [MA/RW]`)
1. **`IF` (Instruction Fetch):**
   - Fetches 32-bit instruction from memory at `PC`.
   - Increments `PC = PC + 4` (unless stalled or branch taken).
   - Latches instruction and `PC` into `IF/OF`.
2. **`OF` (Operand Fetch & Decode):**
   - Control Unit decodes instruction format and peeks control bundle.
   - Evaluates Load-Use hazards via `HazardDetectionUnit`.
   - Multi-cycle instructions (`CALL`, `RET`) assert `freeze_if_of` and stream $\mu$-ops.
   - Reads source operands (`rs1`, `rs2`) from Register File (GPR or FPR).
   - Latches decoded operands and control bundle into `OF/EX`.
3. **`EX` (Execute):**
   - Resolves RAW data hazards via `ForwardingUnit` from `EX/MA_RW`.
   - ALU executes integer arithmetic/logic or calculates effective memory address (`base + offset`).
   - FPU executes IEEE-754 single-precision float operations (`FADD`, `FSUB`, `FMUL`, `FDIV`, `FMOV`, `FCMP`).
   - Branch comparator evaluates condition (`B`, `BEQ`, `BNE`, `BGT`, `BLT`, `CALL`, `RET`).
   - Latches result, store value, and control bundle into `EX/MA_RW`.
4. **`MA/RW` (Memory Access & Register Writeback):**
   - Memory Safety Firewall verifies alignment and stack limits (if enabled).
   - Reads or writes Data Memory (`LD`, `ST`, `FLD`, `FST`).
   - Immediately commits result (`ALU_OUT`, `MEM_DATA`, or return `PC`) to Register File (`regs_[rd]` / `fregs_[rd]`).
   - Handles `HALT` and increments retired instruction counter.

### 6-Stage Variant (`[IF] -> [OF] -> [EX] -> [MEMCHK] -> [MA] -> [RW]`)
1. **`IF` (Instruction Fetch):** Latching `PC`, reading instruction memory at `PC`, incrementing `PC + 4`.
2. **`OF` (Operand Fetch & Decode):** CU opcode decoding, register file read, immediate expansion, $\mu$-op cracking.
3. **`EX` (Execute):** ALU/FPU computation, effective address arithmetic, branch condition evaluation.
4. **`MEMCHK` (Memory Safety Firewall):** Dedicated hardware unit validates effective address, stack limits, and word alignment before memory access. Suppresses write strobe if violated.
5. **`MA` (Memory Access):** Performs verified memory read or write.
6. **`RW` (Register Writeback):** Commits final result to Register File and retires instruction.

---

## 3. Synchronous Inter-Stage Latches

```text
4-Stage Latches:   [PC] ──► [IF/OF] ──► [OF/EX] ──► [EX/MA_RW] ──► (RegFile / Memory Commit)
6-Stage Latches:   [PC] ──► [IF/OF] ──► [OF/EX] ──► [EX/CHK] ──► [CHK/MA] ──► [MA/RW]
```

### Control Bundles Propagated Down Latches:
- **`ExControl`** (consumed in EX): `alu_op`, `operand_a_src`, `operand_b_src`, `set_flags`, `branch_cond`, `is_branch`, `is_call`, `is_ret`, `is_fpu`, `fpu_op`.
- **`ChkControl`** (consumed in MEMCHK / MA/RW): `memchk_enable`, `chk_mode`.
- **`MaControl`** (consumed in MA / MA/RW): `mem_read`, `mem_write`, `is_fpu`.
- **`WbControl`** (consumed in RW / MA/RW): `reg_write_en`, `reg_write_src`, `dest_reg`, `is_fpu`.

---

## 4. Hazard & Bypass Mechanics

1. **RAW Data Hazards & Forwarding:**
   - **4-Stage:** Result from `EX/MA_RW` forwarded directly to ALU/FPU input operands in `EX`.
   - **6-Stage:** Results forwarded from `EX/CHK`, `CHK/MA`, and `MA/RW` to `EX`.
2. **Load-Use Data Hazards:**
   - **4-Stage (1 Bubble):** When load is in `OF/EX`, dependent instruction in `OF` stalls for 1 cycle. In next cycle, load finishes `MA/RW` and writes to register file, allowing dependent instruction to proceed with 0 extra delay.
   - **6-Stage (2 Bubbles):** When load is in `OF/EX` or `EX/CHK`, dependent instruction in `OF` stalls for 2 cycles until memory read completes in `MA`.
3. **Control Hazards (Branches):**
   - Branch outcome resolved in `EX` (Stage 3).
   - On taken branch: `PC` updated to branch target. Younger speculative stages (`IF/OF` and `OF/EX`) flushed (2 cycles penalty).
4. **Microcode Expansion Hazard:**
   - Multi-$\mu$-op instructions (`CALL`, `RET`) assert `freeze_if_of`.
   - Front-end stalls while sequencer injects consecutive micro-operations down the pipeline.
