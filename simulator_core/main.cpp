#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "isa_spec/isa.h"
#include "control_unit/control_rom.h"
#include "simulator_core/core/debugger.h"
#ifdef RISC201_HAS_TUI
#include "simulator_core/ui/tui.h"
#endif

namespace {

// text at 0, .data at 0x10000 (matches the assembler defaults)
constexpr Word kTextBase = 0x00000000;
constexpr Word kDataBase = 0x00010000;
constexpr std::size_t kMemWords = 0x20000 / 4;

bool parse_word(const std::string& s, Word& out) {
  try {
    std::size_t n = 0;
    out = static_cast<Word>(std::stoull(s, &n, 0));
    return n == s.size();
  } catch (...) {
    return false;
  }
}

bool parse_bounded_integer(const std::string& text, int min_value,
                           int max_value, int& out) {
  try {
    std::size_t parsed = 0;
    const int value = std::stoi(text, &parsed, 10);
    if (parsed != text.size() || value < min_value || value > max_value) {
      return false;
    }
    out = value;
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

bool load_hex_file(const std::string& path, std::vector<Word>& out) {
  std::ifstream in(path);
  if (!in) return false;
  out.clear();
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty() || line[0] == '#') continue;
    std::size_t n = 0;
    Word w = 0;
    try {
      w = static_cast<Word>(std::stoull(line, &n, 16));
    } catch (...) {
      n = 0;
    }
    if (n != line.size()) {
      std::printf("  bad line: %s\n", line.c_str());
      return false;
    }
    out.push_back(w);
  }
  return true;
}

void print_registers(const risc201::Debugger& dbg) {
  const risc201::StateView& v = dbg.view();
  std::vector<std::string> registers;
  char line[160];
  std::snprintf(line, sizeof(line), "  pc=0x%X  ir=0x%08X  microPC=%u  stage=%s",
                static_cast<unsigned>(v.pc), static_cast<unsigned>(v.ir),
                static_cast<unsigned>(v.micro_pc), v.stage);
  registers.emplace_back(line);
  std::snprintf(line, sizeof(line), "  flags: Z=%d N=%d C=%d V=%d", v.flags.Z,
                v.flags.N, v.flags.C, v.flags.V);
  registers.emplace_back(line);
  for (int row = 0; row < NUM_REGISTERS / 4; ++row) {
    std::snprintf(line, sizeof(line), "  r%-2d=0x%-8X  r%-2d=0x%-8X  r%-2d=0x%-8X  r%-2d=0x%-8X",
                  row * 4, static_cast<unsigned>(v.gprs[row * 4]),
                  row * 4 + 1, static_cast<unsigned>(v.gprs[row * 4 + 1]),
                  row * 4 + 2, static_cast<unsigned>(v.gprs[row * 4 + 2]),
                  row * 4 + 3, static_cast<unsigned>(v.gprs[row * 4 + 3]));
    registers.emplace_back(line);
  }
  if (v.fault_code != ExceptionCode::NONE) {
    std::snprintf(line, sizeof(line), "  FAULT: %s @ 0x%llX",
                  to_string(v.fault_code),
                  static_cast<unsigned long long>(v.fault_address));
    registers.emplace_back(line);
  } else if (v.halted) {
    std::snprintf(line, sizeof(line), "  halted: %s",
                  (v.halt_reason[0] != '\0') ? v.halt_reason : "halt");
    registers.emplace_back(line);
  }

  std::istringstream stack(dbg.render_stack(16));
  std::string stack_line;
  std::vector<std::string> stack_lines;
  while (std::getline(stack, stack_line)) stack_lines.push_back(stack_line);

  const std::size_t rows = std::max(registers.size(), stack_lines.size());
  for (std::size_t i = 0; i < rows; ++i) {
    const char* register_line = i < registers.size() ? registers[i].c_str() : "";
    const char* stack_text = i < stack_lines.size() ? stack_lines[i].c_str() : "";
    std::printf("%-66s%s\n", register_line, stack_text);
  }
}

void print_fregs(const risc201::Debugger& dbg) {
  const risc201::StateView& v = dbg.view();
  std::printf("  --- Floating-Point Registers (IEEE-754) ---\n");
  char line[160];
  for (int row = 0; row < NUM_FLOAT_REGISTERS / 2; ++row) {
    const int a = row * 2;
    const int b = a + 1;
    float fa = 0.0f, fb = 0.0f;
    std::memcpy(&fa, &v.fprs[a], sizeof(float));
    std::memcpy(&fb, &v.fprs[b], sizeof(float));
    std::snprintf(line, sizeof(line), "  f%-2d=0x%-8X (%-10.4f)   f%-2d=0x%-8X (%-10.4f)",
                  a, static_cast<unsigned>(v.fprs[a]), fa,
                  b, static_cast<unsigned>(v.fprs[b]), fb);
    std::printf("%s\n", line);
  }
}

void print_pipeline_state(const risc201::Debugger& dbg) {
  const risc201::StateView& v = dbg.view();
  const auto& stats = v.stats;
  std::printf("  --- Pipeline Execution State (%s) ---\n", v.stage);
  std::printf("  [IF]     pc=0x%-8X raw=0x%08X valid=%d\n",
              static_cast<unsigned>(v.if_of.pc), static_cast<unsigned>(v.if_of.raw), v.if_of.valid ? 1 : 0);
  std::printf("  [OF/EX]  pc=0x%-8X op=%-8s rd=%-2d rs1=%-2d rs2=%-2d imm=0x%X valid=%d\n",
              static_cast<unsigned>(v.of_ex.pc), opcode_name(v.of_ex.opcode),
              v.of_ex.rd, v.of_ex.rs1, v.of_ex.rs2, static_cast<unsigned>(v.of_ex.imm32), v.of_ex.valid ? 1 : 0);
  std::printf("  [EX]     pc=0x%-8X alu=0x%-8X eff_addr=0x%-8X store_val=0x%X valid=%d\n",
              static_cast<unsigned>(v.ex_stage.pc), static_cast<unsigned>(v.ex_stage.alu_result),
              static_cast<unsigned>(v.ex_stage.effective_addr), static_cast<unsigned>(v.ex_stage.store_val), v.ex_stage.valid ? 1 : 0);
  if (std::string(v.stage).find("6") != std::string::npos) {
    std::printf("  [MEMCHK] pc=0x%-8X v_addr=0x%-8X valid=%d\n",
                static_cast<unsigned>(v.chk_ma.pc), static_cast<unsigned>(v.chk_ma.verified_addr), v.chk_ma.valid ? 1 : 0);
  }
  std::printf("  [MA/RW]  pc=0x%-8X wb_rd=%-2d wb_data=0x%-8X is_fpu=%d valid=%d\n",
              static_cast<unsigned>(v.ma_rw.pc), v.ma_rw.rd, static_cast<unsigned>(v.ma_rw.wb_data),
              v.ma_rw.is_fpu_reg ? 1 : 0, v.ma_rw.valid ? 1 : 0);
  std::printf("  [Stats]  cycles=%zu retired=%zu CPI=%.2f IPC=%.2f stalls=%zu flushes=%zu fwd=%zu\n",
              stats.total_cycles, stats.instructions_retired, stats.cpi(), stats.ipc(),
              stats.load_use_stalls, stats.branch_flushes, stats.forwarding_events);
}

void print_micro_state(const risc201::Debugger& dbg) {
  const risc201::StateView& v = dbg.view();
  const risc201::cu::InternalRegisters& m = v.micro;
  std::printf("  [micro] A=0x%X B=0x%X alu=0x%X  mar=0x%X mdr=0x%X lmd=0x%X\n",
              static_cast<unsigned>(m.A), static_cast<unsigned>(m.B),
              static_cast<unsigned>(m.alu_result), static_cast<unsigned>(m.mar),
              static_cast<unsigned>(m.mdr), static_cast<unsigned>(m.ld_result));
  std::printf("          regsrc=0x%X regval=0x%X regdata=0x%X  btgt=0x%X immx=0x%X\n",
              static_cast<unsigned>(m.reg_src), static_cast<unsigned>(m.reg_val),
              static_cast<unsigned>(m.reg_data),
              static_cast<unsigned>(m.branch_target), static_cast<unsigned>(m.immx));
  std::printf("          I=%d rd=%u rs1=%u rs2=%u  E=%d GT=%d\n", m.I ? 1 : 0,
              static_cast<unsigned>(m.rd), static_cast<unsigned>(m.rs1),
              static_cast<unsigned>(m.rs2), m.flags_E ? 1 : 0, m.flags_GT ? 1 : 0);

  const Opcode op = opcode_of(v.ir);
  const bool is_complex = (op == Opcode::CALL || op == Opcode::RET);
  const std::size_t rom_base = risc201::cu::rom_address(op, 0);
  std::printf("  Control ROM entry: 0x%02X (%s, %s)\n",
              static_cast<unsigned>(rom_base), opcode_name(op),
              is_complex ? "cracked 2-uop sequence" : "fast-path 1-cycle uop");
  if (dbg.at_instruction_boundary())
    std::printf("  -- end of instruction (%zu micro-ops)\n",
                dbg.micro_ops_last_instruction());
  else
    std::printf("  (step uop %u)\n", static_cast<unsigned>(v.micro_pc & 0x1));
}

void print_watch_changes(risc201::Debugger& dbg) {
  for (std::uint8_t r : dbg.changed_registers()) {
    std::printf("  watch: r%u = 0x%X\n", static_cast<unsigned>(r),
                static_cast<unsigned>(dbg.cpu().read_reg(r)));
  }
}

void print_help() {
  std::printf(
      "  step | s              step one instruction\n"
      "  micro | mi            step one micro-cycle (shows micro-registers)\n"
      "  run | r               run to completion\n"
      "  continue | c          run to next breakpoint or completion\n"
      "  break <addr> | b      set breakpoint\n"
      "  delete <addr> | d     remove breakpoint\n"
      "  breaks                list breakpoints\n"
      "  watch <reg> | w       watch a register\n"
      "  unwatch <reg>         stop watching a register\n"
      "  regs                  dump registers and flags\n"
      "  fregs | fr            dump floating-point registers (f0..f15)\n"
      "  pipe | pipeline       show live pipeline latches and statistics\n"
      "  set <rN|fN> <val>     set register or FPR value\n"
      "  mem [addr] [count]    dump memory words\n"
      "  stack [window]        show stack words, SP and call-frame boundaries\n"
      "  stack trace [steps] [window]  render stack after each instruction with diffs\n"
      "  load <text> [data]    load hex program (text at 0x0, data at 0x10000)\n"
      "  reset                 reset the machine\n"
      "  help | h              this help\n"
      "  quit | q              exit\n");
}

}  // namespace

