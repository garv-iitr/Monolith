# Architecture & Module Contracts

Context that would otherwise live in code comments. Keep this current instead of
re-commenting the source.

## Ownership

| Module | Owner | Notes |
| --- | --- | --- |
| `isa_spec/isa.h` | Garv (shared) | Opcodes/formats/accessors; width parameterized |
| `simulator_core/` | Aditya | Memory, register file, decoder, pipeline, loader, CLI/TUI |
| `alu/` | Atharva | ALU ops + flags (and datapath/stage logic per the PRD) |
| `control unit/` | Garv | Control memory, microinstructions, sequencer, microcode routines |
| `memory_safety/` | Paraas | Stack guards, exceptions, stack visualizer |

## Repository layout

```
isa_spec/isa.h              shared ISA contract
simulator_core/core/        memory, register_file, decoder, pipeline, loader, exception
simulator_core/ui/          state_view, tui (FTXUI), headless
simulator_core/main.cpp     entry point
alu/                        ALU (Atharva)
control unit/               control unit (Garv): control_signals, microinstruction,
                            control_memory, micro_sequencer, microcode_routines, main
tests/                      assert-based unit tests + sample programs
docs/                       PRD, isa_spec, architecture, tui
```

## Core module interfaces

- **`Memory`** (`simulator_core/core/memory.h`): flat, word-addressable; default
  4096 words. `read_word`/`write_word`/`load_words`/`clear`/`size`. Out-of-range
  access throws `RuntimeException{BAD_MEM_ACCESS, address}` — the hook the
  memory-safety module extends.
- **`RegisterFile`** (`simulator_core/core/register_file.h`): 16 GPRs, PC, flags.
  `kRegSP = 14`, `kRegRA = 15`. `read`/`write` assert the index is < 16.
  `pc`/`set_pc`, `sp`/`set_sp`, `ra`/`set_ra`, `flags`/`set_flags`, `reset`.
- **`Flags`**: `{ bool E; bool GT; }`. Defined in the core (Garv's `isa.h` has no
  flag type) — candidate to promote into `isa_spec/isa.h`.
- **`RuntimeException`** (`simulator_core/core/exception.h`): `{ ExceptionCode code;
  uint64_t address; }`. Exception codes: NONE, DIV_BY_ZERO, BAD_MEM_ACCESS,
  MISALIGNED_ACCESS, STACK_OVERFLOW, STACK_UNDERFLOW, ILLEGAL_INSTRUCTION.

## Control unit (Garv)

`ControlSignals` is a horizontal control word — one field per control line, no
encoding. Enums:

- `ALUOp`: ADD SUB MUL DIV MOD AND OR NOT LSL LSR ASR CMP PASS_A NONE.
- `BusSrc`: RS1 RS2 IMM PC ALU_OUT MEM_DATA NONE.
- `RegWriteSrc`: ALU_OUT MEM_DATA PC_PLUS4 NONE.
- `MemOp`: READ WRITE NONE.
- `PCControl`: INCREMENT LOAD_BRANCH LOAD_ABS HOLD.
- `NextMicroAddr`: INCREMENT DISPATCH JUMP BRANCH_COND FETCH.

`ControlSignals` fields: `alu_op`, `operand_a_src`, `operand_b_src`,
`reg_write_src`, `reg_write_en`, `mem_op`, `pc_control`, `set_flags`,
`next_addr_mode`, `jump_target`, `stage_name`.

`Microinstruction` = `{ ControlSignals signals; }` (one row of Control Memory).

`ControlMemory` is an array of microinstructions plus an opcode → entry-point map.
Layout convention:

- address 0: shared IF+OF microinstruction (all opcodes enter here, then `DISPATCH`)
- addresses 1..N: per-opcode EX/MA/RW blocks

`MicroSequencer` owns the `microPC` and advances it per `NextMicroAddr`:
`INCREMENT` = +1, `DISPATCH` = entry_point(opcode), `JUMP` = jump_target,
`BRANCH_COND` = jump_target if taken else +1, `FETCH` = back to address 0.

`build_control_memory()` (`microcode_routines.cpp`) currently defines routines for
ADD (EX→RW), LD (EX addr → MA read → RW), and BEQ (BRANCH_COND → not-taken FETCH /
taken PC-update FETCH). The BEQ `jump_target` is back-patched once the taken-path
address is known.

## Execution flow

`MicroSequencer` -> `ControlSignals` -> datapath (memory, register file, ALU) ->
flags/PC -> repeat. `risc201-cu-demo` walks ADD/LD/BEQ one microinstruction at a
time and prints each step.

## Build

```
cmake -S . -B build [-DRISC201_WORD_BITS=64]
cmake --build build
./build/risc201-tests
```

Targets: `risc201-sim`, `risc201-cu-demo`, `risc201-tests`, plus the
`risc201_config` / `risc201_core` / `risc201_cu` libraries.

## Open decisions

1. **Microprogrammed control vs. pipelining.** Garv's unit is multi-cycle
   (IF+OF → EX → MA → RW via `microPC`); the target datapath is 4-stage pipelined
   (IF/OF → EX → MEM → WB). The micro-steps map 1:1 onto the stages, but how
   sequenced microcode coexists with overlapped stages is unresolved.
2. **Ownership of pipeline/stage logic.** The PRD's repo comment puts "pipeline
   stage logic" under `alu/` (Atharva); the simulator core also needs it. Decide
   before Step 6/7.
3. **`get_imm18` vs `get_mod`** semantics (see `docs/isa_spec.md`).
4. **Promote `Flags` + exception codes** into `isa_spec/isa.h` at Milestone 0.
