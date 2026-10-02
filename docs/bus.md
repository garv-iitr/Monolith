# RISC201 Bus & Integration Architecture

Single source of truth for how the five modules connect and how data flows from
assembly text to the TUI. Read this before touching the datapath, control unit,
or engine seam. `isa_spec/isa.h` stays the encoding contract; this document is
the *flow* contract.

## 1. Purpose

We simulate a processor, so the engine is structural: components do pure work,
and a single **Bus** carries every value that moves between stages each cycle.
The Bus is the one place to inspect "what is on the wires right now", which is
exactly what the TUI/debugger and the evaluator render per cycle.

Two distinct interfaces, kept separate on purpose:

- **Component interfaces** — pure methods on `Memory`, `RegisterFile`, `Alu`,
  `ControlMemory`. The bus does **not** replace these; it is fed by them.
- **The Bus** — the datapath interconnect: a struct of named signals that stages
  read from and write back to.

## 2. Canonical data flow

```
 assembly.s
     |  Saatvik (assembler)
     v
 machine.hex   (32-bit words, one per line, 8 hex digits)
     |  Aditya (loader: Memory::load_words(0, words))
     v
 +------------------  RISC201 run loop (Aditya)  ------------------+
 |                                                                 |
 |  IF   pc -> Memory::read_word(pc) -> bus.ir                     |
 |  OF   decode(bus.ir) -> bus.decoded  (opcode + fields)          |
 |        |                                                       |
 |        v                                                       |
 |  CU   MicroSequencer: microPC = ControlMemory::entry(opcode)    |
 |        (shared IF+OF microinstruction at addr 0 -> DISPATCH)    |
 |        each micro-step emits bus.signals (ControlSignals)       |
 |  EX   operands per signals.operand_a/b_src -> bus.operand_a/b   |
 |        Alu::execute(signals.alu_op, a, b) -> bus.alu_result     |
 |  MA   if signals.mem_op: Memory::read/write -> bus.mem_data     |
 |  RW   if signals.reg_write_en: RegisterFile::write(rd, src)     |
 |  PC   signals.pc_control -> bus.next_pc -> set_pc               |
 |                                                                 |
 |  exceptions: components throw RuntimeException -> engine halts  |
 +-----------------------------------------------------------------+
     |  Aditya (StateView snapshot each cycle)
     v
 TUI / debugger  (microPC + bus + registers + memory)
     ^
     |  Paras (stack visualizer reads the same StateView snapshot)
     v
 evaluation matrix (Aditya + Paras)
```

The **control unit sequences microinstructions** (Garv) and emits `ControlSignals`;
the **datapath applies them** (Atharva); the **engine coordinates and holds state**
(Aditya); the **assembler produces the words** (Saatvik); **memory safety watches
the boundaries** (Paras).

## 3. The Bus

Proposed shared header (co-owned, frozen at Milestone 0). Lives where all four
owners can include it; include chain is `bus.h -> alu.h + decoder.h +
control_signals.h`.

```cpp
struct Bus {
  Word            ir            = 0;   // IF: instruction word
  DecodedInstruction decoded{};        // OF: decoded fields
  Word            operand_a     = 0;   // EX: A input (rs1 | imm | pc)
  Word            operand_b     = 0;   // EX: B input (rs2 | imm | 0)
  Word            alu_result    = 0;   // EX: ALU output
  AluResult       alu_flags{};         // EX: {zero, negative, carry, overflow}
  Word            mem_addr      = 0;   // MA: effective address
  Word            mem_data      = 0;   // MA: read result / write value
  Word            pc            = 0;   // current PC
  Word            next_pc       = 0;   // PC after update
  ControlSignals  signals{};           // active control word (from CU)
  bool            branch_taken  = false;
};
```

