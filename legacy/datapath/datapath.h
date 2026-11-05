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
#include "memory_safety/stack_guard.h"

namespace risc201::cu {

// Microarchitectural datapath containing internal state, buses, storage units, and execution logic
class Datapath {
 public:
  struct Snapshot {
    InternalRegisters regs;
    SharedBus bus;
    RegisterFileUnit rf;
    bool halted;
    const char* halt_reason;
    StageClass last_stage;
  };

  InternalRegisters regs;   // Active microarchitectural register bank
  SharedBus bus;            // Internal transfer bus connecting registers and multiplexers

  DecodeUnit decode;
  RegisterFileUnit rf;
  MemoryUnit mem;
  risc201::safety::StackGuard stack_guard;

  StageLayout layout = StageLayout::SIX_STAGE;
  bool halted = false;
  const char* halt_reason = "";

  explicit Datapath(std::size_t mem_words = MemoryUnit::kDefaultWords,
                    bool runtime_safety = false);

  // Clears internal registers, register file, memory, and execution flags
  void reset();
  Snapshot snapshot() const;
  void restore(const Snapshot& snapshot);

  // Executes one micro-cycle: runs bus transfer, register latching, instruction decoding, and subunit clock ticks
  void execute(const ControlSignals& sig, Word muimm);

  // Returns human-readable name of the current pipeline stage classification
  const char* current_stage_name() const { return stage_name(layout, last_stage); }

 private:
  StageClass last_stage = StageClass::FETCH;
  bool runtime_safety_ = false;
};

}

#endif
