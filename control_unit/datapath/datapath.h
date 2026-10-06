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

class Datapath {
 public:
  InternalRegisters regs;
  SharedBus bus;

  DecodeUnit decode;
  RegisterFileUnit rf;
  MemoryUnit mem;

  StageLayout layout = StageLayout::SIX_STAGE;
  bool halted = false;
  const char* halt_reason = "";

  explicit Datapath(std::size_t mem_words = MemoryUnit::kDefaultWords);

  void reset();
  void execute(const ControlSignals& sig, Word muimm);

  const char* current_stage_name() const { return stage_name(layout, last_stage); }

 private:
  StageClass last_stage = StageClass::FETCH;
};

}

#endif