| Signal | Written by | Consumed by |
| --- | --- | --- |
| `ir` | IF (engine) | OF (decoder) |
| `decoded` | OF (`decode`) | EX, CU dispatch, TUI |
| `operand_a/b` | EX (from `operand_a_src`/`operand_b_src`) | ALU |
| `alu_result` | ALU (`Alu::execute`) | RW, MA (address), TUI |
| `alu_flags` | ALU | branch resolution, RW flag write, TUI |
| `mem_addr` | EX/MA | `Memory::read_word`/`write_word` |
| `mem_data` | MA (memory read) or EX (store value) | RW |
| `pc` / `next_pc` | engine | IF, PC update |
| `signals` | CU (`MicroSequencer::current()`) | every stage |
| `branch_taken` | EX (compare vs `alu_flags`) | CU `BRANCH_COND`, PC update |

**Flag mapping.** The ISA flags are `E`/`GT` (in `RegisterFile`); the ALU produces
`zero`/`negative`/`carry`/`overflow` (`AluResult`). The engine maps after `CMP`:

```
E  = alu_flags.zero
GT = !alu_flags.negative && !alu_flags.zero
```

## 4. Microcode ↔ stage mapping

Garv's `ControlSignals` is a horizontal control word. Each field maps to a
datapath stage:

| ControlSignals field | Stage | Effect |
| --- | --- | --- |
| `operand_a_src` (`RS1/IMM/PC`) | EX | select A-bus source |
| `operand_b_src` (`RS2/IMM`) | EX | select B-bus source |
| `alu_op` (`ADD..NONE`) | EX | ALU operation |
| `set_flags` | EX | commit `alu_flags` -> `E`/`GT` |
| `mem_op` (`READ/WRITE/NONE`) | MA | memory access |
| `reg_write_en` + `reg_write_src` | RW | writeback to `rd` |
| `pc_control` (`INCREMENT/LOAD_BRANCH/LOAD_ABS/HOLD`) | PC | next PC |
| `next_addr_mode` | CU | microPC sequencing |

Microcode layout convention (see `docs/architecture.md`): address 0 = shared IF+OF
microinstruction that `DISPATCH`es to `ControlMemory::entry_point(opcode)`;
each opcode's routine ends with `FETCH` back to 0.

## 5. Component interfaces (already in place)

- `Memory` (`simulator_core/core/memory.h`): `read_word`, `write_word`,
  `load_words`, `clear`, `size`. Out-of-range throws
  `RuntimeException{BAD_MEM_ACCESS, addr}` — this is Paras's hook.
- `RegisterFile` (`simulator_core/core/register_file.h`): `read`, `write`,
  `pc`/`set_pc`, `sp`/`set_sp` (`r14`), `ra`/`set_ra` (`r15`),
  `flags`/`set_flags`, `reset`. `Flags { E, GT }`.
- `Alu` (`alu/alu.h`): `AluResult execute(ALUOp op, Word a, Word b) const`.
- `ControlMemory` (`control unit/control_memory.h`): `add`, `at`, `size`,
  `set_entry_point`, `entry_point`.
- `MicroSequencer` (`control unit/micro_sequencer.h`): `micro_pc()`, `current()`,
  `advance(opcode, branch_taken)`.
- `decode` (`simulator_core/core/decoder.h`): `decode(Word)` and
  `expand_immediate(mod, raw16)`.
- `RuntimeException` (`simulator_core/core/exception.h`): `{ code, address }`.

## 6. Per-member deliverables and exact specs

### Saatvik — assembler / disassembler / preprocessor

- Deliver `risc201-asm` and `risc201-disasm`.
- **Emit/consume words that byte-for-byte match `isa_spec/isa.h`**: opcode bits
  27–31, I-bit 26, `rd` 22–25, `rs1` 18–21, `rs2` 14–17 (register format) or
  `mod` 16–17 + `imm` 0–15 (immediate format), branch `offset` 0–26.
- **Output contract**: one 32-bit word per line, 8 lowercase hex digits
  (`%08x`), big-endian digit order; document any `--binary` flag with Aditya.
- Must **not** redefine opcodes; consume `Opcode`/`get_*` from `isa.h` only.
- Immediate modifier semantics: `mod=00` sign-extend, `01` zero-extend (`u`),
  `10` shift-left-16 (`h`).
- Round-trip guarantee: assemble → disassemble → reassemble produces identical
  words.

### Garv — control unit

