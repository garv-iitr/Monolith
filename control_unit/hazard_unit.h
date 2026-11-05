#pragma once

#include <cstdint>
#include "control_unit/pipeline_bundles.h"

namespace risc201::pipeline {

// =============================================================================
// PIPELINE HAZARD RESOLUTION & DATA FORWARDING UNIT
// =============================================================================
// Implements:
// 1. RAW Data Forwarding Unit: Bypasses calculated results directly from EX/MA,
//    MEMCHK, and MA/RW pipeline registers into the EX stage input multiplexers.
// 2. Load-Use Hazard Detection Unit: Interlocks pipeline by asserting PC/IF-OF
//    freeze and injecting an execution bubble (NOP) when an instruction consumes
//    a memory load result before it has been read from the cache/RAM.
// =============================================================================

struct ForwardingResult {
  Word operand_a;
  Word operand_b;
  bool forwarded_a = false;
  bool forwarded_b = false;
};

class ForwardingUnit {
 public:
  static ForwardingResult resolve(std::uint8_t rs1, std::uint8_t rs2,
                                  bool rs1_is_fpu, bool rs2_is_fpu,
                                  Word val_a, Word val_b,
                                  const ExStageLatch& ex_stage,
                                  const ChkMaLatch* chk_ma,
                                  const MaRwLatch* ma_rw,
                                  bool is_six_stage) {
    ForwardingResult res{val_a, val_b, false, false};

    // 1. Forward from stage immediately following EX (MA/RW in 4-Stage, CHK in 6-Stage)
    if (ex_stage.valid && ex_stage.control.wb.reg_write_en &&
        (ex_stage.rd != 0 || ex_stage.is_fpu_reg) &&
        ex_stage.control.wb.reg_write_src != RegWriteSrc::MEM_DATA) {
      if ((rs1 != 0 || rs1_is_fpu) && ex_stage.rd == rs1 && ex_stage.is_fpu_reg == rs1_is_fpu) {
        res.operand_a = ex_stage.alu_result;
        res.forwarded_a = true;
      }
      if ((rs2 != 0 || rs2_is_fpu) && ex_stage.rd == rs2 && ex_stage.is_fpu_reg == rs2_is_fpu) {
        res.operand_b = ex_stage.alu_result;
        res.forwarded_b = true;
      }
    }

    // 2. In 6-Stage: Forward from CHK/MA latch (stage 4 to stage 3)
    if (is_six_stage && chk_ma && chk_ma->valid &&
        chk_ma->control.wb.reg_write_en && (chk_ma->rd != 0 || chk_ma->is_fpu_reg) &&
        chk_ma->control.wb.reg_write_src != RegWriteSrc::MEM_DATA) {
      if (!res.forwarded_a && (rs1 != 0 || rs1_is_fpu) &&
          chk_ma->rd == rs1 && chk_ma->is_fpu_reg == rs1_is_fpu) {
        res.operand_a = chk_ma->alu_result;
        res.forwarded_a = true;
      }
      if (!res.forwarded_b && (rs2 != 0 || rs2_is_fpu) &&
          chk_ma->rd == rs2 && chk_ma->is_fpu_reg == rs2_is_fpu) {
        res.operand_b = chk_ma->alu_result;
        res.forwarded_b = true;
      }
    }

    // 3. In 6-Stage: Forward from MA/RW latch (stage 5 to stage 3)
    if (is_six_stage && ma_rw && ma_rw->valid &&
        ma_rw->control.wb.reg_write_en && (ma_rw->rd != 0 || ma_rw->is_fpu_reg)) {
      if (!res.forwarded_a && (rs1 != 0 || rs1_is_fpu) &&
          ma_rw->rd == rs1 && ma_rw->is_fpu_reg == rs1_is_fpu) {
        res.operand_a = ma_rw->wb_data;
        res.forwarded_a = true;
      }
      if (!res.forwarded_b && (rs2 != 0 || rs2_is_fpu) &&
          ma_rw->rd == rs2 && ma_rw->is_fpu_reg == rs2_is_fpu) {
        res.operand_b = ma_rw->wb_data;
        res.forwarded_b = true;
      }
    }

    return res;
  }
};

class HazardDetectionUnit {
 public:
  struct HazardDecision {
    bool stall_pc = false;
    bool stall_if_of = false;
    bool insert_bubble_ex = false;
  };

  static HazardDecision evaluate(const OfExLatch& of_ex,
                                 const ExStageLatch* ex_chk,
                                 const ChkMaLatch* chk_ma,
                                 std::uint8_t decode_rs1,
                                 std::uint8_t decode_rs2,
                                 bool rs1_is_fpu,
                                 bool rs2_is_fpu,
                                 bool is_six_stage) {
    HazardDecision d{};

    // Primary Load-Use Hazard: Instruction immediately following LD/FLD uses loaded register
    if (of_ex.valid && of_ex.control.ma.mem_read && (of_ex.rd != 0 || of_ex.dest_is_fpu)) {
      if (((decode_rs1 != 0 || rs1_is_fpu) && of_ex.rd == decode_rs1 && of_ex.dest_is_fpu == rs1_is_fpu) ||
          ((decode_rs2 != 0 || rs2_is_fpu) && of_ex.rd == decode_rs2 && of_ex.dest_is_fpu == rs2_is_fpu)) {
        d.stall_pc = true;
        d.stall_if_of = true;
        d.insert_bubble_ex = true;
        return d;
      }
    }

    // Extended 6-Stage Load-Use Hazard: Load in EX/CHK stage requires additional bubble
    // because memory access happens in Stage 5 (MA), two stages away from EX
    if (is_six_stage && ex_chk && ex_chk->valid &&
        ex_chk->control.ma.mem_read && (ex_chk->rd != 0 || ex_chk->is_fpu_reg)) {
      if (((decode_rs1 != 0 || rs1_is_fpu) && ex_chk->rd == decode_rs1 && ex_chk->is_fpu_reg == rs1_is_fpu) ||
          ((decode_rs2 != 0 || rs2_is_fpu) && ex_chk->rd == decode_rs2 && ex_chk->is_fpu_reg == rs2_is_fpu)) {
        d.stall_pc = true;
        d.stall_if_of = true;
        d.insert_bubble_ex = true;
        return d;
      }
    }

    // 6-Stage Load-Use Hazard: Load in CHK/MA stage requires bubble while memory is being read
    if (is_six_stage && chk_ma && chk_ma->valid &&
        chk_ma->control.ma.mem_read && (chk_ma->rd != 0 || chk_ma->is_fpu_reg)) {
      if (((decode_rs1 != 0 || rs1_is_fpu) && chk_ma->rd == decode_rs1 && chk_ma->is_fpu_reg == rs1_is_fpu) ||
          ((decode_rs2 != 0 || rs2_is_fpu) && chk_ma->rd == decode_rs2 && chk_ma->is_fpu_reg == rs2_is_fpu)) {
        d.stall_pc = true;
        d.stall_if_of = true;
        d.insert_bubble_ex = true;
        return d;
      }
    }

    return d;
  }
};

}  // namespace risc201::pipeline

namespace risc201::cu {
using pipeline::ForwardingResult;
using pipeline::ForwardingUnit;
using pipeline::HazardDetectionUnit;
}  // namespace risc201::cu
