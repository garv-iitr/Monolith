# RISC201 ISA Specification

The instruction set is **SimpleRISC** (Sarangi, *Basic Computer Architecture*), a
21-instruction teaching RISC. The single source of truth is `isa_spec/isa.h`
(relocated there from `control unit/isa.h`); the actual encoding is owned by Garv.

## Registers

- 16 registers, `r0`-`r15`. `r14` = stack pointer (`sp`), `r15` = return address (`ra`).
- Word width is parameterized: `RISC201_WORD_BITS` (default `32`, `64` also supported).
- Flags: `E` (equal) and `GT` (greater than), both set by `cmp`.

## Instructions (21) and 5-bit opcodes

| op | mnemonic | op | mnemonic | op | mnemonic |
| --- | --- | --- | --- | --- | --- |
| 00000 | add | 00111 | or | 01110 | ld |
| 00001 | sub | 01000 | not | 01111 | st |
| 00010 | mul | 01001 | mov | 10000 | beq |
| 00011 | div | 01010 | lsl | 10001 | bgt |
| 00100 | mod | 01011 | lsr | 10010 | b |
| 00101 | cmp | 01100 | asr | 10011 | call |
| 00110 | and | 01101 | nop | 10100 | ret |

## Formats

| Format | Layout |
| --- | --- |
| 0-address | `opcode[5]` (nop, ret) |
| Branch | `opcode[5] \| offset[27]` (b, beq, bgt, call) |
| Register | `opcode[5] \| I=0[1] \| rd[4] \| rs1[4] \| rs2[4]` |
| Immediate | `opcode[5] \| I=1[1] \| rd[4] \| rs1[4] \| mod[2] \| imm[16]` |

Bit positions (LSB-indexed): `op` bits 27-31, `I` bit 26, `rd` bits 22-25,
`rs1` bits 18-21, `rs2` bits 14-17, `mod` bits 16-17, branch `offset` bits 0-26.

## Field accessors (`isa_spec/isa.h`)

`get_opcode`, `get_ibit`, `get_rd`, `get_rs1`, `get_rs2`, `get_mod` return the raw
fields. `get_imm18` returns the 18-bit `[17:0]` field sign-extended from bit 17
(this folds in the 2 `mod` bits). `get_offset27` returns the branch offset
sign-extended from bit 26. `format_of(opcode)` classifies the opcode; for the
register/immediate families the I-bit decides at runtime.

> **Open item:** confirm with Garv how `get_imm18` (18-bit, includes `mod`) should
> be combined with `get_mod` when expanding the operand. SimpleRISC defines the
> immediate as `mod[2] + const[16]` (00 sign-extend, 01 `u` zero-extend, 10 `h`
> shift-left-16); the current accessor sign-extends the whole 18-bit field.

## Width parameterization

`Word`/`Sword` are `uint32/int32` or `uint64/int64` selected by `RISC201_WORD_BITS`
(default 32). The instruction encoding stays a fixed 32-bit SimpleRISC layout
regardless of width. Set it at configure time: `-DRISC201_WORD_BITS=64`.
