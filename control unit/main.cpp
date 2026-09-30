// ============================================================================
// Control-unit demonstration / self-test.
//
// Runs several RISC201 programs through all four configurations
//   {4-stage, 6-stage} x {horizontal, vertical}
// and prints, per micro-cycle, the microPC, the pipeline stage, the register
// driving the write bus, the registers latching the read bus, the transfer
// multiplexer selection, the functional-unit args and the sequencer action.
//
// The four configurations must produce identical architectural state; the demo
// checks that for every program and exits non-zero otherwise.
// ============================================================================

#include <array>
#include <cstdint>
#include <cstdio>
#include <utility>
#include <vector>

#include "control_unit.h"
#include "isa_spec/isa.h"
#include "simulator_core/core/exception.h"

using namespace risc201::cu;

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

struct Program_ {
  const char* name;
  std::vector<Word> code;
  std::vector<std::pair<Word, Word>> data;  // byte address -> value
  std::uint32_t probe_addr = 0;             // word to report
  Word expected_probe = 0;
};

// mov/add/st/ld -- arithmetic and data movement.
Program_ prog_arith() {
  return {"arith",
          {
              enc_imm(Opcode::MOV, 1, 0, 5),      // mov  r1, #5
              enc_imm(Opcode::MOV, 2, 0, 3),      // mov  r2, #3
              enc_reg(Opcode::ADD, 3, 1, 2),      // add  r3, r1, r2
              enc_mem(Opcode::ST, 3, 0, 0x100),   // st   r3, 0x100[r0]
              enc_mem(Opcode::LD, 4, 0, 0x100),   // ld   r4, 0x100[r0]
              enc_halt(),
          },
          {},
          0x100,
          8};
}

// cmp/beq/b -- a counted loop that sums 5+4+3+2+1.
Program_ prog_loop() {
  return {"loop (cmp/beq/b)",
          {
              enc_imm(Opcode::MOV, 1, 0, 0),       // 0:  mov r1, #0    (sum)
              enc_imm(Opcode::MOV, 2, 0, 5),       // 4:  mov r2, #5    (i)
              enc_reg(Opcode::ADD, 1, 1, 2),       // 8:  add r1, r1, r2
              enc_imm(Opcode::SUB, 2, 2, 1),       // 12: sub r2, r2, #1
              enc_imm(Opcode::CMP, 0, 2, 0),       // 16: cmp r2, #0
              enc_branch(Opcode::BEQ, 1),          // 20: beq +1 (-> 28)
              enc_branch(Opcode::B, -5),           // 24: b -5  (-> 8)
              enc_mem(Opcode::ST, 1, 0, 0x108),    // 28: st r1, 0x108[r0]
              enc_halt(),                          // 32: halt
          },
          {},
          0x108,
          15};
}

// call/ret -- full-descending stack return-address handling.
Program_ prog_call() {
  return {"call/ret",
          {
              enc_imm(Opcode::MOV, 1, 0, 7),        // 0:  mov r1, #7
              enc_imm(Opcode::MOV, REG_SP, 0, 0x300),  // 4:  mov sp, #0x300
              enc_branch(Opcode::CALL, 2),          // 8:  call +2 (-> 20)
              enc_mem(Opcode::ST, 1, 0, 0x10C),     // 12: st r1, 0x10C[r0]
              enc_halt(),                           // 16: halt
              enc_imm(Opcode::ADD, 1, 1, 1),        // 20: add r1, r1, #1
              enc_zero(Opcode::RET),                // 24: ret
          },
          {},
          0x10C,
          8};
}

