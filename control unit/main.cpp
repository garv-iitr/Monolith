// ============================================================================
// Control-unit demonstration / self-test.
//
// Two layers of checks:
//
//   1. Coverage -- every RISC201 opcode has a microcode routine and an entry
//      point in both the horizontal and the vertical control memory.
//
//   2. Execution -- programs that exercise everything this module owns:
//      the shared bus, the decode unit, the register file, the data memory
//      and the micro-sequencer: immediate/register moves, unconditional and
//      flag-driven branches, full-descending call/ret, and the illegal-
//      instruction halt path.  The ALU, FPU and memory-check units are owned
//      by Atharva and Paraas; this module only emits their args on the bus.
//
// Every execution program runs under all four configurations
//   {4-stage, 6-stage} x {horizontal, vertical}
// and the four configurations must produce identical architectural state;
// the demo exits non-zero otherwise.
// ============================================================================

#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "control_unit.h"
#include "isa_spec/isa.h"
#include "simulator_core/core/exception.h"

using namespace risc201::cu;

namespace {

constexpr int kOpcodeCount = 34;
const std::array<Opcode, kOpcodeCount> kAllOpcodes = {
    Opcode::NOP, Opcode::ADD, Opcode::SUB, Opcode::MUL, Opcode::DIV, Opcode::MOD,
    Opcode::AND, Opcode::OR,  Opcode::NOT, Opcode::XOR, Opcode::LSL, Opcode::LSR,
    Opcode::ASR, Opcode::CMP, Opcode::MOV, Opcode::LD,  Opcode::ST,
    Opcode::B,   Opcode::BEQ, Opcode::BNE, Opcode::BGT, Opcode::BLT,
    Opcode::CALL, Opcode::RET, Opcode::BNDCHK, Opcode::HALT,
    Opcode::FADD, Opcode::FSUB, Opcode::FMUL, Opcode::FDIV, Opcode::FCMP,
    Opcode::FLD, Opcode::FST, Opcode::FMOV,
};

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
Word enc_branch(Opcode op, int word_offset) {
  return (static_cast<Word>(op) << 26) | (static_cast<Word>(word_offset) & 0x03FFFFFF);
}
Word enc_halt() { return static_cast<Word>(Opcode::HALT) << 26; }
Word enc_zero(Opcode op) { return static_cast<Word>(op) << 26; }
Word enc_raw(std::uint8_t opcode) { return static_cast<Word>(opcode) << 26; }

struct Program_ {
  const char* name;
  std::vector<Word> code;
  std::vector<std::pair<Word, Word>> data = {};                   // byte addr -> value
  std::vector<std::pair<std::uint8_t, Word>> pre_seed = {};        // GPRs set before the run
  std::vector<std::pair<std::uint8_t, Word>> expected_regs = {};   // GPRs checked after
  std::uint32_t probe_addr = 0;                               // memory word to report
  Word expected_probe = 0;
  bool seed_flags = false;                                    // pre-set Z/N before the run
  bool seed_z = false;
  bool seed_n = false;
  const char* expected_reason = nullptr;  // expected halt reason (null = normal halt)
};

// mov (imm + reg forms) / nop / halt -- shared bus, decode, register file.
Program_ prog_transfer() {
  return {"mov/nop",
          {
              enc_imm(Opcode::MOV, 1, 0, 5),        // 0:  mov r1, #5
              enc_reg(Opcode::MOV, 2, 0, 1),        // 4:  mov r2, r1
              enc_imm(Opcode::MOV, 3, 0, 0x1234),   // 8:  mov r3, #0x1234
              enc_zero(Opcode::NOP),                // 12: nop
              enc_halt(),                           // 16: halt
          },
          {},
          {},
          {{1, 5}, {2, 5}, {3, 0x1234}}};
}

// call/ret -- full-descending stack through the memory unit.
// Branch targets are PC-relative to the *next* PC: target = (addr+4) + offset*4.
Program_ prog_call() {
  return {"call/ret",
          {
              enc_imm(Opcode::MOV, REG_SP, 0, 0x300),  // 0:  mov sp, #0x300
              enc_imm(Opcode::MOV, 1, 0, 7),           // 4:  mov r1, #7
              enc_branch(Opcode::CALL, 3),             // 8:  call +3 (-> 24)
              enc_halt(),                              // 12: halt
              enc_zero(Opcode::NOP),                   // 16: (padding)
              enc_zero(Opcode::NOP),                   // 20: (padding)
              enc_zero(Opcode::RET),                   // 24: ret
          },
          {},
          {},
          {{1, 7}, {REG_SP, 0x300}},
          0x2FC,
          12};  // the return address pushed by the call (PC after its preamble)
}

// beq/bne/bgt/blt with pre-seeded Z=0, N=0 (so GT=1):
//   target = (addr+4) + offset*4
//   0:  beq +4  -> 20, not taken
//   4:  bne +3  -> 20, taken
//   20: bgt +1  -> 28, taken
//   28: blt +0  -> 32, not taken
Program_ prog_branch() {
  Program_ p{"branches (beq/bne/bgt/blt)",
             {
                 enc_branch(Opcode::BEQ, 4),   // 0:  (-> 20, not taken)
                 enc_branch(Opcode::BNE, 3),   // 4:  (-> 20, taken)
                 enc_branch(Opcode::BGT, 2),   // 8:  (skipped)
                 enc_branch(Opcode::BLT, 2),   // 12: (skipped)
                 enc_branch(Opcode::B, -4),    // 16: (skipped -- trap)
                 enc_branch(Opcode::BGT, 1),   // 20: (-> 28, taken)
                 enc_branch(Opcode::B, -6),    // 24: (skipped -- trap)
                 enc_branch(Opcode::BLT, 0),   // 28: (-> 32, not taken)
                 enc_halt(),                   // 32: halt
             }};
  p.pre_seed = {{5, 0xDEAD}};
  p.expected_regs = {{5, 0xDEAD}};  // branches must not clobber GPRs
  p.seed_flags = true;
  p.seed_z = false;
  p.seed_n = false;
  return p;
}

// An undefined opcode must halt through the dispatch table's missing entry.
Program_ prog_illegal() {
  Program_ p{"illegal opcode", {enc_raw(0x3F)}};
  p.expected_reason = "illegal instruction";
  return p;
}

struct Snapshot {
  std::array<Word, NUM_REGISTERS> regs{};
  Word probe = 0;
  bool halted = false;
  bool faulted = false;
  std::string reason;
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

template <typename CU> const char* encoding_name();
template <> const char* encoding_name<HorizontalControlUnit>() { return "horizontal"; }
template <> const char* encoding_name<VerticalControlUnit>() { return "vertical"; }

template <typename CU>
Snapshot execute(StageLayout layout, const Program_& prog, bool emit) {
  Datapath dp(1024);
  dp.mem.load_words(0, prog.code);
  for (const auto& d : prog.data) dp.mem.write_word(d.first, d.second);
  for (const auto& s : prog.pre_seed) dp.rf.write(s.first, s.second);
  if (prog.seed_flags) {
    dp.regs.flags.Z = prog.seed_z;
    dp.regs.flags.N = prog.seed_n;
    dp.regs.flags_E = prog.seed_z;
    dp.regs.flags_GT = !prog.seed_n && !prog.seed_z;
  }

  const Program program = build_program();
  CU cu(dp, layout);
  cu.install(RomBuilder<CU>::build(program));

  if (emit) {
    std::printf("\n=== %s : %s : %s ===\n", encoding_name<CU>(),
                layout == StageLayout::SIX_STAGE ? "6-stage" : "4-stage", prog.name);
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
  snap.reason = dp.halt_reason;
  for (int i = 0; i < NUM_REGISTERS; ++i) snap.regs[i] = dp.rf.read(static_cast<std::uint8_t>(i));
  snap.probe = dp.mem.read_word(prog.probe_addr);

  if (emit) {
    std::printf("  -- halted after %d micro-cycles (%s) --\n", cycles,
                snap.reason.empty() ? "halt" : snap.reason.c_str());
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
  if (a.reason != b.reason) return false;
  for (int i = 0; i < NUM_REGISTERS; ++i) {
    if (a.regs[i] != b.regs[i]) return false;
  }
  return true;
}

// --- Coverage: every opcode must have a routine and a dispatch entry --------
int check_coverage() {
  std::printf("\n--- microcode coverage: all %d opcodes ---\n", kOpcodeCount);
  const Program p = build_program();
  const auto h = assemble_horizontal(p);
  const auto v = assemble_vertical(p);

  int bad = 0;
  for (Opcode op : kAllOpcodes) {
    bool in_program = false;
    for (const ProgramEntry& e : p) {
      if (e.opcode == op) { in_program = true; break; }
    }
    const bool h_ok = h.has_entry(static_cast<std::uint8_t>(op));
    const bool v_ok = v.has_entry(static_cast<std::uint8_t>(op));
    const bool ok = in_program && h_ok && v_ok;
    std::printf("  [%s] %-8s 0x%02X\n", ok ? "PASS" : "FAIL", opcode_name(op),
                static_cast<unsigned>(op));
    if (!ok) ++bad;
  }
  std::printf("  control memory: %lu words (horizontal), %lu words (vertical)\n",
              static_cast<unsigned long>(h.used()), static_cast<unsigned long>(v.used()));
  return bad;
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

  failures += check_coverage();

  const std::vector<Program_> programs = {prog_transfer(), prog_call(), prog_branch(), prog_illegal()};

  for (std::size_t i = 0; i < programs.size(); ++i) {
    const Program_& p = programs[i];
    // Only the first program is traced.
    const Snapshot hs6 = execute<HorizontalControlUnit>(StageLayout::SIX_STAGE, p, i == 0);
    const Snapshot vs6 = execute<VerticalControlUnit>(StageLayout::SIX_STAGE, p, false);
    const Snapshot hs4 = execute<HorizontalControlUnit>(StageLayout::FOUR_STAGE, p, false);
    const Snapshot vs4 = execute<VerticalControlUnit>(StageLayout::FOUR_STAGE, p, false);

    std::printf("\n--- %s ---\n", p.name);
    check(hs6.halted && vs6.halted && hs4.halted && vs4.halted && !hs6.faulted &&
              !vs6.faulted && !hs4.faulted && !vs4.faulted,
          "runs to halt without fault");
    if (p.expected_reason) check(hs6.reason == p.expected_reason, "halt reason correct");
    for (const auto& e : p.expected_regs) {
      check(hs6.regs[e.first] == e.second, "register value correct");
    }
    if (p.probe_addr != 0 || p.expected_probe != 0) {
      check(hs6.probe == p.expected_probe, "memory probe correct");
    }
    check(same(hs6, vs6), "horizontal == vertical (6-stage)");
    check(same(hs4, vs4), "horizontal == vertical (4-stage)");
    check(same(hs6, hs4), "6-stage == 4-stage");
  }

  std::printf("\n%s\n", failures == 0 ? "ALL CHECKS PASSED" : "CHECKS FAILED");
  return failures == 0 ? 0 : 1;
}
