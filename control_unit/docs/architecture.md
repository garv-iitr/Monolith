# RISC201 Microprogrammed Control Unit

Owner: Garv. Reference: Sarangi, *Basic Computer Architecture*, ch. 9
(§9.4–9.8), plus the RISC201 ISA in `isa_spec/isa.h` / `ISA.md`.

This module implements the microprogrammed control unit and the microprogrammed
datapath, in both a **horizontal** and a **vertical** encoding, under both a
**4-stage** and a **6-stage** stage layout.

## 1. What a control word is

A control word is a bundle of signal lines issued for one micro-cycle:

| Field | Meaning |
| --- | --- |
| `write_src` | the single internal register driving the **write bus** |
| `read_mask` | every internal register latching the **read bus** |
| `transfer_sel` | the transfer mux: `write_bus` / `muimm` / `muadder` |
| `alu_op`, `reg_op`, `mem_op`, `fpu_op`, `memchk_op` | functional-unit args |
| `set_flags`, `fpu_set_fcc` | commit flags |
| `fetch_ir` | `mloadIR`: `ir <- instruction_memory[pc]` |
| `next_addr`, `jump_target` | micro-sequencer control |
| `halt` | stop the machine |

This is `ControlSignals` (`common/control_signals.h`). Both encodings converge
on it, so the datapath and execute unit are shared.

## 2. Shared bus datapath (Sarangi §9.5–9.7)

```
 registers --(write bus)--> [transfer mux] --(read bus)--> registers
                              ^       ^
                           muimm   micro-adder (write bus + muimm)
```

* `common/shared_bus.h` — write bus, micro-adder, `isMBranch` (write bus ==
  muimm), transfer mux, read bus.
* `common/mux.h` — parameterised `Mux<T,N>` and one-hot `Decoder<N>`.
* `common/internal_registers.h/.cpp` — all 30 microcode-visible registers:
  Sarangi's 19 (Table 9.7) plus the FPU (`fpSrc/fpData/fpVal/fA/fB/fpuResult/fcc`)
  and memory-check (`memChkAddr/memChkStatus`) extensions. `get`/`set` give the
  width-agnostic access used by `mmov`.

### Functional units

This module executes the units it owns; for the others it only emits args on
the bus (the contract lives in `common/control_signals.h`):

| Unit | File | Owner |
| --- | --- | --- |
| decode | `datapath/decode_unit.h` | control unit (Garv) |
| register file | `datapath/register_file_unit.h` | control unit (Garv) |
| memory | `datapath/memory_unit.h/.cpp` | control unit (Garv) |
| ALU | -- (`AluOp` args) | Atharva (`alu/`) |
| FPU | -- (`FpuOp` args) | Atharva |
| memory check | -- (`MemChkOp` args) | Paraas (`memory_safety/`) |

The ALU/FPU/memory-check units latch their results from the shared internal
registers (`A/B/aluResult`, `fpSrc/fpData/...`, `memChkAddr/memChkStatus`) when
their implementations are integrated.

`datapath/datapath.h/.cpp` is the **execute unit**: one `execute()` is one
micro-cycle. Inputs latched this cycle are visible to the units in the *same*
cycle, which is what lets a microinstruction fuse a transfer with a
functional-unit op (e.g. `mmov B, regVal, <add>`).

## 3. Microinstructions (Sarangi §9.6)

The eight microinstructions are `MicroOp` in `isa_spec/isa.h`:
`mloadIR, mdecode, mswitch, mmov, mmovi, madd, mbeq, mb`.
Preamble (shared by every instruction):

```
mloadIR          ; ir <- [pc]
mdecode          ; field registers populate
madd pc, 4       ; pc <- pc + 4
mswitch          ; dispatch on the opcode
```

## 4. Authoring once, encoding twice

Microcode is written once in a symbolic micro-assembly language
(`microcode/micro_asm.h`) and placed by `microcode/routine_layout.h`. The
microprogram itself is `microcode/microcode_program.cpp` — one routine per
RISC201 opcode (all 34). Two assemblers translate the same listing:

* `microcode/h_assembler.cpp` -> `ControlMemory<HorizontalMicroWord>` (signals
  embedded; no decode stage; `horizontal/h_codec.h` is a wire-through).
* `microcode/v_assembler.cpp` -> `ControlMemory<VerticalMicroWord>` (encoded;
  `vertical/v_decoder.cpp` reconstructs the signals).

Both feed the shared `common/micro_control_unit.h`, which owns the microPC,
the micro-fetch mux and the `switch` unit (`common/micro_sequencer.h`).

### Encodings

* Horizontal (Sarangi fig. 9.26 scaled to 30 internal registers): 77 bits — 45
  control signals + 12-bit imm + 10-bit target + 10-bit args. Larger storage, no decoder.
* Vertical (Sarangi fig. 9.24): 45 bits — `type[3] src[5] dest[5] imm[12]
  target[10] args[10]`. Smaller storage, needs the micro-decoder.

## 5. 4-stage vs 6-stage

A micro-step is tagged with a `StageClass`; the concrete stage name comes from
`StageLayout` (`common/control_signals.h`):

| StageClass | 4-stage | 6-stage |
| --- | --- | --- |
| FETCH / DECODE / SEQUENCE | IF/OF | IF / OF |
| EXECUTE | EX | EX |
| CHECK | MA | **MEMCHK** |
| MEMORY | MA | MA |
| WRITEBACK | RW | RW |

So the 4-stage layout merges IF with OF; the 6-stage layout exposes a dedicated
**MEMCHK** stage that validates the effective address (bounds + word alignment)
before the memory access. The checker itself is Paraas's memory-check unit; the
microcode issues `MemChkOp::CHECK` args and reads the latched `memChkStatus`.

## 6. Build and run

```
cmake -S . -B build
cmake --build build --target risc201-cu-demo
./build/risc201-cu-demo
```

The demo checks, in every configuration, that all 34 opcodes have a microcode
routine and an entry point in both control memories, and runs programs that
exercise this module's datapath (immediate/register moves, flag-driven
branches, call/ret, illegal-opcode halt) through all four configurations,
printing a per-micro-cycle trace for the first one and self-checking that all
four reach identical architectural state.

## 7. Assumptions / open items

1. **ISA.md is canonical.** `isa_spec/isa.h` was finished to match `ISA.md`
   (6-bit opcodes, formats 1–8). `simulator_core`'s decoder and `tests/` still
   use the older SimpleRISC field layout and need a follow-up migration.
2. **Memory is byte-addressed, word-granular.** PC advances by 4, branch
   offsets are `<< 2`, load/store addresses are byte addresses; misaligned or
   out-of-range accesses fault (`memchk` in 6-stage, the memory unit otherwise).
3. **CALL/RET use the full-descending stack** (`sp = r14`), per `ISA.md`
   ("Push PC" / "Pop PC") and Sarangi Examples 132/133 — not a dedicated `ra`.
4. **Instruction fetch is one cycle** (`mloadIR` reads the instruction memory
   directly), matching the book's microinstruction count; data access goes
   through `mar`/`ldResult`.
5. `BNDCHK` performs the same presence check as the 6-stage MEMCHK stage.
6. `PUSH/POP/SPUSH/SPOP` are assembler macros (per `ISA.md`), not opcodes, so
   they have no microcode routine.
