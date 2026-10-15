// Aggregates processor execution hardware including registers, shared bus, decode unit, register file, and memory.
#ifndef RISC201_CU_DATAPATH_H
#define RISC201_CU_DATAPATH_H

#include <cstddef>

#include "common/control_signals.h"
#include "common/internal_registers.h"
#include "common/shared_bus.h"
#include "datapath/decode_unit.h"
#include "datapath/memory_unit.h"
#include "datapath/register_file_unit.h"
#include "isa_spec/isa.h"

namespace risc201::cu {

// Microarchitectural datapath containing internal state, buses, storage units, and execution logic
class Datapath {
 public:
  InternalRegisters regs;   // Active microarchitectural register bank
  SharedBus bus;            // Internal transfer bus connecting registers and multiplexers

  DecodeUnit decode;        // Combinational instruction decode logic
  RegisterFileUnit rf;      // 16-entry general purpose register file
  MemoryUnit mem;           // Main unified instruction and data memory

  StageLayout layout = StageLayout::SIX_STAGE; // Active pipeline structure configuration
  bool halted = false;                         // Processor halt latch
  const char* halt_reason = "";                // Diagnostic explanation if execution halted or faulted

  // Constructs datapath with specified data/instruction memory capacity (in words)
  explicit Datapath(std::size_t mem_words = MemoryUnit::kDefaultWords);

  // Clears internal registers, register file, memory, and execution flags
  void reset();

  // Executes one micro-cycle: runs bus transfer, register latching, instruction decoding, and subunit clock ticks
  void execute(const ControlSignals& sig, Word muimm);

  // Returns human-readable name of the current pipeline stage classification
  const char* current_stage_name() const { return stage_name(layout, last_stage); }

 private:
  StageClass last_stage = StageClass::FETCH; // Tracks most recently executed stage classification
};

}

#endif
