# Control Unit Diagrams — Draw.io Set

Six diagrams for the presentation, mirroring Sarangi, *Basic Computer Architecture*
ch. 9 (figs 9.22, 9.23, 9.25, 9.27). Open the `.drawio` files in draw.io
(app.diagrams.net), then tweak routing/labels to taste.

## Files

| File | Mirrors | Content |
| --- | --- | --- |
| `master_full_cu.drawio` | fig 9.22 + 9.23 + 9.27 combined | **The dry-run diagram.** Every internal register (all 30), every functional unit, the μCU (ROM, μpc, sequencer, switch), the shared bus, and every signal (opcode, args, isMBranch, branchTarget, μimm, fetch_ir, control signals). Use this one to mentally execute any instruction. |
| `datapath_overview.drawio` | fig 9.22 | Clean overview: fetch → decode → units over the shared bus, for slides. |
| `shared_bus.drawio` | fig 9.23 | The bus internals: write bus, transfer mux, μadder, μimm, isMBranch comparator, read bus. |
| `vertical_ucu.drawio` | fig 9.25 | Vertical μCU: 45-bit words, μ-decoder stage, μpc, switch, μfetch mux. |
| `horizontal_ucu.drawio` | fig 9.27 | Horizontal μCU: 65-bit words, no decode stage, M1 + μmux, isMBranch. |
| `stage_mapping.drawio` | — (RISC201 extension) | StageClass → 4-stage vs 6-stage names. |

## Shape conventions (match the Sarangi screenshots)

| Hardware | draw.io shape | Style |
| --- | --- | --- |
| Internal register (pc, ir, A, B, mar, …) | Rounded rectangle | fill `#F8CECC` (light pink), border `#B85450` |
| Functional unit (ALU, register file, decode, execute, FPU) | Rounded rectangle | fill `#F5B7B1` (pink), border `#B85450` |
| Memory (instruction memory, data memory) | Rounded rectangle | fill `#DAE8FC` (light blue), border `#6C8EBF` |
| Control memory ROM | Rectangle | fill `#B85450` (dark red), white text |
| Multiplexer (μmux, M1, transfer mux) | **Electrical → Abstract → MUX 2:1** (or Misc → Trapezoid) | pink; μmux is `flipH` so its output faces μpc |
| Adder / comparator (+, =, μadder) | Ellipse | pink (adder) / light blue (comparator) |
| Constant ("1", "0 (fetch)") | Rectangle | grey `#D5D5D5` |
| Switch unit | Ellipse | pink |
| Bus bar (write/read/shared bus) | Rounded rectangle | orange `#FFE6CC` / teal `#B0E0E6` |
| Containers (μCU, datapath) | Rounded rectangle, 50% opacity | light blue |
| Data path arrow | Orthogonal edge | blue `#6C8EBF`, `strokeWidth=3`, block arrow |
| Control signal | Orthogonal edge | blue, **dashed**, open arrow |

> If a mux renders as a plain box in your draw.io version, select it and pick
> **Misc → Trapezoid** (rotate/flip so inputs face the source side), or
> **Electrical → Abstract → MUX 2:1**.

## Facts encoded in the diagrams

