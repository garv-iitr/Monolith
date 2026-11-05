#include "control_unit/pipeline_processor.h"

#include <algorithm>
#include <cstring>

namespace risc201::pipeline {

PipelineProcessor::PipelineProcessor(cu::StageLayout layout, bool vertical_encoding,
                                     std::size_t mem_words, bool runtime_safety)
    : layout_(layout),
      cu_(create_control_unit(vertical_encoding)),
      mem_(mem_words),
      stack_guard_(safety::MemoryMap::kStackTop, safety::MemoryMap::kStackLimit),
      runtime_safety_(runtime_safety) {
  reset();
}

void PipelineProcessor::reset() {
  pc_ = 0;
  std::fill(std::begin(regs_), std::end(regs_), Word{0});
  std::fill(std::begin(fregs_), std::end(fregs_), Word{0});
  flags_ = Flags{};
  halted_ = false;
  fault_code_ = ExceptionCode::NONE;
  fault_address_ = 0;

  if_of_.flush();
  of_ex_.flush();
  ex_stage_.flush();
  chk_ma_.flush();
  ma_rw_.flush();

  cu_->reset();
  stats_ = PipelineStats{};
  initialize_stack_pointer();
}

void PipelineProcessor::initialize_stack_pointer() {
  regs_[REG_SP] = static_cast<Word>(safety::MemoryMap::kStackTop);
}

void PipelineProcessor::load(const std::vector<Word>& words, Word base) {
  mem_.load_words(base, words);
  pc_ = base;
}

void PipelineProcessor::write_data(Word byte_addr, Word value) {
  mem_.write_word(byte_addr, value);
}

void PipelineProcessor::run(std::size_t max_cycles) {
  std::size_t cycles = 0;
  while (!halted_ && !faulted() && cycles < max_cycles) {
    step();
    ++cycles;
  }
}

void PipelineProcessor::check_memory_safety(Word addr, const ChkControl& chk) {
  if (!runtime_safety_ || !chk.memchk_enable) return;

  if (addr % 4 != 0) {
    throw RuntimeException{ExceptionCode::MISALIGNED_ACCESS, addr};
  }
  if (chk.chk_mode == ChkMode::STACK_BOUNDS) {
    if (addr < stack_guard_.limit()) {
      throw RuntimeException{ExceptionCode::STACK_OVERFLOW, addr};
    }
    if (addr > stack_guard_.base()) {
      throw RuntimeException{ExceptionCode::STACK_UNDERFLOW, addr};
    }
  }
}

bool PipelineProcessor::step() {
  if (halted_) return false;

  const bool is_six_stage = (layout_ == cu::StageLayout::SIX_STAGE);

  try {
    // -------------------------------------------------------------------------
    // 1. Back-End Execution (MA & RW stages)
    // -------------------------------------------------------------------------
    ChkMaLatch next_chk_ma{};
    MaRwLatch next_ma_rw{};

    if (is_six_stage) {
      // 6-STAGE: Dedicated RW (Stage 6), MA (Stage 5), and MEMCHK (Stage 4)
      if (ma_rw_.valid && !ma_rw_.bubble) {
        if (ma_rw_.control.wb.reg_write_en) {
          if (ma_rw_.is_fpu_reg) fregs_[ma_rw_.rd] = ma_rw_.wb_data;
          else if (ma_rw_.rd != 0) regs_[ma_rw_.rd] = ma_rw_.wb_data;
        }
        if (ma_rw_.control.halt) {
          halted_ = true;
          return false;
        }
        if (!ma_rw_.control.is_multi_uop || ma_rw_.control.is_last_uop) {
          stats_.instructions_retired++;
        }
      }

      if (chk_ma_.valid && !chk_ma_.bubble) {
        const auto& c = chk_ma_.control;
        Word wb_val = chk_ma_.alu_result;
        if (c.ma.mem_read) {
          wb_val = mem_.read_word(chk_ma_.verified_addr);
        } else if (c.ma.mem_write) {
          mem_.write_word(chk_ma_.verified_addr, chk_ma_.store_val);
        }

        next_ma_rw.pc = chk_ma_.pc;
        next_ma_rw.wb_data = (c.wb.reg_write_src == RegWriteSrc::MEM_DATA) ? wb_val
                             : (c.wb.reg_write_src == RegWriteSrc::PC_PLUS4) ? (chk_ma_.pc + 4)
                             : chk_ma_.alu_result;
        next_ma_rw.rd = chk_ma_.rd;
        next_ma_rw.is_fpu_reg = chk_ma_.is_fpu_reg;
        next_ma_rw.control = c;
        next_ma_rw.valid = true;
        next_ma_rw.bubble = false;
      }

      if (ex_stage_.valid && !ex_stage_.bubble) {
        check_memory_safety(ex_stage_.effective_addr, ex_stage_.control.chk);
        next_chk_ma.pc = ex_stage_.pc;
        next_chk_ma.verified_addr = ex_stage_.effective_addr;
        next_chk_ma.store_val = ex_stage_.store_val;
        next_chk_ma.alu_result = ex_stage_.alu_result;
        next_chk_ma.rd = ex_stage_.rd;
        next_chk_ma.is_fpu_reg = ex_stage_.is_fpu_reg;
        next_chk_ma.control = ex_stage_.control;
        next_chk_ma.valid = true;
        next_chk_ma.bubble = false;
      }

    } else {
      // 4-STAGE: Combined MA/RW Stage (Memory access + register writeback in 1 cycle)
      if (ex_stage_.valid && !ex_stage_.bubble) {
        const auto& c = ex_stage_.control;
        check_memory_safety(ex_stage_.effective_addr, c.chk);

        Word wb_val = ex_stage_.alu_result;
        if (c.ma.mem_read) {
          wb_val = mem_.read_word(ex_stage_.effective_addr);
        } else if (c.ma.mem_write) {
          mem_.write_word(ex_stage_.effective_addr, ex_stage_.store_val);
        }

        wb_val = (c.wb.reg_write_src == RegWriteSrc::MEM_DATA) ? wb_val
                 : (c.wb.reg_write_src == RegWriteSrc::PC_PLUS4) ? (ex_stage_.pc + 4)
                 : ex_stage_.alu_result;

        // Direct writeback to register file in same clock cycle
        if (c.wb.reg_write_en) {
          if (ex_stage_.is_fpu_reg) fregs_[ex_stage_.rd] = wb_val;
          else if (ex_stage_.rd != 0) regs_[ex_stage_.rd] = wb_val;
        }

        // Latch update for state observation
        ma_rw_.pc = ex_stage_.pc;
        ma_rw_.wb_data = wb_val;
        ma_rw_.rd = ex_stage_.rd;
        ma_rw_.is_fpu_reg = ex_stage_.is_fpu_reg;
        ma_rw_.control = c;
        ma_rw_.valid = true;
        ma_rw_.bubble = false;

        if (c.halt) {
          halted_ = true;
          return false;
        }
        if (!c.is_multi_uop || c.is_last_uop) {
          stats_.instructions_retired++;
        }
      } else {
        ma_rw_.flush();
      }
    }

    // -------------------------------------------------------------------------
    // 2. Stage EX: Execution, Forwarding & Branch Resolution
    // -------------------------------------------------------------------------
    ExStageLatch next_ex_stage{};
    bool branch_taken = false;
    Word branch_target_pc = 0;

    if (of_ex_.valid && !of_ex_.bubble) {
      const ForwardingResult fwd = ForwardingUnit::resolve(
          of_ex_.rs1, of_ex_.rs2, of_ex_.rs1_is_fpu, of_ex_.rs2_is_fpu,
          of_ex_.rs1_val, of_ex_.rs2_val,
          ex_stage_, is_six_stage ? &chk_ma_ : nullptr, is_six_stage ? &ma_rw_ : nullptr, is_six_stage);

      if (fwd.forwarded_a || fwd.forwarded_b) {
        stats_.forwarding_events++;
      }

      const Word op_a = (of_ex_.control.ex.operand_a_src == OperandASrc::RS1) ? fwd.operand_a
                        : (of_ex_.control.ex.operand_a_src == OperandASrc::PC) ? of_ex_.pc
                        : Word{0};
      const Word op_b = (of_ex_.control.ex.operand_b_src == OperandBSrc::RS2) ? fwd.operand_b
                        : (of_ex_.control.ex.operand_b_src == OperandBSrc::IMM) ? of_ex_.imm32
                        : Word{0};

      if (of_ex_.control.ex.is_fpu) {
        float fa = 0.0f, fb = 0.0f;
        const std::uint32_t ba = static_cast<std::uint32_t>(op_a);
        const std::uint32_t bb = static_cast<std::uint32_t>(op_b);
        std::memcpy(&fa, &ba, sizeof(fa));
        std::memcpy(&fb, &bb, sizeof(fb));

        switch (of_ex_.control.ex.fpu_op) {
          case cu::FpuOp::FADD: { float r = fa + fb; std::memcpy(&next_ex_stage.alu_result, &r, 4); break; }
          case cu::FpuOp::FSUB: { float r = fa - fb; std::memcpy(&next_ex_stage.alu_result, &r, 4); break; }
          case cu::FpuOp::FMUL: { float r = fa * fb; std::memcpy(&next_ex_stage.alu_result, &r, 4); break; }
          case cu::FpuOp::FDIV: { float r = fa / fb; std::memcpy(&next_ex_stage.alu_result, &r, 4); break; }
          case cu::FpuOp::FMOV: { next_ex_stage.alu_result = op_b; break; }
          case cu::FpuOp::FCMP: {
            flags_.Z = (fa == fb);
            flags_.N = (fa < fb);
            flags_.C = false;
            flags_.V = false;
            break;
          }
          default: next_ex_stage.alu_result = op_b; break;
        }
      } else if (of_ex_.control.ex.alu_op != cu::AluOp::NOP) {
        const AluResult res = alu_.execute(of_ex_.control.ex.alu_op, op_a, op_b);
        next_ex_stage.alu_result = res.value;
        if (of_ex_.control.ex.set_flags) {
          flags_.Z = res.zero;
          flags_.N = res.negative;
          flags_.C = res.carry;
          flags_.V = res.overflow;
        }
      } else {
        next_ex_stage.alu_result = op_b;
      }

      next_ex_stage.effective_addr = next_ex_stage.alu_result;
      if (of_ex_.control.ex.is_call) {
        next_ex_stage.store_val = of_ex_.pc + 4;
        next_ex_stage.effective_addr = fwd.operand_a; // decremented SP from uop 0
      } else {
        next_ex_stage.store_val = fwd.operand_b;
      }

      if (of_ex_.control.ex.is_branch) {
        if (of_ex_.control.ex.is_ret) {
          branch_taken = true;
          branch_target_pc = fwd.operand_b;
        } else if (of_ex_.opcode == Opcode::B ||
                   of_ex_.opcode == Opcode::CALL ||
                   eval_cond(of_ex_.control.ex.branch_cond, flags_)) {
          branch_taken = true;
          branch_target_pc = of_ex_.branch_target;
        }
      }

      next_ex_stage.pc = of_ex_.pc;
      next_ex_stage.rd = of_ex_.rd;
      next_ex_stage.is_fpu_reg = of_ex_.dest_is_fpu;
      next_ex_stage.control = of_ex_.control;
      next_ex_stage.valid = true;
      next_ex_stage.bubble = false;
    }

    if (branch_taken) {
      pc_ = branch_target_pc;
      if_of_.flush();
      cu_->reset();
      stats_.branch_flushes += 2;
    }

    // -------------------------------------------------------------------------
    // 3. Stage OF: Decode, Hazard Detection, Register Read & Microcode Cracking
    // -------------------------------------------------------------------------
    OfExLatch next_of_ex{};
    bool stall_pc = false;
    bool stall_if_of = false;

    if (if_of_.valid && !if_of_.bubble && !branch_taken) {
      const Word inst = if_of_.raw;
      const PipelineControlBundle bundle = cu_->peek_bundle(inst, flags_);

      const HazardDetectionUnit::HazardDecision haz = HazardDetectionUnit::evaluate(
          of_ex_, is_six_stage ? &ex_stage_ : nullptr,
          bundle.src_reg_a, bundle.src_reg_b,
          bundle.src_reg_a_is_fpu, bundle.src_reg_b_is_fpu,
          is_six_stage);

      if (haz.insert_bubble_ex) {
        next_of_ex.flush();
        stats_.load_use_stalls += 1;
        stall_pc = true;
        stall_if_of = true;
      } else {
        cu_->advance(inst);
        if (bundle.freeze_if_of) {
          stall_pc = true;
          stall_if_of = true;
          stats_.uop_expansion_cycles++;
        }

        next_of_ex.pc = if_of_.pc;
        next_of_ex.raw = inst;
        next_of_ex.opcode = opcode_of(inst);
        next_of_ex.rd = bundle.wb.dest_reg;
        next_of_ex.rs1 = bundle.src_reg_a;
        next_of_ex.rs2 = bundle.src_reg_b;
        next_of_ex.rs1_is_fpu = bundle.src_reg_a_is_fpu;
        next_of_ex.rs2_is_fpu = bundle.src_reg_b_is_fpu;
        next_of_ex.dest_is_fpu = bundle.wb.is_fpu;

        // Register file read (write-before-read ensures 4-stage MA/RW bypass)
        next_of_ex.rs1_val = bundle.src_reg_a_is_fpu ? fregs_[bundle.src_reg_a] : regs_[bundle.src_reg_a];
        next_of_ex.rs2_val = bundle.src_reg_b_is_fpu ? fregs_[bundle.src_reg_b] : regs_[bundle.src_reg_b];

        next_of_ex.imm32 = bundle.imm;
        next_of_ex.branch_target = if_of_.pc + 4 + get_offset26(inst) * 4;
        next_of_ex.control = bundle;
        next_of_ex.valid = true;
        next_of_ex.bubble = false;
      }
    }

    // -------------------------------------------------------------------------
    // 4. Stage IF: Instruction Fetch
    // -------------------------------------------------------------------------
    IfOfLatch next_if_of = if_of_;
    if (!stall_pc && !branch_taken && !halted_) {
      if (pc_ < mem_.words() * 4) {
        next_if_of.pc = pc_;
        next_if_of.raw = mem_.fetch(pc_);
        next_if_of.valid = true;
        next_if_of.bubble = false;
        pc_ += 4;
      } else {
        next_if_of.flush();
      }
    }

    // Synchronous clock edge: commit latches
    if (!stall_if_of && !branch_taken) {
      if_of_ = next_if_of;
    }
    of_ex_ = next_of_ex;
    ex_stage_ = next_ex_stage;
    if (is_six_stage) {
      chk_ma_ = next_chk_ma;
      ma_rw_ = next_ma_rw;
    }

    stats_.total_cycles++;
    return !halted_;

  } catch (const RuntimeException& e) {
    fault_code_ = e.code;
    fault_address_ = static_cast<Word>(e.address);
    halted_ = true;
    return false;
  }
}

}  // namespace risc201::pipeline
