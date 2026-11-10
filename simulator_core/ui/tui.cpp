#include "simulator_core/ui/tui.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include "isa_spec/isa.h"
#include "memory_safety/visualizer.h"
#include "simulator_core/core/debugger.h"

namespace risc201 {
namespace {

using namespace ftxui;

struct SourceLine {
  int number = 0;
  std::string text;
  std::vector<Word> addresses;
};

std::string read_file(const std::string& path) {
  std::ifstream input(path);
  if (!input) throw std::runtime_error("Could not open " + path);
  std::ostringstream contents;
  contents << input.rdbuf();
  return contents.str();
}

std::string hex_value(Word value) {
  std::ostringstream output;
  output << std::uppercase << std::hex << static_cast<unsigned long long>(value);
  return output.str();
}

std::string find_risc201asm_bin() {
  const std::vector<std::string> candidates = {
      "build/risc201asm.exe",
      "build/risc201asm",
      "build/Release/risc201asm.exe",
      "build/Debug/risc201asm.exe",
      "../build/risc201asm.exe",
      "../build/Release/risc201asm.exe",
      "risc201asm.exe",
      "risc201asm"
  };
  for (const auto& path : candidates) {
    std::ifstream f(path);
    if (f.good()) return path;
  }
  return "risc201asm";
}

std::vector<SourceLine> source_lines(const std::string& source,
                                    const std::vector<Word>& words) {
  std::vector<SourceLine> lines;
  std::istringstream input(source);
  std::string text;
  Word addr = 0;
  while (std::getline(input, text)) {
    SourceLine line;
    line.number = static_cast<int>(lines.size()) + 1;
    line.text = text;
    size_t non_ws = text.find_first_not_of(" \t\r\n");
    if (non_ws != std::string::npos && text[non_ws] != '#' && text[non_ws] != '.' &&
        text.find(':') == std::string::npos && addr / 4 < words.size()) {
      std::istringstream tok_stream(text.substr(non_ws));
      std::string mnem;
      tok_stream >> mnem;
      std::string upper_mnem = mnem;
      for (char& c : upper_mnem) c = static_cast<char>(std::toupper(c));

      int word_count = 1;
      if (upper_mnem == "LI") {
        word_count = 2;
      } else if (upper_mnem == "PUSH" || upper_mnem == "POP" ||
                 upper_mnem == "FPUSH" || upper_mnem == "FPOP") {
        word_count = 2;
      } else if (upper_mnem == "SPUSH" || upper_mnem == "SPOP" ||
                 upper_mnem == "SFPUSH" || upper_mnem == "SFPOP") {
        word_count = 3;
      }

      for (int w = 0; w < word_count && (addr / 4) < words.size(); ++w) {
        line.addresses.push_back(addr);
        addr += 4;
      }
    }
    lines.push_back(std::move(line));
  }
  return lines;
}

std::string status_text(const Debugger& debugger) {
  const StateView& view = debugger.view();
  if (view.fault_code != ExceptionCode::NONE) {
    return std::string("FAULT  ") + to_string(view.fault_code) + " @ 0x" +
           hex_value(static_cast<Word>(view.fault_address));
  }
  if (view.halted) return std::string("HALTED  ") + view.halt_reason;
  if (debugger.at_breakpoint()) return "BREAKPOINT";
  return "READY";
}

Color status_color(const Debugger& debugger) {
  if (debugger.cpu().faulted()) return Color::Red;
  if (debugger.cpu().halted()) return Color::GrayDark;
  if (debugger.at_breakpoint()) return Color::Yellow;
  return Color::RGB(100, 220, 150);
}

std::string register_name(int index) {
  if (index == REG_SP) return "sp";
  if (index == REG_RA) return "ra";
  return "r" + std::to_string(index);
}

Element pane(std::string title, Element content, bool focused) {
  Element inset = hbox({text(" "),
                        vbox({text(" "), std::move(content), text(" ")}) | flex,
                        text(" ")}) | flex;
  return window(text(" " + title + " "), inset) |
         color(focused ? Color::Cyan : Color::RGB(92, 108, 128));
}

// one stack slot, themed to match the panes: SP glows, frames are tagged
Element stack_row_element(const risc201::safety::StackRow& row) {
  std::ostringstream addr;
  addr << "0x" << std::uppercase << std::hex << std::setw(5)
       << std::setfill('0') << row.address;

  Element pointer = text(row.is_sp ? " \u25B8 " : "   ") |
                    color(row.is_sp ? Color::GreenLight : Color::GrayDark);
  Element address = text(addr.str() + "  ") |
                    color(row.is_sp ? Color::CyanLight : Color::RGB(120, 140, 160));
  Color value_color = Color::White;
  if (row.is_sp) value_color = Color::GreenLight;
  else if (!row.in_memory) value_color = Color::GrayDark;

  Element value = text(row.in_memory ? "0x" + hex_value(row.value)
                                     : std::string("  <top>")) |
                  color(value_color);

  Element line = hbox({std::move(pointer), std::move(address), std::move(value)});
  if (row.is_sp) line = line | bold;

  Elements tags = {std::move(line)};
  if (row.is_sp) tags.push_back(text("   SP") | color(Color::GreenLight) | bold);
  if (row.is_fp) tags.push_back(text("   FP") | color(Color::Magenta));
  if (row.is_frame) tags.push_back(text("   CALL FRAME") | color(Color::Yellow));
  if (row.is_limit) tags.push_back(text("   LIMIT") | color(Color::RGB(130, 200, 255)));
  if (row.is_base) tags.push_back(text("   BASE") | color(Color::RGB(130, 200, 255)));
  return hbox(std::move(tags));
}

Elements stack_pane_rows(const risc201::safety::StackTable& table) {
  Elements rows;
  rows.push_back(hbox({
      text(" SP 0x" + hex_value(table.sp)) |
          color(table.valid_sp ? Color::GreenLight : Color::Yellow) | bold,
      table.empty ? text("  empty") | color(Color::GrayDark) : text(""),
      filler(),
      text("LIMIT 0x" + hex_value(table.limit) + "  BASE 0x" +
           hex_value(table.base) + " ") |
          color(Color::GrayDark),
  }));
  rows.push_back(separator());
  for (const auto& row : table.rows) rows.push_back(stack_row_element(row));
  rows.push_back(separator());
  rows.push_back(text(" \u25B8 SP \u00B7 CALL FRAME holds a saved return address \u00B7 grows down") |
                 color(Color::GrayDark));
  return rows;
}

class SimulatorTui : public ComponentBase {
 public:
  SimulatorTui(Debugger& debugger, bool vertical, unsigned stages)
      : debugger_(debugger), vertical_(vertical), stages_(stages) {}