- Deliver `build_control_memory()` with a microcode routine for **all 21
  opcodes** (currently ADD/LD/BEQ only).
- Each routine must set, per micro-step: `alu_op`, `operand_a_src`,
  `operand_b_src`, `reg_write_en`, `reg_write_src`, `mem_op`, `pc_control`,
  `set_flags`, and end with `next_addr_mode = FETCH`.
- Keep the shared IF+OF microinstruction at address 0 that `DISPATCH`es to
  `entry_point(opcode)`.
- Expose `MicroSequencer::micro_pc()` + `current()` for Aditya's per-cycle TUI.
- Resolve `mov`/`not` operand routing with Atharva (open decision §8).
- Vertical microcode: second selectable mode (Final Eval).

### Atharva — ALU + datapath

- Replace `SimpleAlu` with the real `Alu` implementing
  `AluResult execute(ALUOp, Word, Word)` with `{value, zero, negative, carry,
  overflow}`; `DIV`/`MOD` by zero must signal `DIV_BY_ZERO`.
- Own the `Bus` + `Datapath`: deliver the stage logic that, given
  `bus.signals`, reads `RegisterFile`/`Memory`, calls `Alu`, and writes results
  back onto the bus. Required surface for the engine:
  - `void step()` — advance one micro-step.
  - `const Bus& bus() const` — snapshot for TUI.
  - `void reset()`.
- Deliver the 4-stage and 6-stage datapath block diagrams (`/docs`).
- Resolve operand conventions: `NOT` complements operand B; `mov` source lives in
  the second-operand field, so add `PASS_B` to `ALUOp` or route onto the A bus
  (coordinate with Garv).

### Aditya — core engine, loader, TUI/debugger

- Own `Memory`, `RegisterFile`, `decode`, `RuntimeException`, loader, and the
  run loop that drives `Datapath::step()` and catches `RuntimeException`.
- Loader consumes Saatvik's format exactly (`load_words(0, words)`).
- `StateView`: a per-cycle snapshot of `bus`, register file, memory window, and
  `micro_pc` — the single object the TUI and Paras's visualizer both consume.
- Debugger: step / run / run-to-breakpoint / breakpoints / watchpoints /
  register+memory dump / reset.

### Paras — memory safety + evaluation

- Extend the `Memory` bounds seam into full runtime exceptions; add
  `STACK_OVERFLOW`/`STACK_UNDERFLOW` detection against `r14` (SP) and stack
  bounds; coordinate `DIV_BY_ZERO` with Atharva.
- ASCII stack visualizer reads the same `StateView` snapshots (frame-by-frame
  diffs on push/pop).
- Own the evaluation matrix: shared programs x {4-stage, 6-stage} x {horizontal,
  vertical CU}, collecting cycle counts, hazards, exception cases, correctness.

## 7. Integration sequence

1. **Freeze `bus.h` + `isa.h`** (Milestone 0, before Sept 30) — Saatvik, Garv,
   Atharva, Aditya sign off.
2. **Oct 7 checkpoint** — assembler output loads; engine drives CU+datapath for
   a minimal program (ADD/LD/BEQ first).
3. **Oct 14** — full 21-opcode microcode; ALU fully wired; debugger + visualizer
   working.
4. **Nov 3 dry run** — assemble → run both pipelines both CU modes → trigger a
   deliberate stack fault → confirm visualizer and debugger agree.

## 8. Open decisions

1. **Multi-cycle microcode vs 4-stage pipeline overlap** — Garv's CU sequences
   micro-steps (IF+OF → EX → MA → RW); the pipelined datapath overlaps stages.
   How the two coexist is unresolved and is the main Step 5/6 design task.
2. **`PASS_B` vs A-bus routing** for `mov`/`not` (Atharva + Garv).
3. **`get_imm18` vs `get_mod`** immediate semantics (Garv; see
   `docs/isa_spec.md`).
4. **Promote `Flags` + `ExceptionCode`** into `isa_spec/isa.h` at freeze.
5. **FPU** — only if the team adopts the earlier FP proposal; otherwise
   descoped. If adopted, FP registers are opcode-selected (16 int + 16 FP), not
   bank-switched.
