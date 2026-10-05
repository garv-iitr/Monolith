#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "isa_spec/isa.h"
#include "simulator_core/core/debugger.h"

namespace {

Word enc_imm(Opcode op, int rd, int rs1, int imm) {
  return (static_cast<Word>(op) << 26) | (1u << 24) |
         (static_cast<Word>(rd & 0xF) << 20) | (static_cast<Word>(rs1 & 0xF) << 16) |
         static_cast<Word>(imm & 0xFFFF);
}

Word enc_reg(Opcode op, int rd, int rs1, int rs2) {
  return (static_cast<Word>(op) << 26) | (static_cast<Word>(rd & 0xF) << 20) |
         (static_cast<Word>(rs1 & 0xF) << 16) | (static_cast<Word>(rs2 & 0xF) << 12);
}

Word enc_halt() { return static_cast<Word>(Opcode::HALT) << 26; }

std::vector<Word> demo_program() {
  return {
      enc_imm(Opcode::MOV, 1, 0, 6),
      enc_imm(Opcode::MOV, 2, 0, 7),
      enc_reg(Opcode::MUL, 3, 1, 2),
      enc_reg(Opcode::ADD, 4, 1, 2),
      enc_reg(Opcode::SUB, 5, 2, 1),
      enc_halt(),
  };
}

bool parse_word(const std::string& s, Word& out) {
  try {
    std::size_t n = 0;
    out = static_cast<Word>(std::stoull(s, &n, 0));
    return n == s.size();
  } catch (...) {
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
    Word w = 0;
    if (!parse_word(line, w)) {
      std::printf("  bad line: %s\n", line.c_str());
      return false;
    }
    out.push_back(w);
  }
  return true;
}

void print_registers(const risc201::Debugger& dbg) {
  const risc201::StateView& v = dbg.view();
  std::printf("  pc=0x%X  ir=0x%08X  microPC=%u  stage=%s\n",
              static_cast<unsigned>(v.pc), static_cast<unsigned>(v.ir),
              static_cast<unsigned>(v.micro_pc), v.stage);
  std::printf("  flags: Z=%d N=%d C=%d V=%d\n", v.flags.Z, v.flags.N, v.flags.C,
              v.flags.V);
  for (int i = 0; i < NUM_REGISTERS; ++i) {
    std::printf("  r%-2d=0x%-8X%s", i, static_cast<unsigned>(v.gprs[i]),
                (i % 4 == 3) ? "\n" : "");
  }
  if (v.fault_code != ExceptionCode::NONE)
    std::printf("  FAULT: %s @ 0x%llX\n", to_string(v.fault_code),
                static_cast<unsigned long long>(v.fault_address));
  else if (v.halted)
    std::printf("  halted: %s\n", (v.halt_reason[0] != '\0') ? v.halt_reason : "halt");
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
      "  micro | mi            step one micro-cycle\n"
      "  run | r               run to completion\n"
      "  continue | c          run to next breakpoint or completion\n"
      "  break <addr> | b      set breakpoint\n"
      "  delete <addr> | d     remove breakpoint\n"
      "  breaks                list breakpoints\n"
      "  watch <reg> | w       watch a register\n"
      "  unwatch <reg>         stop watching a register\n"
      "  regs                  dump registers and flags\n"
      "  mem [addr] [count]    dump memory words\n"
      "  load <file>           load hex program (one word per line)\n"
      "  demo                  reload the built-in demo program\n"
      "  reset                 reset the machine\n"
      "  help | h              this help\n"
      "  quit | q              exit\n");
}

}  // namespace

int main() {
  risc201::Debugger dbg;
  dbg.load(demo_program());

  std::printf("RISC201 debugger. Type 'help' for commands.\n");
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
    } else if (cmd == "load") {
      std::string path;
      if (iss >> path) {
        std::vector<Word> code;
        if (load_hex_file(path, code)) {
          dbg.load(code);
          std::printf("  loaded %zu words\n", code.size());
          print_registers(dbg);
        } else {
          std::printf("  failed to load '%s'\n", path.c_str());
        }
      }
    } else if (cmd == "demo") {
      dbg.load(demo_program());
      print_registers(dbg);
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