  Element Render() override {
    const StateView& view = debugger_.view();

    Elements source_rows;
    if (source_.empty()) {
      source_rows.push_back(text("Open an .asm file with o") | color(Color::GrayDark));
    } else {
      for (std::size_t i = 0; i < source_.size(); ++i) {
        const auto& source_line = source_[i];
        std::ostringstream content;
        content << std::setw(4) << source_line.number << "  " << source_line.text;
        const int source_start = std::max(0, source_selection_ - 6);
        if (static_cast<int>(i) < source_start || static_cast<int>(i) >= source_start + 18)
          continue;
        const bool current = static_cast<int>(i) == current_source_line_;
        const bool selected = static_cast<int>(i) == source_selection_;
        Element row = text(content.str());
        if (current) row = row | color(Color::GreenLight) | bold;
        if (selected && focus_ == 0) row = row | bgcolor(Color::RGB(46, 58, 76));
        source_rows.push_back(row);
      }
    }

    Elements register_rows;
    for (int row = 0; row < NUM_REGISTERS / 2; ++row) {
      const int a = row * 2;
      const int b = a + 1;
      Element register_row = hbox({
          text(register_name(a)) | color(Color::RGB(238, 190, 92)) | bold,
          text(" 0x" + hex_value(view.gprs[a])) | color(Color::White),
          text("   " + register_name(b)) | color(Color::RGB(238, 190, 92)) | bold,
          text(" 0x" + hex_value(view.gprs[b])) | color(Color::White),
      });
      register_rows.push_back(register_row);
    }
    register_rows.push_back(separator());
    register_rows.push_back(text("FPU REGISTERS (IEEE-754)") | color(Color::Cyan) | bold);
    for (int row = 0; row < NUM_FLOAT_REGISTERS / 2; ++row) {
      const int a = row * 2;
      const int b = a + 1;
      Element freg_row = hbox({
          text("f" + std::to_string(a)) | color(Color::RGB(130, 200, 255)) | bold,
          text(" 0x" + hex_value(view.fprs[a])) | color(Color::White),
          text("   f" + std::to_string(b)) | color(Color::RGB(130, 200, 255)) | bold,
          text(" 0x" + hex_value(view.fprs[b])) | color(Color::White),
      });
      register_rows.push_back(freg_row);
    }

    const std::string stage_str = (view.stage && view.stage[0] != '\0') ? view.stage : (stages_ == 4 ? "4-STAGE" : "6-STAGE");

    Elements execution_rows = {
        text("Retire PC  0x" + hex_value(view.pc)) | color(Color::Cyan),
        text("Fetch PC   0x" + hex_value(debugger_.cpu().pipe().fetch_pc())) | color(Color::CyanLight),
        text("Decode IR  0x" + hex_value(view.ir) + " (" + std::string(opcode_name(opcode_of(view.ir))) + ")") |
            color(Color::RGB(238, 190, 92)) | bold,
        text("Mode       " + stage_str + " / " + (vertical_ ? "Vertical" : "Horizontal")) |
            color(Color::Cyan),
        text("Flags      Z=" + bit(view.flags.Z) + " N=" + bit(view.flags.N) +
             " C=" + bit(view.flags.C) + " V=" + bit(view.flags.V)) |
            color(Color::RGB(160, 190, 220)),
        separator(),
        text("PIPELINE STAGE LATCHES") | bold | color(Color::Yellow),
    };

    // [IF/OF] Latch
    if (!view.if_of.valid || view.if_of.bubble) {
      execution_rows.push_back(text("[IF/OF]  [BUBBLE / FLUSH]") | color(Color::RGB(120, 140, 160)));
    } else {
      execution_rows.push_back(text("[IF/OF]  PC=0x" + hex_value(view.if_of.pc) +
                                    " RAW=0x" + hex_value(view.if_of.raw) +
                                    " (" + std::string(opcode_name(opcode_of(view.if_of.raw))) + ")"));
    }

    // [OF/EX] Latch
    if (!view.of_ex.valid || view.of_ex.bubble) {
      execution_rows.push_back(text("[OF/EX]  [BUBBLE / STALL]") | color(Color::RGB(120, 140, 160)));
    } else {
      std::string of_str = "[OF/EX]  OP=" + std::string(opcode_name(view.of_ex.opcode));
      if (view.of_ex.rd != 0 || view.of_ex.control.wb.reg_write_en) {
        of_str += " RD=" + std::string(view.of_ex.dest_is_fpu ? "f" : "r") + std::to_string(view.of_ex.rd);
      }
      of_str += " RS1=" + std::string(view.of_ex.rs1_is_fpu ? "f" : "r") + std::to_string(view.of_ex.rs1) +
                "(0x" + hex_value(view.of_ex.rs1_val) + ")";
      if (view.of_ex.control.ex.operand_b_src == risc201::cu::OperandBSrc::IMM) {
        of_str += " IMM=0x" + hex_value(view.of_ex.imm32);
      } else {
        of_str += " RS2=" + std::string(view.of_ex.rs2_is_fpu ? "f" : "r") + std::to_string(view.of_ex.rs2) +
                  "(0x" + hex_value(view.of_ex.rs2_val) + ")";
      }
      execution_rows.push_back(text(of_str));
    }

    // [EX/CHK] (6-stage) or [EX/MA] (4-stage) Latch
    if (!view.ex_stage.valid || view.ex_stage.bubble) {
      execution_rows.push_back(text(stages_ == 6 ? "[EX/CHK] [BUBBLE]" : "[EX/MA]  [BUBBLE]") |
                               color(Color::RGB(120, 140, 160)));
    } else {
      std::string ex_str = (stages_ == 6 ? "[EX/CHK] " : "[EX/MA]  ");
      ex_str += "ALU=0x" + hex_value(view.ex_stage.alu_result);
      if (view.ex_stage.control.ma.mem_read || view.ex_stage.control.ma.mem_write ||
          view.ex_stage.control.chk.memchk_enable || view.ex_stage.control.ex.is_call ||
          view.ex_stage.control.ex.is_ret) {
        ex_str += " ADDR=0x" + hex_value(view.ex_stage.effective_addr);
      }
      if (view.ex_stage.control.ma.mem_write) {
        ex_str += " VAL=0x" + hex_value(view.ex_stage.store_val);
      }
      if (view.ex_stage.branch_taken) {
        ex_str += " [BR -> 0x" + hex_value(view.ex_stage.target_pc) + "]";
      }
      execution_rows.push_back(text(ex_str));
    }

    // 6-stage latches: [CHK/MA], [MA/WB], [COMMIT]
    if (stages_ == 6) {
      // [CHK/MA]
      if (!view.chk_ma.valid || view.chk_ma.bubble) {
        execution_rows.push_back(text("[CHK/MA] [BUBBLE]") | color(Color::RGB(120, 140, 160)));
      } else if (view.chk_ma.control.chk.memchk_enable) {
        execution_rows.push_back(hbox({
            text("[CHK/MA] ADDR=0x" + hex_value(view.chk_ma.verified_addr) + " "),
            text("[VERIFIED SAFE]") | color(Color::Green) | bold,
        }));
      } else if (view.chk_ma.control.ma.mem_read || view.chk_ma.control.ma.mem_write ||
                 view.chk_ma.control.ex.is_call || view.chk_ma.control.ex.is_ret) {
        execution_rows.push_back(text("[CHK/MA] ADDR=0x" + hex_value(view.chk_ma.verified_addr) +
                                      " [STACK/PASS]") | color(Color::White));
      } else {
        execution_rows.push_back(text("[CHK/MA] [BYPASS - NO MEM ACCESS]") | color(Color::RGB(120, 140, 160)));
      }

      // [MA/WB]
      if (!view.ma_rw.valid || view.ma_rw.bubble) {
        execution_rows.push_back(text("[MA/WB]  [BUBBLE]") | color(Color::RGB(120, 140, 160)));
      } else if (view.ma_rw.control.ma.mem_read) {
        execution_rows.push_back(text("[MA/WB]  LOAD DATA=0x" + hex_value(view.ma_rw.wb_data) +
                                      " -> " + (view.ma_rw.is_fpu_reg ? "f" : "r") +
                                      std::to_string(view.ma_rw.rd)) | color(Color::Cyan));
      } else if (view.ma_rw.control.ma.mem_write) {
        execution_rows.push_back(text("[MA/WB]  STORE OK [0x" + hex_value(view.chk_ma.verified_addr) +
                                      "] <= 0x" + hex_value(view.chk_ma.store_val)) |
                                 color(Color::GreenLight));
      } else if (view.ma_rw.control.wb.reg_write_en && view.ma_rw.rd != 0) {
        execution_rows.push_back(text("[MA/WB]  ALU DATA=0x" + hex_value(view.ma_rw.wb_data) +
                                      " -> " + (view.ma_rw.is_fpu_reg ? "f" : "r") +
                                      std::to_string(view.ma_rw.rd)));
      } else {
        execution_rows.push_back(text("[MA/WB]  [PASS-THROUGH]") | color(Color::RGB(120, 140, 160)));
      }

      // [COMMIT]
      if (view.ma_rw.valid && !view.ma_rw.bubble) {
        if (view.ma_rw.control.halt) {
          execution_rows.push_back(text("[COMMIT] HALTED") | color(Color::Red) | bold);
        } else if (view.ma_rw.control.wb.reg_write_en && view.ma_rw.rd != 0) {
          execution_rows.push_back(text("[COMMIT] " + std::string(view.ma_rw.is_fpu_reg ? "f" : "r") +
                                        std::to_string(view.ma_rw.rd) + " <= 0x" +
                                        hex_value(view.ma_rw.wb_data)) | color(Color::Green) | bold);
        } else {
          execution_rows.push_back(text("[COMMIT] RETIRED (NO REG WRITE)") | color(Color::GrayLight));
        }
      } else {
        execution_rows.push_back(text("[COMMIT] [IDLE]") | color(Color::RGB(120, 140, 160)));
      }
    } else {
      // 4-stage [COMMIT]
      if (view.ex_stage.valid && !view.ex_stage.bubble) {
        if (view.ex_stage.control.halt) {
          execution_rows.push_back(text("[COMMIT] HALTED") | color(Color::Red) | bold);
        } else if (view.ex_stage.control.wb.reg_write_en && view.ex_stage.rd != 0) {
          execution_rows.push_back(text("[COMMIT] " + std::string(view.ex_stage.is_fpu_reg ? "f" : "r") +
                                        std::to_string(view.ex_stage.rd) + " <= 0x" +
                                        hex_value(view.ma_rw.wb_data)) | color(Color::Green) | bold);
        } else {
          execution_rows.push_back(text("[COMMIT] RETIRED") | color(Color::GrayLight));
        }
      } else {
        execution_rows.push_back(text("[COMMIT] [IDLE]") | color(Color::RGB(120, 140, 160)));
      }
    }

    execution_rows.push_back(separator());
    execution_rows.push_back(text("Cycles: " + std::to_string(view.stats.total_cycles) +
                                  "  Ret: " + std::to_string(view.stats.instructions_retired) +
                                  "  CPI: " + std::to_string(view.stats.cpi()).substr(0, 4)));
    execution_rows.push_back(text("Stalls: " + std::to_string(view.stats.load_use_stalls) +
                                  "  Flushes: " + std::to_string(view.stats.branch_flushes) +
                                  "  Fwd: " + std::to_string(view.stats.forwarding_events)));
    if (view.fault_code != ExceptionCode::NONE) {
      execution_rows.push_back(text("FAULT: " + std::string(to_string(view.fault_code))) | color(Color::Red) | bold);
    }

    Elements stack_memory_rows;
    if (show_memory_) {
      for (unsigned i = 0; i < 10; ++i) {
        const Word address = memory_address_ + static_cast<Word>(i * 4);
        try {
          stack_memory_rows.push_back(text("0x" + hex_value(address) + "  0x" +
              hex_value(debugger_.cpu().read_mem(address))));
        } catch (const RuntimeException&) {
          stack_memory_rows.push_back(text("0x" + hex_value(address) + "  <out of range>") |
                                      color(Color::Red));
          break;
        }
      }
    } else {
      stack_memory_rows = stack_pane_rows(debugger_.cpu().stack_table(14));
    }

    Element header = hbox({
        text(" RISC201 ") | bold | color(Color::Black) | bgcolor(Color::Cyan),
        text("  " + std::to_string(stages_) + "-STAGE ") | color(Color::White) | bold,
        text(vertical_ ? "VERTICAL" : "HORIZONTAL") |
            color(vertical_ ? Color::Yellow : Color::Green) | bold,
        filler(),
        text(" " + status_text(debugger_) + " ") | color(status_color(debugger_)) | bold,
        text(" | PC: 0x" + hex_value(view.pc) + " | Fetch: 0x" +
             hex_value(debugger_.cpu().pipe().fetch_pc()) + " ") | color(Color::CyanLight),
    });

    Elements upper = {
        pane("SOURCE  " + source_name_, vbox(std::move(source_rows)), focus_ == 0) | flex,
        pane("REGISTERS", vbox(std::move(register_rows)), focus_ == 1),
    };
    Elements lower = {
        pane("EXECUTION / INTERNAL STATE", vbox(std::move(execution_rows)), focus_ == 2) | flex,
        pane(show_memory_ ? "MEMORY" : "STACK", vbox(std::move(stack_memory_rows)),
             focus_ == 3) | flex,
    };

    Element footer = vbox({
        hbox({
            text(" s:step  u:back  r:run  b:break  Tab:pane  o:open  v:stack/mem  g:addr  x:reset  ?:help  q:quit") |
                color(Color::GrayLight),
            filler(),
            text(running_ ? "[RUNNING]" : "[PAUSED]") | color(running_ ? Color::GreenLight : Color::Yellow) | bold,
        }),
        hbox({
            text(" Status: ") | color(Color::Cyan) | bold,
            text(message_.empty() ? "Ready" : message_) |
                color(message_error_ ? Color::Red : Color::RGB(100, 220, 150)) | bold,
        }),
    });

    Element root = vbox({header, separator(), hbox(std::move(upper)) | flex,
                         hbox(std::move(lower)) | flex, separator(), footer}) |
                  bgcolor(Color::RGB(16, 20, 26)) | color(Color::GrayLight) | flex;
    if (show_help_) {
      root = dbox({root, center(vbox({
          text(" SOURCE-DRIVEN RISC201 DEBUGGER ") | bold | color(Color::Cyan),
          separator(),
          text("o       open and assemble an .asm file"),
          text("Down / Up in source: step forward / back one instruction"),
          text("s       step instruction; u       step back one instruction"),
          text("Back-step history keeps the most recent 4096 steps"),
          text("r       run / pause, b       toggle breakpoint at PC"),
          text("Tab     change focused pane"),
          text("v       toggle stack / memory; g       memory address"),
          text("x       reset loaded program; q       quit"),
          text("Press F1 or Escape to close") | color(Color::GrayDark),
      }) | border | color(Color::Cyan))});
    }
    if (show_input_) {
      const int dialog_width = std::max(20, Terminal::Size().dimx - 8);
      root = dbox({root, center(vbox({
          text(input_title_) | bold | color(Color::Cyan),
          input_->Render() | size(WIDTH, LESS_THAN, dialog_width - 4) | border,
          text("Enter confirms, Escape cancels") | color(Color::GrayDark),
      }) | size(WIDTH, LESS_THAN, dialog_width) | border | color(Color::Cyan))});
    }
    return root;
  }

