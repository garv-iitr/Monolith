# RISC201 Simulator TUI

> **Status:** placeholder (Step 0 scaffolding). Implemented in Step 9 with FTXUI.

## Modes

- `risc201-sim --tui` — interactive full-screen debugger (default).
- `risc201-sim --headless ...` — non-interactive batch mode for tests and scripting.

Both front-ends share one command grammar and drive the same UI-agnostic core
(`simulator_core/core/*` + a `StateView` snapshot).

## Panels

| Region | Contents |
| --- | --- |
| Left | Registers `r0`-`r15` (sp/ra labelled), PC, FLAGS (E, GT) |
| Center | Pipeline stages IF/OF -> EX -> MEM -> WB, stalls/bubbles, cycle counter |
| Right | Memory word dump around SP/PC or a typed address |
| Bottom | Current instruction, microPC, horizontal control word |

## Commands

`load <file>`, `run`, `step [n]`, `regs`, `mem <addr> <count>`, `break <addr>`,
`disasm <addr> <count>`, `reset`, `quit`, `help`.

## Hotkeys

`s` step, `r` run/continue, `b` toggle breakpoint at PC, `g` goto address,
`q` quit, `?` help.