int main(int argc, char* argv[]) {
  risc201::cu::StageLayout layout = risc201::cu::StageLayout::SIX_STAGE;
  risc201::ControlEncoding encoding = risc201::ControlEncoding::HORIZONTAL;
  bool use_tui = false;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--help" || arg == "-h") {
      std::printf("Usage: risc201-sim [--tui|--headless] [--stages 4|6] [--encoding horizontal|vertical] [--help|-h]\n"
                  "  --tui       Launch the full-screen debugger (requires RISC201_TUI=ON)\n"
                  "  --headless  Launch the line-based debugger\n"
                  "  --stages    Select the 4-stage or 6-stage microprogram (default: 6)\n"
                  "  --encoding  Select horizontal or vertical control encoding (default: horizontal)\n\n"
                  "Commands:\n");
      print_help();
      return 0;
    }
    if (arg == "--tui") {
      use_tui = true;
    } else if (arg == "--headless") {
      use_tui = false;
    } else if (arg == "--stages" && i + 1 < argc) {
      const std::string value = argv[++i];
      if (value == "4") {
        layout = risc201::cu::StageLayout::FOUR_STAGE;
      } else if (value == "6") {
        layout = risc201::cu::StageLayout::SIX_STAGE;
      } else {
        std::printf("Invalid stage count '%s' (expected 4 or 6)\n", value.c_str());
        return 1;
      }
    } else if (arg == "--encoding" && i + 1 < argc) {
      const std::string value = argv[++i];
      if (value == "horizontal") {
        encoding = risc201::ControlEncoding::HORIZONTAL;
      } else if (value == "vertical") {
        encoding = risc201::ControlEncoding::VERTICAL;
      } else {
        std::printf("Invalid encoding '%s' (expected horizontal or vertical)\n", value.c_str());
        return 1;
      }
    } else {
      std::printf("Unknown or incomplete option: %s\nUse --help for usage.\n", arg.c_str());
      return 1;
    }
  }

  risc201::Debugger dbg(kMemWords, layout, encoding);

  if (use_tui) {
#ifdef RISC201_HAS_TUI
    return risc201::run_tui(
        dbg, encoding == risc201::ControlEncoding::VERTICAL,
        static_cast<unsigned>(layout));
#else
    std::printf("This build has no TUI. Reconfigure with -DRISC201_TUI=ON.\n");
    return 1;
#endif
  }

  std::printf("RISC201 debugger (%s, %u-stage). Type 'help' for commands.\n",
              encoding == risc201::ControlEncoding::HORIZONTAL ? "horizontal" : "vertical",
              static_cast<unsigned>(layout));
  print_registers(dbg);

  std::string line;
  for (;;) {
    std::printf("(risc201) ");
    if (!std::getline(std::cin, line)) break;
    if (line.empty()) continue;

    std::istringstream iss(line);
    std::string cmd;
    iss >> cmd;

    if (cmd == "step" || cmd == "s") {
      dbg.step_instruction();
      print_registers(dbg);
      print_watch_changes(dbg);
    } else if (cmd == "micro" || cmd == "mi") {
      dbg.step_micro();
      print_registers(dbg);
      print_micro_state(dbg);
      print_watch_changes(dbg);
    } else if (cmd == "run" || cmd == "r") {
      dbg.run();
      print_registers(dbg);
      print_watch_changes(dbg);
    } else if (cmd == "continue" || cmd == "c") {
      dbg.run_to_breakpoint();
      print_registers(dbg);
      if (dbg.at_breakpoint()) std::printf("  hit breakpoint at 0x%X\n",
                                            static_cast<unsigned>(dbg.cpu().pc()));
      print_watch_changes(dbg);
    } else if (cmd == "break" || cmd == "b") {
      Word addr = 0;
      std::string a;
      if (iss >> a && parse_word(a, addr)) {
        dbg.add_breakpoint(addr);
        std::printf("  breakpoint at 0x%X\n", static_cast<unsigned>(addr));
      } else {
        std::printf("  usage: break <addr>\n");
      }
    } else if (cmd == "delete" || cmd == "d") {
      Word addr = 0;
      std::string a;
      if (iss >> a && parse_word(a, addr)) {
        dbg.remove_breakpoint(addr);
        std::printf("  removed breakpoint at 0x%X\n", static_cast<unsigned>(addr));
      } else {
        std::printf("  usage: delete <addr>\n");
      }
    } else if (cmd == "breaks") {
      for (Word a : dbg.breakpoints())
        std::printf("  0x%X\n", static_cast<unsigned>(a));
    } else if (cmd == "watch" || cmd == "w") {
      std::string a;
      if (iss >> a) {
        Word r = 0;
        if (parse_word(a, r) && r < NUM_REGISTERS) {
          dbg.watch(static_cast<std::uint8_t>(r));
          std::printf("  watching r%u\n", static_cast<unsigned>(r));
        } else {
          std::printf("  usage: watch <reg 0..15>\n");
        }
      }
    } else if (cmd == "unwatch") {
      std::string a;
      if (iss >> a) {
        Word r = 0;
        if (parse_word(a, r)) dbg.unwatch(static_cast<std::uint8_t>(r));
      }
    } else if (cmd == "regs") {
      print_registers(dbg);
    } else if (cmd == "fregs" || cmd == "fr") {
      print_fregs(dbg);
    } else if (cmd == "pipe" || cmd == "pipeline") {
      print_pipeline_state(dbg);
    } else if (cmd == "set") {
      std::string target, val_str;
      if (iss >> target >> val_str) {
        Word val = 0;
        if (parse_word(val_str, val)) {
          if (target.size() >= 2 && target[0] == 'r') {
            try {
              int reg = std::stoi(target.substr(1));
              if (reg >= 0 && reg < NUM_REGISTERS) {
                dbg.write_reg(static_cast<std::uint8_t>(reg), val);
                std::printf("  set r%d = 0x%X\n", reg, static_cast<unsigned>(val));
              }
            } catch (...) {}
          } else if (target.size() >= 2 && target[0] == 'f') {
            try {
              int freg = std::stoi(target.substr(1));
              if (freg >= 0 && freg < NUM_FLOAT_REGISTERS) {
                dbg.write_freg(static_cast<std::uint8_t>(freg), val);
                std::printf("  set f%d = 0x%X\n", freg, static_cast<unsigned>(val));
              }
            } catch (...) {}
          }
        } else {
          std::printf("  usage: set <rN|fN> <hex_or_dec_val>\n");
        }
      } else {
        std::printf("  usage: set <rN|fN> <hex_or_dec_val>\n");
      }
    } else if (cmd == "mem") {
      Word addr = 0;
      std::size_t count = 16;
      std::string a, c;
      if (iss >> a) {
        if (parse_word(a, addr)) {
          if (iss >> c) count = static_cast<std::size_t>(std::stoul(c));
        }
      }
      for (std::size_t i = 0; i < count; ++i) {
        const Word byte = addr + static_cast<Word>(i * 4);
        try {
          std::printf("  0x%X: 0x%08X\n", static_cast<unsigned>(byte),
                      static_cast<unsigned>(dbg.cpu().read_mem(byte)));
        } catch (const RuntimeException&) {
          std::printf("  0x%X: <out of range>\n", static_cast<unsigned>(byte));
          break;
        }
      }
    } else if (cmd == "stack" || cmd == "stackviz") {
      std::string mode;
      iss >> mode;
      if (mode == "trace") {
        int max_steps = 64;
        int window = 8;
        std::string steps_arg, window_arg, extra;
        bool valid_args = true;
        if (iss >> steps_arg) {
          valid_args =
              parse_bounded_integer(steps_arg, 1, 1000, max_steps);
        }
        if (valid_args && (iss >> window_arg)) {
          valid_args = parse_bounded_integer(window_arg, 1, 64, window);
        }
        if (iss >> extra) valid_args = false;
        if (!valid_args) {
          std::printf("  usage: stack trace [steps 1..1000] [window 1..64]\n");
        } else {
          int traced = 0;
          while (!dbg.cpu().halted() && traced < max_steps) {
            const Word before_pc = dbg.cpu().pc();
            const Word before_sp = dbg.cpu().stack_pointer();
            const std::vector<Word> before_stack =
                dbg.cpu().stack_memory_snapshot();
            dbg.step_instruction();
            std::printf("[STACK TRACE] instruction at 0x%X -> PC 0x%X\n",
                        static_cast<unsigned>(before_pc),
                        static_cast<unsigned>(dbg.cpu().pc()));
            std::printf("%s",
                        dbg.cpu()
                            .render_stack_diff(before_stack, before_sp)
                            .c_str());
            std::printf("%s", dbg.render_stack(window).c_str());
            ++traced;
          }
          if (traced == max_steps && !dbg.cpu().halted()) {
            std::printf("  trace stopped at %d instruction(s); CPU remains "
                        "runnable\n",
                        traced);
          }
          print_registers(dbg);
        }
      } else {
        int window = 8;
        std::string extra;
        const bool valid_window =
            mode.empty() || parse_bounded_integer(mode, 1, 64, window);
        if (!valid_window || (iss >> extra)) {
          std::printf("  usage: stack [window 1..64] | stack trace [steps] "
                      "[window]\n");
        } else {
          std::printf("%s", dbg.render_stack(window).c_str());
        }
      }
    } else if (cmd == "load") {
      std::string path, dataPath;
      if (iss >> path) {
        iss >> dataPath;
        std::vector<Word> code;
        if (!load_hex_file(path, code)) {
          std::printf("  failed to load '%s'\n", path.c_str());
        } else {
          dbg.load(kTextBase, code);
          std::printf("  loaded %zu text word(s) at 0x%X\n", code.size(),
                      static_cast<unsigned>(kTextBase));
          if (!dataPath.empty()) {
            std::vector<Word> data;
            if (!load_hex_file(dataPath, data)) {
              std::printf("  failed to load data '%s'\n", dataPath.c_str());
            } else {
              dbg.load(kDataBase, data);
              std::printf("  loaded %zu data word(s) at 0x%X\n", data.size(),
                          static_cast<unsigned>(kDataBase));
            }
          }
          print_registers(dbg);
        }
      } else {
        std::printf("  usage: load <text.hex> [data.hex]\n");
      }
    } else if (cmd == "reset") {
      dbg.reset();
      print_registers(dbg);
    } else if (cmd == "help" || cmd == "h") {
      print_help();
    } else if (cmd == "quit" || cmd == "q") {
      break;
    } else {
      std::printf("  unknown command '%s' (try 'help')\n", cmd.c_str());
    }
  }

  std::printf("\n");
  return 0;
}