  bool OnEvent(Event event) override {
    if (show_input_) return handle_input_event(event);
    if (event == Event::Tab || event == Event::TabReverse) {
      focus_ = (focus_ + (event == Event::Tab ? 1 : 3)) % 4;
      return true;
    }
    if (event == Event::Escape || event == Event::F1 || event == Event::Character('?')) {
      show_help_ = !show_help_;
      return true;
    }
    if (show_help_) return true;
    if (event == Event::Character('q')) {
      screen_->Exit();
      return true;
    }
    if (event == Event::ArrowUp || event == Event::ArrowDown) {
      const bool forward = event == Event::ArrowDown;
      if (focus_ == 0) {
        if (forward) step_source();
        else step_back_instruction();
      } else {
        focus_ = (focus_ + (forward ? 1 : 3)) % 4;
      }
      return true;
    }
    if (event == Event::Character('s')) {
      step_source();
      return true;
    }
    if (event == Event::Character('u')) {
      step_back_instruction();
      return true;
    }
    if (event == Event::Character('r')) {
      running_ = !running_;
      skip_current_breakpoint_ = running_ && debugger_.at_breakpoint();
      message_ = running_ ? "running, press r to pause" : "paused";
      if (running_) schedule_run_batch();
      return true;
    }
    if (event == Event::Character('b')) {
      stop_running();
      if (debugger_.at_breakpoint()) {
        debugger_.remove_breakpoint(debugger_.cpu().pc());
        message_ = "breakpoint removed";
      } else {
        debugger_.add_breakpoint(debugger_.cpu().pc());
        message_ = "breakpoint set at current PC";
      }
      return true;
    }
    if (event == Event::Character('v')) {
      show_memory_ = !show_memory_;
      return true;
    }
    if (event == Event::Character('g')) {
      open_input("Memory address (hex)", [this](const std::string& value) {
        memory_address_ = parse_address(value);
      });
      return true;
    }
    if (event == Event::Character('o')) {
      open_input("Open ASM source", [this](const std::string& path) {
        assemble_file(path);
      });
      return true;
    }
    if (event == Event::Character('x')) {
      stop_running();
      if (!loaded_image_) {
        debugger_.reset();
        message_ = "CPU reset";
      } else {
        debugger_.load_program(text_base_, text_words_, data_base_, data_words_);
        active_instruction_address_ = text_base_;
        current_source_line_ = -1;
        source_selection_ = first_executable_line();
        message_ = "program reloaded at entry";
      }
      return true;
    }
    if (event == Event::Character('n')) {
      focus_ = (focus_ + 1) % 4;
      return true;
    }
    return false;
  }

