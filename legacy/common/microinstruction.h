// Defines data structures and bitfield widths for Horizontal and Vertical microinstruction formats.
#ifndef RISC201_CU_MICROINSTRUCTION_H
#define RISC201_CU_MICROINSTRUCTION_H

#include <cstdint>

#include "common/control_signals.h"
#include "isa_spec/isa.h"

namespace risc201::cu {

// Horizontal Micro-Word: Direct, uncompressed control signals with zero decoding latency (77 bits conceptual width)
struct HorizontalMicroWord {
  ControlSignals sig;       // Pre-decoded control signals driving hardware lines directly
  std::int32_t  imm = 0;    // Sign-extended micro-immediate operand
  std::uint16_t target = 0; // Jump or branch target micro-PC address
  MicroOp       op = MicroOp::MMOV; // High-level micro-op mnemonic (used for tracing/debugging)
};

// Vertical Micro-Word: Highly compressed micro-word minimizing ROM area (45 bits encoded width)
// op and stage are just sim metadata, they arent part of the encoded word
struct VerticalMicroWord {
  MicroOp       op = MicroOp::MMOV;          // 3-bit micro-operation opcode
  InternalReg   src = InternalReg::NONE;     // 5-bit source internal register index
  InternalReg   dest = InternalReg::NONE;    // 5-bit destination internal register index
  std::int32_t  imm = 0;                     // 12-bit signed immediate operand
  std::uint16_t target = 0;                  // 10-bit branch/jump target micro-address
  std::uint16_t args = 0;                    // 10-bit packed subunit command arguments
  StageClass    stage = StageClass::EXECUTE; // Simulation metadata: pipeline stage attribution
};

// Decoded representation produced by codecs and fed directly into the datapath execution loop
struct DecodedMicro {
  ControlSignals sig;       // Active control lines for the cycle
  Word muimm = 0;           // Expanded 32-bit immediate driving the bus adder/transfer mux
};

// Vertical encoding bit allocation:
// Opcode (3) + Dest (5) + Src (5) + Imm (12) + Target (10) + Subunit Args (10) = 45 bits
constexpr int kVerticalOpBits     = 3;
constexpr int kVerticalRegBits    = 5;
constexpr int kVerticalImmBits    = 12;
constexpr int kVerticalTargetBits = 10;
constexpr int kVerticalArgsBits   = 10;
constexpr int kVerticalWordBits   = kVerticalOpBits + 2 * kVerticalRegBits +
                                    kVerticalImmBits + kVerticalTargetBits + kVerticalArgsBits;

// Horizontal encoding width: 33 raw control signal bits + 12-bit Imm + 10-bit Target + 10-bit Args = 65 to 77 bits
constexpr int kHorizontalWordBits = 33 + kVerticalImmBits + kVerticalTargetBits + kVerticalArgsBits;

}

#endif
