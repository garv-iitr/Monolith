#include "tests/test_programs.h"

namespace risc201::testbench {
namespace {

// --- Instruction encoders (ISA.md formats) ----------------------------------
Word enc_imm(Opcode op, int rd, int rs1, int imm) {
  return (static_cast<Word>(op) << 26) | (1u << 24) |
         (static_cast<Word>(rd & 0xF) << 20) | (static_cast<Word>(rs1 & 0xF) << 16) |
         static_cast<Word>(imm & 0xFFFF);
}
Word enc_reg(Opcode op, int rd, int rs1, int rs2) {
  return (static_cast<Word>(op) << 26) | (static_cast<Word>(rd & 0xF) << 20) |
         (static_cast<Word>(rs1 & 0xF) << 16) | (static_cast<Word>(rs2 & 0xF) << 12);
}
Word enc_mem(Opcode op, int rd, int rs1, int imm) {
  return (static_cast<Word>(op) << 26) | (static_cast<Word>(rd & 0xF) << 20) |
         (static_cast<Word>(rs1 & 0xF) << 16) | static_cast<Word>(imm & 0xFFFF);
}
Word enc_branch(Opcode op, int word_offset) {
  return (static_cast<Word>(op) << 26) | (static_cast<Word>(word_offset) & 0x03FFFFFF);
}
Word enc_halt() { return static_cast<Word>(Opcode::HALT) << 26; }
Word enc_zero(Opcode op) { return static_cast<Word>(op) << 26; }

}  // namespace

// ----------------------------------------------------------------------------
// fib: recursive Fibonacci.  fib(10) = 55.
//   fib(n): r1 = n in, r2 = result out; saves n and fib(n-1) on the stack.
// Branch convention: target = (instr addr + 4) + offset * 4.
// ----------------------------------------------------------------------------
TestProgram program_fib() {
  TestProgram p;
  p.name = "recursive fibonacci";
  p.code = {
      enc_imm(Opcode::MOV, REG_SP, 0, 0x300),  // 0:  mov sp, #0x300
      enc_imm(Opcode::MOV, 1, 0, 10),          // 4:  mov r1, #10
      enc_branch(Opcode::CALL, 8),             // 8:  call fib (-> 44)
      enc_zero(Opcode::NOP),                   // 12: nop
      enc_mem(Opcode::ST, 2, 0, 0x100),        // 16: st r2, 0x100[r0]
      enc_halt(),                              // 20: halt
      enc_zero(Opcode::NOP),                   // 24: padding
      enc_zero(Opcode::NOP),                   // 28: padding
      enc_zero(Opcode::NOP),                   // 32: padding
      enc_zero(Opcode::NOP),                   // 36: padding
      enc_zero(Opcode::NOP),                   // 40: padding
      enc_imm(Opcode::CMP, 0, 1, 1),           // 44: fib: cmp r1, #1
      enc_branch(Opcode::BGT, 5),              // 48: bgt recurse (-> 72)
      enc_branch(Opcode::BNE, 2),              // 52: bne base0   (-> 64)
      enc_imm(Opcode::MOV, 2, 0, 1),           // 56: mov r2, #1
      enc_branch(Opcode::B, 1),                // 60: b baseret   (-> 68)
      enc_imm(Opcode::MOV, 2, 0, 0),           // 64: base0: mov r2, #0
      enc_zero(Opcode::RET),                   // 68: baseret: ret
      enc_imm(Opcode::SUB, REG_SP, REG_SP, 4), // 72: recurse: sub sp, sp, #4
      enc_mem(Opcode::ST, 1, REG_SP, 0),       // 76: st r1, 0[sp]
      enc_imm(Opcode::SUB, 1, 1, 1),           // 80: sub r1, r1, #1
      enc_branch(Opcode::CALL, -11),           // 84: call fib (-> 44)
      enc_imm(Opcode::SUB, REG_SP, REG_SP, 4), // 88: sub sp, sp, #4
      enc_mem(Opcode::ST, 2, REG_SP, 0),       // 92: st r2, 0[sp]
      enc_mem(Opcode::LD, 1, REG_SP, 4),       // 96: ld r1, 4[sp]
      enc_imm(Opcode::SUB, 1, 1, 2),           // 100: sub r1, r1, #2
      enc_branch(Opcode::CALL, -16),           // 104: call fib (-> 44)
      enc_mem(Opcode::LD, 3, REG_SP, 0),       // 108: ld r3, 0[sp]
      enc_reg(Opcode::ADD, 2, 2, 3),           // 112: add r2, r2, r3
      enc_imm(Opcode::ADD, REG_SP, REG_SP, 8), // 116: add sp, sp, #8
      enc_zero(Opcode::RET),                   // 120: ret
  };
  p.expected_mem = {{0x100, 55}};              // fib(10) = 55
  p.expected_regs = {{REG_SP, 0x300}};         // stack restored
  return p;
}

// ----------------------------------------------------------------------------
// matmul: C = A x B (2x2, row-major at 0x200/0x220/0x240), then an FPU kernel:
//   f1=1.5, f2=2.25 -> fadd 3.75, fsub -0.75, fmul 3.375, fdiv 2/3,
//   fcmp f1,f2 (takes the blt path), fmov f2.
// ----------------------------------------------------------------------------
TestProgram program_matmul() {
  TestProgram p;
  p.name = "matrix multiply + fpu";
  p.code = {
      enc_imm(Opcode::MOV, 1, 0, 0),          // 0:  mov r1, #0        (i)
      enc_imm(Opcode::CMP, 0, 1, 2),          // 4:  i_loop: cmp r1, #2
      enc_branch(Opcode::BEQ, 36),            // 8:  beq done (-> 156)
      enc_branch(Opcode::BLT, 1),             // 12: blt i_body (-> 20)
      enc_branch(Opcode::B, 34),              // 16: b done (-> 156)
      enc_imm(Opcode::MOV, 2, 0, 0),          // 20: i_body: mov r2, #0   (j)
      enc_imm(Opcode::CMP, 0, 2, 2),          // 24: j_loop: cmp r2, #2
      enc_branch(Opcode::BEQ, 29),            // 28: beq j_end (-> 148)
      enc_branch(Opcode::BLT, 1),             // 32: blt j_body (-> 40)
      enc_branch(Opcode::B, 27),              // 36: b j_end (-> 148)
      enc_reg(Opcode::XOR, 4, 4, 4),          // 40: j_body: xor r4, r4, r4  (sum)
      enc_imm(Opcode::MOV, 3, 0, 0),          // 44: mov r3, #0        (k)
      enc_imm(Opcode::CMP, 0, 3, 2),          // 48: k_loop: cmp r3, #2
      enc_branch(Opcode::BEQ, 16),            // 52: beq k_end (-> 120)
      enc_branch(Opcode::BLT, 1),             // 56: blt k_body (-> 64)
      enc_branch(Opcode::B, 14),              // 60: b k_end (-> 120)
      enc_reg(Opcode::MOV, 5, 0, 1),          // 64: k_body: mov r5, r1
      enc_imm(Opcode::MUL, 5, 5, 2),          // 68: mul r5, r5, #2   (i*2)
      enc_reg(Opcode::ADD, 5, 5, 3),          // 72: add r5, r5, r3   (i*2+k)
      enc_imm(Opcode::LSL, 5, 5, 2),          // 76: lsl r5, r5, #2   (byte offset)
      enc_reg(Opcode::MOV, 6, 0, 3),          // 80: mov r6, r3
      enc_imm(Opcode::MUL, 6, 6, 2),          // 84: mul r6, r6, #2   (k*2)
      enc_reg(Opcode::ADD, 6, 6, 2),          // 88: add r6, r6, r2   (k*2+j)
      enc_imm(Opcode::LSL, 6, 6, 2),          // 92: lsl r6, r6, #2
      enc_mem(Opcode::LD, 7, 5, 0x200),       // 96: ld r7, 0x200[r5] (A[i][k])
      enc_mem(Opcode::LD, 8, 6, 0x220),       // 100: ld r8, 0x220[r6] (B[k][j])
      enc_reg(Opcode::MUL, 7, 7, 8),          // 104: mul r7, r7, r8
      enc_reg(Opcode::ADD, 4, 4, 7),          // 108: add r4, r4, r7  (sum +=)
      enc_imm(Opcode::ADD, 3, 3, 1),          // 112: add r3, r3, #1
      enc_branch(Opcode::B, -18),             // 116: b k_loop (-> 48)
      enc_reg(Opcode::MOV, 9, 0, 1),          // 120: k_end: mov r9, r1
      enc_imm(Opcode::MUL, 9, 9, 2),          // 124: mul r9, r9, #2   (i*2)
      enc_reg(Opcode::ADD, 9, 9, 2),          // 128: add r9, r9, r2   (i*2+j)
      enc_imm(Opcode::LSL, 9, 9, 2),          // 132: lsl r9, r9, #2
      enc_mem(Opcode::ST, 4, 9, 0x240),       // 136: st r4, 0x240[r9] (C[i][j])
      enc_imm(Opcode::ADD, 2, 2, 1),          // 140: add r2, r2, #1
      enc_branch(Opcode::B, -31),             // 144: b j_loop (-> 24)
      enc_imm(Opcode::ADD, 1, 1, 1),          // 148: j_end: add r1, r1, #1
      enc_branch(Opcode::B, -38),             // 152: b i_loop (-> 4)
      enc_mem(Opcode::FLD, 1, 0, 0x260),      // 156: done: fld f1, 0x260[r0]  (1.5)
      enc_mem(Opcode::FLD, 2, 0, 0x264),      // 160: fld f2, 0x264[r0]  (2.25)
      enc_reg(Opcode::FADD, 3, 1, 2),         // 164: fadd f3, f1, f2    (3.75)
      enc_mem(Opcode::FST, 3, 0, 0x270),      // 168: fst f3, 0x270[r0]
      enc_reg(Opcode::FSUB, 4, 1, 2),         // 172: fsub f4, f1, f2    (-0.75)
      enc_mem(Opcode::FST, 4, 0, 0x274),      // 176: fst f4, 0x274[r0]
      enc_reg(Opcode::FMUL, 5, 1, 2),         // 180: fmul f5, f1, f2    (3.375)
      enc_mem(Opcode::FST, 5, 0, 0x278),      // 184: fst f5, 0x278[r0]
      enc_reg(Opcode::FDIV, 6, 1, 2),         // 188: fdiv f6, f1, f2    (2/3)
      enc_mem(Opcode::FST, 6, 0, 0x27C),      // 192: fst f6, 0x27C[r0]
      enc_reg(Opcode::FCMP, 0, 1, 2),         // 196: fcmp f1, f2  -> N=1 (f1 < f2)
      enc_branch(Opcode::BGT, 5),             // 200: bgt fbad (-> 224, not taken)
      enc_branch(Opcode::BLT, 1),             // 204: blt fgood (-> 212, taken)
      enc_branch(Opcode::B, 3),               // 208: b fbad (-> 224)
      enc_imm(Opcode::MOV, 10, 0, 7),         // 212: fgood: mov r10, #7
      enc_mem(Opcode::ST, 10, 0, 0x280),      // 216: st r10, 0x280[r0]
      enc_branch(Opcode::B, 2),               // 220: b fcont (-> 232)
      enc_imm(Opcode::MOV, 10, 0, 99),        // 224: fbad: mov r10, #99
      enc_mem(Opcode::ST, 10, 0, 0x280),      // 228: st r10, 0x280[r0]
      enc_reg(Opcode::FMOV, 7, 0, 2),         // 232: fcont: fmov f7, f2
      enc_mem(Opcode::FST, 7, 0, 0x284),      // 236: fst f7, 0x284[r0]
      enc_halt(),                             // 240: halt
  };
  p.data = {
      {0x200, 1}, {0x204, 2}, {0x208, 3}, {0x20C, 4},        // A
      {0x220, 5}, {0x224, 6}, {0x228, 7}, {0x22C, 8},        // B
      {0x260, 0x3FC00000},                                   // 1.5f
      {0x264, 0x40100000},                                   // 2.25f
  };
  p.expected_mem = {
      {0x240, 19}, {0x244, 22}, {0x248, 43}, {0x24C, 50},     // C = A x B
      {0x270, 0x40700000},   // 3.75f
      {0x274, 0xBF400000},   // -0.75f
      {0x278, 0x40580000},   // 3.375f
      {0x27C, 0x3F2AAAAB},   // 2/3f
      {0x280, 7},            // fcmp took the blt path
      {0x284, 0x40100000},   // fmov f2 = 2.25f
  };
  return p;
}

// ----------------------------------------------------------------------------
// sort: bubble sort [5,2,9,1,7,3,8,4] with xor-swap, then a checksum pass
// using mod/div/and/or/lsr/asr/not and a bndchk on the array.
// ----------------------------------------------------------------------------
TestProgram program_sort() {
  TestProgram p;
  p.name = "bubble sort + checksum";
  p.code = {
      enc_imm(Opcode::MOV, 1, 0, 7),          // 0:  mov r1, #7     (outer i)
      enc_imm(Opcode::CMP, 0, 1, 0),          // 4:  outer: cmp r1, #0
      enc_branch(Opcode::BEQ, 26),            // 8:  beq checksum (-> 116)
      enc_branch(Opcode::BLT, 25),            // 12: blt checksum (-> 116, safety)
      enc_imm(Opcode::MOV, 2, 0, 0),          // 16: mov r2, #0     (inner j)
      enc_reg(Opcode::CMP, 0, 2, 1),          // 20: inner: cmp r2, r1
      enc_branch(Opcode::BEQ, 20),            // 24: beq inner_end (-> 108)
      enc_branch(Opcode::BGT, 19),            // 28: bgt inner_end (-> 108)
      enc_reg(Opcode::MOV, 3, 0, 2),          // 32: mov r3, r2
      enc_imm(Opcode::LSL, 3, 3, 2),          // 36: lsl r3, r3, #2  (&a[j])
      enc_mem(Opcode::LD, 4, 3, 0x300),       // 40: ld r4, 0x300[r3]  (a[j])
      enc_imm(Opcode::ADD, 3, 3, 4),          // 44: add r3, r3, #4
      enc_mem(Opcode::LD, 5, 3, 0x300),       // 48: ld r5, 0x300[r3]  (a[j+1])
      enc_reg(Opcode::CMP, 0, 4, 5),          // 52: cmp r4, r5
      enc_branch(Opcode::BGT, 1),             // 56: bgt swap (-> 64)
      enc_branch(Opcode::B, 9),               // 60: b noswap (-> 100)
      enc_imm(Opcode::SUB, 3, 3, 4),          // 64: swap: sub r3, r3, #4  (&a[j])
      enc_mem(Opcode::LD, 4, 3, 0x300),       // 68: ld r4, 0x300[r3]
      enc_imm(Opcode::ADD, 6, 3, 4),          // 72: add r6, r3, #4        (&a[j+1])
      enc_mem(Opcode::LD, 5, 6, 0x300),       // 76: ld r5, 0x300[r6]
      enc_reg(Opcode::XOR, 4, 4, 5),          // 80: xor r4, r4, r5
      enc_reg(Opcode::XOR, 5, 5, 4),          // 84: xor r5, r5, r4
      enc_reg(Opcode::XOR, 4, 4, 5),          // 88: xor r4, r4, r5
      enc_mem(Opcode::ST, 4, 3, 0x300),       // 92: st r4, 0x300[r3]
      enc_mem(Opcode::ST, 5, 6, 0x300),       // 96: st r5, 0x300[r6]
      enc_imm(Opcode::ADD, 2, 2, 1),          // 100: noswap: add r2, r2, #1
      enc_branch(Opcode::B, -22),             // 104: b inner (-> 20)
      enc_imm(Opcode::SUB, 1, 1, 1),          // 108: inner_end: sub r1, r1, #1
      enc_branch(Opcode::B, -28),             // 112: b outer (-> 4)
      enc_mem(Opcode::BNDCHK, 0, 0, 0x300),   // 116: checksum: bndchk r0, #0x300
      enc_imm(Opcode::MOV, 6, 0, 0),          // 120: mov r6, #0    (sum)
      enc_imm(Opcode::MOV, 7, 0, 0),          // 124: mov r7, #0    (index)
      enc_imm(Opcode::CMP, 0, 7, 8),          // 128: csum_loop: cmp r7, #8
      enc_branch(Opcode::BEQ, 6),             // 132: beq csum_done (-> 160)
      enc_reg(Opcode::MOV, 8, 0, 7),          // 136: mov r8, r7
      enc_imm(Opcode::LSL, 8, 8, 2),          // 140: lsl r8, r8, #2
      enc_mem(Opcode::LD, 4, 8, 0x300),       // 144: ld r4, 0x300[r8]
      enc_reg(Opcode::ADD, 6, 6, 4),          // 148: add r6, r6, r4
      enc_imm(Opcode::ADD, 7, 7, 1),          // 152: add r7, r7, #1
      enc_branch(Opcode::B, -8),              // 156: b csum_loop (-> 128)
      enc_imm(Opcode::MOD, 6, 6, 10),         // 160: csum_done: mod r6, r6, #10 -> 9
      enc_mem(Opcode::ST, 6, 0, 0x320),       // 164: st r6, 0x320[r0]
      enc_imm(Opcode::MOV, 6, 0, 39),         // 168: mov r6, #39
      enc_imm(Opcode::DIV, 6, 6, 10),         // 172: div r6, r6, #10 -> 3
      enc_imm(Opcode::AND, 6, 6, 0xFF),       // 176: and r6, r6, #0xFF -> 3
      enc_imm(Opcode::OR, 6, 6, 0x100),       // 180: or r6, r6, #0x100 -> 0x103
      enc_imm(Opcode::LSR, 6, 6, 4),          // 184: lsr r6, r6, #4 -> 16
      enc_imm(Opcode::ASR, 6, 6, 1),          // 188: asr r6, r6, #1 -> 8
      enc_reg(Opcode::NOT, 6, 0, 6),          // 192: not r6, r6 -> 0xFFFFFFF7
      enc_mem(Opcode::ST, 6, 0, 0x324),       // 196: st r6, 0x324[r0]
      enc_halt(),                             // 200: halt
  };
  p.data = {
      {0x300, 5}, {0x304, 2}, {0x308, 9}, {0x30C, 1},
      {0x310, 7}, {0x314, 3}, {0x318, 8}, {0x31C, 4},
  };
  p.expected_mem = {
      {0x300, 1}, {0x304, 2}, {0x308, 3}, {0x30C, 4},
      {0x310, 5}, {0x314, 7}, {0x318, 8}, {0x31C, 9},   // sorted ascending
      {0x320, 9},            // 39 mod 10
      {0x324, 0xFFFFFFF7},   // ~(((39/10 & 0xFF) | 0x100) >> 4 >> 1) = ~8
  };
  return p;
}

std::set<std::uint8_t> opcodes_used(const std::vector<TestProgram>& programs) {
  std::set<std::uint8_t> used;
  for (const TestProgram& p : programs) {
    for (Word w : p.code) used.insert(get_opcode(w));
  }
  return used;
}

const std::set<std::uint8_t>& all_opcodes() {
  static const std::set<std::uint8_t> all = {
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
      0x0C, 0x0D, 0x10, 0x11, 0x12, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26,
      0x27, 0x28, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
  };
  return all;
}

}  // namespace risc201::testbench