  void set_screen(ScreenInteractive* screen) { screen_ = screen; }

 private:
  static std::string bit(bool value) { return value ? "1" : "0"; }

  Word parse_address(const std::string& value) const {
    std::size_t parsed = 0;
    const auto address = std::stoull(value, &parsed, 0);
    if (parsed != value.size()) throw std::invalid_argument("Invalid address");
    return static_cast<Word>(address);
  }

  void open_input(std::string title, std::function<void(const std::string&)> submit) {
    input_title_ = std::move(title);
    input_value_.clear();
    input_submit_ = std::move(submit);
    input_component_ = Input(&input_value_, "");
    input_ = input_component_;
    show_input_ = true;
    message_error_ = false;
  }

  bool handle_input_event(Event event) {
    if (event == Event::Escape) {
      show_input_ = false;
      return true;
    }
    if (event == Event::Return) {
      try {
        input_submit_(input_value_);
        message_error_ = false;
      } catch (const std::exception& error) {
        message_ = error.what();
        message_error_ = true;
      }
      show_input_ = false;
      return true;
    }
    return input_component_->OnEvent(event);
  }

  void assemble_file(const std::string& input_path) {
    std::string path = input_path;
    while (!path.empty() && (path.front() == ' ' || path.front() == '\t')) path.erase(path.begin());
    while (!path.empty() && (path.back() == ' ' || path.back() == '\t' || path.back() == '\r' || path.back() == '\n')) path.pop_back();

    if (path.empty()) {
      message_ = "No file path specified";
      message_error_ = true;
      return;
    }

    if (!std::ifstream(path).good()) {
      if (std::ifstream("tests/programs/" + path).good()) {
        path = "tests/programs/" + path;
      } else if (std::ifstream("../tests/programs/" + path).good()) {
        path = "../tests/programs/" + path;
      } else {
        message_ = "File not found: " + path;
        message_error_ = true;
        return;
      }
    }

    std::string hex_file = path;
    const std::string source = read_file(path);
    std::vector<Word> text_words;
    std::vector<Word> data_words;

    if (path.size() >= 4 && path.substr(path.size() - 4) == ".asm") {
      hex_file = path.substr(0, path.size() - 4) + ".hex";
      std::string asm_bin = find_risc201asm_bin();
      std::string win_asm = asm_bin;
      std::replace(win_asm.begin(), win_asm.end(), '/', '\\');
      std::string win_path = path;
      std::replace(win_path.begin(), win_path.end(), '/', '\\');
      std::string win_hex = hex_file;
      std::replace(win_hex.begin(), win_hex.end(), '/', '\\');
      const std::string err_file = hex_file + ".err";
      std::string win_err = err_file;
      std::replace(win_err.begin(), win_err.end(), '/', '\\');

      std::string cmd = "\"\"" + win_asm + "\" \"" + win_path + "\" \"" + win_hex + "\" --hex > \"" + win_err + "\" 2>&1\"";
      const int ret = std::system(cmd.c_str());
      if (ret != 0) {
        std::ifstream err_in(err_file);
        std::string err_msg;
        if (err_in.is_open()) {
          std::getline(err_in, err_msg);
        }
        if (err_msg.empty()) {
          err_msg = "Assembler failed on " + path;
        }
        message_ = err_msg;
        message_error_ = true;
        return;
      }
    }

    std::ifstream hex_in(hex_file);
    if (!hex_in.is_open()) {
      message_ = "could not open hex file " + hex_file;
      message_error_ = true;
      return;
    }

    std::string line;
    while (std::getline(hex_in, line)) {
      size_t first = line.find_first_not_of(" \t\r\n");
      if (first == std::string::npos || line[first] == '#') continue;
      try {
        Word word = static_cast<Word>(std::stoul(line.substr(first, 8), nullptr, 16));
        text_words.push_back(word);
      } catch (...) {}
    }

    if (text_words.empty()) {
      message_ = "no machine words assembled from " + path;
      message_error_ = true;
      return;
    }

    const std::vector<SourceLine> source_map = source_lines(source, text_words);
    debugger_.load_program(0, text_words, 0x10000, data_words);
    source_ = source_map;
    source_name_ = path;
    text_words_ = std::move(text_words);
    data_words_ = std::move(data_words);
    text_base_ = 0;
    data_base_ = 0x10000;
    loaded_image_ = true;
    active_instruction_address_ = text_base_;
    current_source_line_ = -1;
    source_selection_ = first_executable_line();
    message_ = "assembled and loaded " + path;
    message_error_ = false;
  }

