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

* **30 internal registers**: Sarangi's 19 (Table 9.7) + FPU (`fpSrc, fpData,
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

## Dry-run example: `add r3, r1, r2`

Walk the `master_full_cu.drawio`:

1. **μpc=0** — ROM word 0 asserts `fetch_ir` (dashed → instruction memory):
   `ir ← [pc]`.
2. **μpc=1** — `mdecode`: decode unit slices `ir`; `rd, rs1, rs2` populate
   combinationally.
3. **μpc=2** — `madd pc, 4`: pc drives the write bus, transfer mux selects
   μadder, read bus = pc + 4, pc latches it.
4. **μpc=3** — `mswitch`: opcode (from `ir`) enters the switch unit; μmux picks
   its output; μpc = entry point of the ADD routine.
5. `mmov regSrc, rs1, <read>`: rs1 → write bus → read bus → regSrc; args carry
   `regfile/read`; register file reads GPR[rs1] into regVal.
6. `mmov A, regVal`: regVal → write bus → read bus → A.
7. `mbeq I, 1, .imm`: I drives the write bus; comparator sets isMBranch =
   (I == μimm=1). Here I=0 → not taken → next.
8. `mmov regSrc, rs2, <read>` then `mmov B, regVal, <add>`: B latches regVal and
   args carry `alu/add`; ALU computes aluResult = A + B, sets flags.
9. `mmov regSrc, rd` then `mmov regData, aluResult, <write>`: register file
   stores aluResult into GPR[rd].
10. `mb .begin`: μmux selects fetch (addr 0); next instruction starts.
