#pragma once

#include <cstdint>
#include <memory>
#include "control_unit/control_rom.h"
#include "control_unit/control_word.h"
#include "control_unit/cu_types.h"
#include "control_unit/micro_sequencer.h"
#include "control_unit/pipeline_bundles.h"
#include "isa_spec/isa.h"

// Backward-compatibility support for non-pipelined simulator core and codecs
#include "common/micro_control_unit.h"
#include "horizontal/h_codec.h"
#include "vertical/v_codec.h"
#include "microcode/h_assembler.h"
#include "microcode/v_assembler.h"

namespace risc201::pipeline {

// =============================================================================
// PIPELINE CONTROL UNIT INTERFACE
// =============================================================================
class IPipelineControlUnit {
 public:
  virtual ~IPipelineControlUnit() = default;
  virtual void reset() = 0;
  virtual PipelineControlBundle peek_bundle(Word instruction, const Flags& flags) const = 0;
  virtual void advance(Word instruction) = 0;
  virtual bool is_cracking() const = 0;
  virtual bool freeze_pipeline() const = 0;
  virtual std::uint16_t micro_pc() const = 0;
  virtual const char* encoding_name() const = 0;
};

// =============================================================================
// HORIZONTAL PIPELINE CONTROL UNIT (48-bit ROM, T_dec = 0)
// =============================================================================
class HorizontalPipelineControlUnit final : public IPipelineControlUnit {
 public:
  HorizontalPipelineControlUnit();
  void reset() override;
  PipelineControlBundle peek_bundle(Word instruction, const Flags& flags) const override;
  void advance(Word instruction) override;
  bool is_cracking() const override { return sequencer_.is_cracking(); }
  bool freeze_pipeline() const override { return sequencer_.freeze_pipeline(); }
  std::uint16_t micro_pc() const override { return sequencer_.micro_pc(); }
  const char* encoding_name() const override { return "Horizontal (48-bit ROM)"; }

  static PipelineControlBundle crack_single_cycle(Opcode op, Word inst);
  static PipelineControlBundle crack_multi_uop(Opcode op, Word inst, std::uint16_t step);

 private:
  MicroSequencer sequencer_;
};

// =============================================================================
// VERTICAL PIPELINE CONTROL UNIT (24-bit ROM + Micro-Decoder, -50.0% Area)
// =============================================================================
class VerticalPipelineControlUnit final : public IPipelineControlUnit {
 public:
  VerticalPipelineControlUnit();
  void reset() override;
  PipelineControlBundle peek_bundle(Word instruction, const Flags& flags) const override;
  void advance(Word instruction) override;
  bool is_cracking() const override { return sequencer_.is_cracking(); }
  bool freeze_pipeline() const override { return sequencer_.freeze_pipeline(); }
  std::uint16_t micro_pc() const override { return sequencer_.micro_pc(); }
  const char* encoding_name() const override { return "Vertical (24-bit ROM + Decoder)"; }

 private:
  MicroSequencer sequencer_;

  static VerticalControlWord fetch_vertical_rom(Opcode op, std::uint16_t step);
  PipelineControlBundle decode_compact(const VerticalControlWord& vw, Word inst, std::uint16_t step) const;
};

std::unique_ptr<IPipelineControlUnit> create_control_unit(bool vertical);

// Forward declaration of PipelineProcessor
class PipelineProcessor;

}  // namespace risc201::pipeline

namespace risc201::cu {

// Canonical modern interfaces and type aliases
using IPipelineControlUnit          = risc201::pipeline::IPipelineControlUnit;
using HorizontalPipelineControlUnit = risc201::pipeline::HorizontalPipelineControlUnit;
using VerticalPipelineControlUnit   = risc201::pipeline::VerticalPipelineControlUnit;
using PipelineProcessor             = risc201::pipeline::PipelineProcessor;
using PipelinedProcessor            = risc201::pipeline::PipelineProcessor;

// Legacy non-pipelined instantiations
using HorizontalControlUnit = MicroControlUnit<HorizontalMicroWord, HorizontalCodec>;
using VerticalControlUnit   = MicroControlUnit<VerticalMicroWord, VerticalCodec>;

}  // namespace risc201::cu