  int first_executable_line() const {
    for (std::size_t i = 0; i < source_.size(); ++i)
      if (!source_[i].addresses.empty()) return static_cast<int>(i);
    return 0;
  }

  int source_line_for_address(Word address) const {
    for (std::size_t i = 0; i < source_.size(); ++i) {
      if (std::find(source_[i].addresses.begin(), source_[i].addresses.end(), address) !=
          source_[i].addresses.end())
        return static_cast<int>(i);
    }
    return -1;
  }

  void step_source() {
    if (source_.empty() || debugger_.cpu().halted()) return;
    stop_running();
    active_instruction_address_ = debugger_.cpu().pc();
    if (!debugger_.step_instruction()) {
      message_ = debugger_.cpu().faulted() ? to_string(debugger_.cpu().fault_code())
                                           : debugger_.cpu().halt_reason();
    } else {
      message_ = "stepped instruction at 0x" + hex_value(active_instruction_address_);
    }
    sync_execution_selection();
  }

  void step_back_instruction() {
    stop_running();
    if (!debugger_.step_back_instruction()) {
      message_ = "no earlier instruction in history";
      return;
    }
    sync_execution_selection();
    message_ = "stepped back one instruction";
  }

  void sync_execution_selection() {
    active_instruction_address_ = debugger_.at_instruction_boundary()
                                      ? debugger_.view().pc
                                      : debugger_.active_instruction_address();
    current_source_line_ = source_line_for_address(active_instruction_address_);
    if (current_source_line_ >= 0) source_selection_ = current_source_line_;
  }