// fld/fadd/fst -- floating point.
Program_ prog_fpu() {
  const Word f1_5 = 0x3FC00000;  // 1.5
  const Word f2_25 = 0x40100000; // 2.25
  return {"fpu (fld/fadd/fst)",
          {
              enc_mem(Opcode::FLD, 1, 0, 0x200),   // fld f1, 0x200[r0]
              enc_mem(Opcode::FLD, 2, 0, 0x204),   // fld f2, 0x204[r0]
              enc_reg(Opcode::FADD, 3, 1, 2),      // fadd f3, f1, f2
              enc_mem(Opcode::FST, 3, 0, 0x208),   // fst f3, 0x208[r0]
              enc_halt(),
          },
          {{0x200, f1_5}, {0x204, f2_25}},
          0x208,
          0x40700000};  // 3.75
}

struct Snapshot {
  std::array<Word, NUM_REGISTERS> regs{};
  Word probe = 0;
  bool halted = false;
  bool faulted = false;
};

// --- Trace helpers -----------------------------------------------------------
const char* transfer_name(TransferSel s) {
  switch (s) {
    case TransferSel::WRITE_BUS: return "write_bus";
    case TransferSel::MU_IMM:    return "muimm";
    case TransferSel::MU_ADDER:  return "muadder";
  }
  return "?";
}
const char* next_name(NextAddr n) {
  switch (n) {
    case NextAddr::INCREMENT:   return "next";
    case NextAddr::MSWITCH:     return "switch";
    case NextAddr::JUMP:        return "target";
    case NextAddr::BRANCH_COND: return "branch";
    case NextAddr::FETCH:       return "fetch";
  }
  return "?";
}
const char* unit_op_name(UnitId unit, std::uint8_t op) {
  switch (unit) {
    case UnitId::ALU:     return alu_op_name(static_cast<AluOp>(op));
    case UnitId::REGFILE: return reg_op_name(static_cast<RegOp>(op));
    case UnitId::MEMORY:  return mem_op_name(static_cast<MemOp>(op));
    case UnitId::FPU:     return fpu_op_name(static_cast<FpuOp>(op));
    case UnitId::MEMCHK:  return op == 0 ? "nop" : "check";
    case UnitId::CONTROL: return op == 0 ? "nop" : "halt";
    default:              return "nop";
  }
}
void print_reads(std::uint32_t mask) {
  bool first = true;
  for (int i = 1; i < static_cast<int>(kNumInternalRegs); ++i) {
    const InternalReg r = static_cast<InternalReg>(i);
    if (mask & reg_bit(r)) {
      std::printf("%s%s", first ? "" : ",", internal_reg_name(r));
      first = false;
    }
  }
  if (first) std::printf("-");
}

// --- ROM builders, selected by control-unit type ----------------------------
template <typename CU> struct RomBuilder;
template <> struct RomBuilder<HorizontalControlUnit> {
  static ControlMemory<HorizontalMicroWord> build(const Program& p) { return assemble_horizontal(p); }
};
template <> struct RomBuilder<VerticalControlUnit> {
  static ControlMemory<VerticalMicroWord> build(const Program& p) { return assemble_vertical(p); }
};

