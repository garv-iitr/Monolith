# RISC201 Simulator TUI

The optional FTXUI debugger assembles source files in-process and drives the same UI-agnostic `Debugger` as the line interface. It displays the original source alongside registers, the symbolic microprogram, execution state, and stack or memory.

## Build and launch

```sh
cmake -S . -B build-tui -DRISC201_TUI=ON
cmake --build build-tui --target risc201-sim
./build-tui/risc201-sim --tui
```

The stage and encoding flags work in both modes:

```sh
./build-tui/risc201-sim --tui --stages 4 --encoding vertical
```

The line-based debugger remains the default and can be selected with `--headless`. Configure with `-DRISC201_TUI=OFF` to avoid fetching FTXUI; `--tui` then reports that the feature is unavailable.

## Open and execute source

Press `o`, enter an `.asm` path, and press Enter. The simulator calls the in-process assembler. On success it resets the CPU, loads text and data at the assembly result's configured bases, and shows the original source. Assembly errors appear in the status line and do not replace the currently loaded program.

The source pane is a runtime trace rather than a random-access editor. Focus it with Tab and use Down or `s` to execute one instruction; use Up or `u` to step back one instruction. The source highlight follows the executed or restored instruction, so branches and loops follow actual runtime control flow. Expanded instructions such as `li` map back to their original source line.

Focus the microinstruction pane with Tab and use Down or `m` to execute one actual microinstruction; use Up to undo one microinstruction. The list contains the shared fetch/decode preamble and the current opcode's symbolic routine. The active highlight follows the actual micro-PC. The UI does not jump directly to selected micro-PCs, because a microinstruction may depend on state established by earlier steps.

The debugger retains up to 4096 micro-step undo records and discards the oldest records when the limit is exceeded. Loading or resetting a program clears history, as do debugger-mediated external register or memory edits.

## Panes

- **Source:** original `.asm` lines, with selected and currently executing lines distinguished.
- **Registers:** PC, IR, `r0`-`r15` with `sp`/`ra` labels, and Z/N/C/V flags.
- **Microinstructions:** symbolic control steps and the active micro-PC.
- **Execution / internal state:** decoded opcode, stage, internal ALU/memory latches, current control signals, halt/fault status.
- **Stack / memory:** stack view by default; press `v` to switch to memory and `g` to enter a byte address.
- **Header/footer:** stage layout, encoding, execution status, and key hints.

Stage labels are microcode/debug metadata; execution is not overlapped as in a hardware pipeline.

## Keys

| Key | Action |
| --- | --- |
| `o` | Open and assemble an `.asm` file |
| `Down` in Source, or `s` | Step one instruction forward |
| `Up` in Source, or `u` | Step one instruction backward |
| `Down` in Microinstructions, or `m` | Step one microinstruction forward |
| `Up` in Microinstructions | Undo one microinstruction |
| `r` | Run or pause; stop at breakpoint or halt |
| `b` | Toggle breakpoint at current PC |
| `Tab` / `Shift-Tab` | Focus next/previous pane |
| `v` | Toggle stack and memory views |
| `g` | Enter the memory pane address |
| `x` | Reload the current assembled program from its entry point |
| `?`, `F1` | Toggle help |
| `q` | Quit |