* **30 internal registers**: Sarangi's 19 (Table 9.7) + `flags.N` (for `BLT`) + FPU (`fpSrc, fpData,
  fpVal, fA, fB, fpuResult, fcc`) + memory-check (`memChkAddr, memChkStatus`).
* **Shared bus** = write bus → transfer mux → read bus. Mux inputs: write bus,
  μimm (sign-extended 12-bit), μadder output (write bus + μimm).
* **isMBranch** = (write bus == μimm), produced by the "=" comparator; it is the
  select of M1 and implements `mbeq`.
* **μfetch mux rules** (Table 9.9): `mloadIR/mdecode/mmov/mmovi/madd` → next;
  `mswitch` → switch unit output (entry point of opcode from `ir`); `mb` →
  branchTarget; `mbeq` → branchTarget if isMBranch else next; end of routine →
  fetch (addr 0).
* **Preamble** (control-memory addr 0–3): `mloadIR` → `mdecode` → `madd pc, 4` →
  `mswitch`. Every routine ends with `mb .begin`.
* **branchTarget** = pc + (offset26 << 2); since the preamble has already done
  `pc += 4`, the *effective* target = (instr addr + 4) + offset·4.
* **Encodings**: horizontal 65 bits (33 control signals + 12 imm + 10 target +
  10 args); vertical 45 bits (3 type + 5 src + 5 dest + 12 imm + 10 target +
  10 args) plus the μ-decoder.

## Dry-run Walkthroughs (using `master_full_cu.drawio`)

### 1. Preamble (common to all instructions, μpc = 0..3)
1. **μpc=0 (`mloadIR`)**: ROM word 0 asserts `fetch_ir` (dashed line → Instruction Memory). Instruction at `pc` is latched into `ir`.
2. **μpc=1 (`mdecode`)**: Decode unit combinational logic splits `ir` into fields `I, rd, rs1, rs2, immx, branchTarget`.
3. **μpc=2 (`madd pc, 4`)**: `pc` drives write bus, transfer mux selects `μadder` (`write bus + 4`), read bus drives `pc + 4` back into `pc`.
4. **μpc=3 (`mswitch`)**: Opcode from `ir` enters the switch unit. Switch output drives `μmux` to set `μpc` to the routine entry point.

---

### 2. R-Type ALU: `ADD r3, r1, r2`
5. `mmov regSrc, rs1, <read>`: `rs1` → write bus → read bus → `regSrc`. Register file reads `GPR[rs1]` into `regVal`.
6. `mmov A, regVal`: `regVal` → write bus → read bus → `A`. ALU input A receives `GPR[rs1]`.
7. `mbeq I, 1, .imm`: `I` drives write bus. Comparator checks `I == 1`. Since `I=0`, `isMBranch=0` → fall through to next μpc.
8. `mmov regSrc, rs2, <read>`: `rs2` → write bus → read bus → `regSrc`. Register file reads `GPR[rs2]` into `regVal`.
9. `mmov B, regVal, <alu/add>`: `regVal` → write bus → read bus → `B`. Control args trigger `alu/add`. ALU computes `aluResult = A + B` and updates `flags.E, flags.GT, flags.N`.
10. `mmov regSrc, rd`: `rd` → write bus → read bus → `regSrc`.
11. `mmov regData, aluResult, <regfile/write>`: `aluResult` → write bus → read bus → `regData`. Register file writes `aluResult` into `GPR[rd]`.
12. `mb .begin`: `μmux` selects fetch (addr 0).

---

### 3. Immediate Move: `MOV r1, #5` (Format 2, `AluOp::PASS_B`)
5. `mbeq I, 1, .imm`: `I` drives write bus. Since immediate bit is 1, comparator sets `isMBranch=1`. `M1` branches to `.imm`.
6. `mmov B, immx, <alu/pass_b>`: `immx` (#5) → write bus → read bus → `B`. Control args trigger `alu/pass_b`. ALU passes B directly to `aluResult = 5`.
7. `mmov regSrc, rd`: `rd` (r1) → write bus → read bus → `regSrc`.
8. `mmov regData, aluResult, <regfile/write>`: `aluResult` (5) → write bus → read bus → `regData`. Register file writes 5 into `GPR[r1]`.
9. `mb .begin`: `μmux` selects fetch (addr 0).

---

### 4. Memory Load: `LD r1, 4[r2]` (4-stage vs 6-stage)
5. `mmov regSrc, rs1, <read>`: `rs1` (r2) → `regSrc`. Register file reads base address into `regVal`.
6. `mmov A, regVal`: `regVal` → `A`.
7. `mmov B, immx, <alu/add>`: `immx` (4) → `B`. ALU computes `aluResult = A + B` (effective address).
8. *(6-Stage only)* `mmov memChkAddr, aluResult, <chk/check>`: Effective address is validated by Memory Check unit against stack/memory bounds. (In 4-stage, this check is folded).
9. `mmov mar, aluResult, <mem/read>`: Effective address loaded into `mar`. Data Memory reads `[mar]` into `ldResult`.
10. `mmov regData, ldResult`: `ldResult` → `regData`.
11. `mmov regSrc, rd, <regfile/write>`: `rd` (r1) → `regSrc`. Register file writes loaded word into `GPR[r1]`.
12. `mb .begin`: Jumps back to fetch.

---

### 5. Conditional Branch: `BLT label`
5. `mbeq flags.N, 1, .taken`: `flags.N` drives write bus. Comparator checks `write bus == 1`.
   - **If N==1 (taken)**: `isMBranch=1`. `M1` selects `branchTarget` (`.taken`). Next cycle: `mmov pc, branchTarget` latches calculated PC into `pc`. Then `mb .begin`.
   - **If N==0 (not taken)**: `isMBranch=0`. Falls through to `mb .begin`. `pc` retains its incremented value (`pc + 4`).

---

### 6. Subroutine Call: `CALL target`
5. `mmovi regSrc, 14, <read>`: `regSrc` gets 14 (SP). Register file reads current SP into `regVal`.
6. `madd regVal, -4`: `regVal` drives write bus, `μadder` decrements by 4, read bus latches new SP into `regVal`.
7. `mmov mar, regVal`: New SP loaded into `mar`.
8. *(6-Stage only)* `mmov memChkAddr, mar, <chk/check>`: SP address boundary verified.
9. `mmov regData, regVal, <regfile/write>`: SP register (r14) updated with new SP value.
10. `mmov mdr, pc, <mem/write>`: Return address `pc` driven into `mdr`. Data Memory writes `mdr` at `[mar]`.
11. `mmov pc, branchTarget`: Subroutine target address loaded into `pc`.
12. `mb .begin`: Jumps back to fetch first instruction of subroutine.