  void stop_running() { running_ = false; }

  void schedule_run_batch() {
    screen_->Post([this] {
      if (!running_) return;
      for (int i = 0; i < 128; ++i) {
        if (debugger_.cpu().halted()) {
          running_ = false;
          break;
        }
        if (debugger_.at_breakpoint()) {
          if (skip_current_breakpoint_) {
            skip_current_breakpoint_ = false;
          } else {
            running_ = false;
            message_ = "breakpoint reached";
            break;
          }
        }
        debugger_.step_instruction();
        sync_execution_selection();
      }
      screen_->RequestAnimationFrame();
      if (running_) schedule_run_batch();
    });
  }

  Debugger& debugger_;
  bool vertical_ = false;
  unsigned stages_ = 6;
  int focus_ = 0;
  bool show_memory_ = false;
  bool show_help_ = false;
  bool show_input_ = false;
  bool running_ = false;
  bool skip_current_breakpoint_ = false;
  bool loaded_image_ = false;
  Word memory_address_ = 0;
  Word text_base_ = 0;
  Word data_base_ = 0;
  Word active_instruction_address_ = 0;
  std::vector<Word> text_words_;
  std::vector<Word> data_words_;
  std::vector<SourceLine> source_;
  int source_selection_ = 0;
  int current_source_line_ = -1;
  std::string source_name_ = "no source loaded";
  std::string message_ = "press o to open an .asm file";
  bool message_error_ = false;
  std::string input_title_;
  std::string input_value_;
  std::function<void(const std::string&)> input_submit_;
  Component input_component_;
  Component input_;
  ScreenInteractive* screen_ = nullptr;
};

}  // namespace

int run_tui(Debugger& debugger, bool vertical, unsigned stages) {
  auto screen = ScreenInteractive::Fullscreen();
  auto component = std::make_shared<SimulatorTui>(debugger, vertical, stages);
  component->set_screen(&screen);
  screen.Loop(component);
  return 0;
}

}  // namespace risc201