template <typename CU>
Snapshot execute(StageLayout layout, const Program_& prog, bool emit) {
  Datapath dp(1024);
  dp.memchk.enabled = (layout == StageLayout::SIX_STAGE);
  dp.mem.load_words(0, prog.code);
  for (const auto& d : prog.data) dp.mem.write_word(d.first, d.second);

  const Program program = build_program();
  CU cu(dp, layout);
  cu.install(RomBuilder<CU>::build(program));

  if (emit) {
    std::printf("\n=== horizontal : 6-stage : %s ===\n", prog.name);
    std::printf("control memory: %lu microinstructions\n",
                static_cast<unsigned long>(cu.cm.used()));
  }

  Snapshot snap;
  int cycles = 0;
  while (cycles < 20000) {
    const std::uint16_t pc_before = cu.micro_pc();
    bool alive = false;
    try {
      alive = cu.step();
    } catch (const RuntimeException& e) {
      if (emit) {
        std::printf("  [fault] %s @ 0x%llX\n", to_string(e.code),
                    static_cast<unsigned long long>(e.address));
      }
      snap.faulted = true;
      break;
    }
    ++cycles;

    if (emit && cycles <= 80) {
      const ControlSignals& s = cu.signals();
      const UnitId unit = args_unit(s.args);
      std::printf("  upc=%3u %-7s %-6s | wbus=%-11s rbus=[", pc_before, cu.stage(),
                  micro_op_name(cu.word().op), internal_reg_name(s.write_src));
      print_reads(s.read_mask);
      std::printf("] xfer=%-9s args=%s/%s", transfer_name(s.transfer_sel), unit_name(unit),
                  unit_op_name(unit, args_op(s.args)));
      if (s.set_flags) std::printf(" setflags");
      std::printf(" -> %s\n", next_name(s.next_addr));
    }
    if (!alive) break;
  }

  snap.halted = dp.halted;
  for (int i = 0; i < NUM_REGISTERS; ++i) snap.regs[i] = dp.rf.read(static_cast<std::uint8_t>(i));
  snap.probe = dp.mem.read_word(prog.probe_addr);

  if (emit) {
    std::printf("  -- halted after %d micro-cycles (%s) --\n", cycles,
                dp.halt_reason[0] ? dp.halt_reason : "halt");
    std::printf("  regs:");
    for (int i = 0; i < NUM_REGISTERS; ++i)
      std::printf(" r%d=%u", i, static_cast<unsigned>(snap.regs[i]));
    std::printf("\n  flags: Z=%d N=%d C=%d V=%d  mem[0x%X]=0x%X\n", dp.regs.flags.Z, dp.regs.flags.N,
                dp.regs.flags.C, dp.regs.flags.V, prog.probe_addr, static_cast<unsigned>(snap.probe));
  }
  return snap;
}

bool same(const Snapshot& a, const Snapshot& b) {
  if (a.probe != b.probe) return false;
  for (int i = 0; i < NUM_REGISTERS; ++i) {
    if (a.regs[i] != b.regs[i]) return false;
  }
  return true;
}

}  // namespace

int main() {
  std::printf("RISC201 microprogrammed control unit\n");
  std::printf("horizontally encoded microinstruction : %d bits\n", kHorizontalWordBits);
  std::printf("vertically encoded microinstruction   : %d bits\n", kVerticalWordBits);
  std::printf("internal registers visible to microcode : %lu\n",
              static_cast<unsigned long>(kNumInternalRegs));

  int failures = 0;
  auto check = [&failures](bool cond, const char* what) {
    std::printf("  [%s] %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) ++failures;
  };

  const std::vector<Program_> programs = {prog_arith(), prog_loop(), prog_call(), prog_fpu()};

  for (std::size_t i = 0; i < programs.size(); ++i) {
    const Program_& p = programs[i];
    // Only the first program is traced.
    const Snapshot hs6 = execute<HorizontalControlUnit>(StageLayout::SIX_STAGE, p, i == 0);
    const Snapshot vs6 = execute<VerticalControlUnit>(StageLayout::SIX_STAGE, p, false);
    const Snapshot hs4 = execute<HorizontalControlUnit>(StageLayout::FOUR_STAGE, p, false);
    const Snapshot vs4 = execute<VerticalControlUnit>(StageLayout::FOUR_STAGE, p, false);

    std::printf("\n--- %s ---\n", p.name);
    check(hs6.halted && vs6.halted && hs4.halted && vs4.halted && !hs6.faulted,
          "runs to halt without fault");
    check(hs6.probe == p.expected_probe, "program result correct");
    check(same(hs6, vs6), "horizontal == vertical (6-stage)");
    check(same(hs4, vs4), "horizontal == vertical (4-stage)");
    check(same(hs6, hs4), "6-stage == 4-stage");
  }

  std::printf("\n%s\n", failures == 0 ? "ALL CHECKS PASSED" : "CHECKS FAILED");
  return failures == 0 ? 0 : 1;
}
