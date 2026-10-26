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

  InternalRegisters regs;
  SharedBus bus;

  DecodeUnit decode;
  RegisterFileUnit rf;
  MemoryUnit mem;
  risc201::safety::StackGuard stack_guard;

  StageLayout layout = StageLayout::SIX_STAGE;
  bool halted = false;
  const char* halt_reason = "";

  explicit Datapath(std::size_t mem_words = MemoryUnit::kDefaultWords,
                    bool runtime_safety = false);

  void reset();
  Snapshot snapshot() const;
  void restore(const Snapshot& snapshot);
  void execute(const ControlSignals& sig, Word muimm);

  const char* current_stage_name() const { return stage_name(layout, last_stage); }

 private:
  StageClass last_stage = StageClass::FETCH;
  bool runtime_safety_ = false;
};

}

#endif
