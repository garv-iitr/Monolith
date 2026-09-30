#ifndef RISC201_CU_DATAPATH_H
#define RISC201_CU_DATAPATH_H

// ============================================================================
// The microprogrammed datapath + execute unit (Sarangi, sec. 9.5 - 9.7).
//
// One call to execute() realises a single micro-cycle:
//   1. the instruction-fetch path may load ir (mloadIR);
//   2. the decode unit continuously drives the field registers from ir;
//   3. the register named by write_src drives the write bus;
//   4. the micro-adder and the transfer multiplexer place either the write
//      bus, muimm or the adder output on the read bus;
//   5. every register whose bit is set in read_mask latches the read bus;
//   6. the functional units consume the latched inputs and update their state.
//
// Latched inputs are visible to the units in the *same* cycle, which is what
// lets the microcode fuse a register transfer with a functional-unit op
// (e.g. "mmov B, regVal, <add>").
// ============================================================================

#include <cstddef>

#include "common/control_signals.h"
#include "common/internal_registers.h"
#include "common/shared_bus.h"
#include "datapath/alu_unit.h"
#include "datapath/decode_unit.h"
#include "datapath/fpu_unit.h"
#include "datapath/memory_check_unit.h"
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
  AluUnit alu;
  MemoryUnit mem;
  FpuUnit fpu;
  MemoryCheckUnit memchk;

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

}  // namespace risc201::cu

#endif  // RISC201_CU_DATAPATH_H
