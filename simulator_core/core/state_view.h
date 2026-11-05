#pragma once

#include <array>
#include <cstdint>

#include "control_unit/internal_registers.h"
#include "control_unit/pipeline_bundles.h"
#include "control_unit/pipeline_processor.h"
#include "isa_spec/isa.h"
#include "simulator_core/core/exception.h"

namespace risc201 {

struct StateView {
  Word pc = 0;
  Word ir = 0;
  std::array<Word, NUM_REGISTERS> gprs{};
  std::array<Word, NUM_FLOAT_REGISTERS> fprs{};
  ::Flags flags{};
  std::uint16_t micro_pc = 0;
  const char* stage = "";
  bool halted = false;
  const char* halt_reason = "";
  ExceptionCode fault_code = ExceptionCode::NONE;
  std::uint64_t fault_address = 0;

  // Live pipeline inter-stage latches & telemetry
  bool is_pipelined = true;
  pipeline::IfOfLatch if_of{};
  pipeline::OfExLatch of_ex{};
  pipeline::ExStageLatch ex_stage{};
  pipeline::ChkMaLatch chk_ma{};
  pipeline::MaRwLatch ma_rw{};
  pipeline::PipelineStats stats{};

  // Compatibility mirror of internal latches
  risc201::cu::InternalRegisters micro{};
};

}  // namespace risc201
