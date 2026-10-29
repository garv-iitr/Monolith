// Implements datapath clock cycle execution, bus routing, register latching, and unit clocking.
#include "datapath/datapath.h"

#include "simulator_core/core/exception.h"

namespace risc201::cu {
namespace {

void raise_if_fault(const RuntimeException& fault) {
  if (fault.code != ExceptionCode::NONE) throw fault;
}

void validate_stack_pointer(Word sp,
                            const risc201::safety::StackGuard& guard) {
  if (sp % risc201::safety::MemoryMap::kWordBytes != 0) {
    throw RuntimeException{ExceptionCode::MISALIGNED_ACCESS, sp};
  }
  if (sp < guard.limit()) {
    throw RuntimeException{ExceptionCode::STACK_OVERFLOW, sp};
  }
  if (sp > guard.base()) {
    throw RuntimeException{ExceptionCode::STACK_UNDERFLOW, sp};
  }
}

void validate_memory_address(Word addr, Word sp,
                             const risc201::safety::StackGuard& guard) {
  if (addr >= guard.limit() && addr < guard.base()) {
    if (addr % risc201::safety::MemoryMap::kWordBytes != 0) {
      throw RuntimeException{ExceptionCode::MISALIGNED_ACCESS, addr};
    }
    if (sp % risc201::safety::MemoryMap::kWordBytes != 0) {
      throw RuntimeException{ExceptionCode::MISALIGNED_ACCESS, sp};
    }
    if (sp < guard.limit()) {
      throw RuntimeException{ExceptionCode::STACK_OVERFLOW, sp};
    }
    if (sp >= guard.base()) {
      throw RuntimeException{ExceptionCode::STACK_UNDERFLOW, addr};
    }
    if (addr < sp) throw RuntimeException{ExceptionCode::STACK_OVERFLOW, addr};
    return;
  }
  raise_if_fault(guard.check_mem(
      addr, risc201::safety::MemoryMap::kWordBytes, false, 0));
}

}  // namespace

Datapath::Datapath(std::size_t mem_words, bool runtime_safety)
    : mem(mem_words), runtime_safety_(runtime_safety) {}

// Resets internal registers, general register file, memory array, and status flags
void Datapath::reset() {
  regs.reset();
  bus = SharedBus{};
  rf.reset();
  mem.reset();
  halted = false;
  halt_reason = "";
  last_stage = StageClass::FETCH;
}

// Executes one micro-cycle across the datapath according to control signals and micro-immediate
void Datapath::execute(const ControlSignals& sig, Word muimm) {
  last_stage = sig.stage;

  // 1. Process CPU halt control strobe
  if (sig.halt) {
    halted = true;
    return;
  }

  // Work on next-state register container to model synchronous clock latching
  InternalRegisters next = regs;

  // 2. Instruction fetch strobe: Load machine instruction directly from Memory[PC] into IR
  if (sig.fetch_ir) {
    next.ir = mem.fetch(regs.pc);
  }

  // 3. Instruction decode: Extract register fields (rd, rs1, rs2) and expand immediates
  decode.eval(regs, next);

  // 4. Source register drive: Read value from selected source register onto the write bus
  const Word write_val =
      (sig.write_src == InternalReg::NONE) ? Word{0} : regs.get(sig.write_src);

  // 5. Shared bus evaluation: Compute adder sum, equality branch flag, and multiplex to read_bus
  bus.eval(write_val, muimm, sig.transfer_sel);

  // 6. Register write-enable latching: Write read_bus data into all registers selected by read_mask
  for (int i = 1; i < static_cast<int>(kNumInternalRegs); ++i) {
    const InternalReg r = static_cast<InternalReg>(i);
    if (sig.reads(r)) next.set(r, bus.read_bus);
  }

  const Opcode opcode = static_cast<Opcode>(get_opcode(next.ir));
  if (runtime_safety_ && opcode == Opcode::CALL &&
      sig.reg_op == RegOp::WRITE && next.reg_src == REG_SP) {
    raise_if_fault(stack_guard.check_push(rf.read(REG_SP), regs.pc));
  }

  if (runtime_safety_ && sig.reg_op == RegOp::WRITE &&
      next.reg_src == REG_SP) {
    validate_stack_pointer(next.reg_data, stack_guard);
  }

  if (runtime_safety_ && sig.memchk_op == MemChkOp::CHECK) {
    if (opcode == Opcode::CALL) {
      raise_if_fault(stack_guard.check_push(rf.read(REG_SP), regs.pc));
    } else if (opcode == Opcode::RET) {
      raise_if_fault(stack_guard.check_pop(rf.read(REG_SP), regs.pc));
    } else {
      validate_memory_address(next.memchk_addr, rf.read(REG_SP), stack_guard);
    }
  }

  // alu/fpu/memchk live in other files, we just emit their args on the bus
  rf.tick(next, sig, next);

  // 8. Internal ALU pass-through modes (optimizations for MOV and NOT operations)
  if (sig.alu_op == AluOp::PASS_B) {
    next.alu_result = next.B;
  } else if (sig.alu_op == AluOp::PASS_A) {
    next.alu_result = next.A;
  }
  if (runtime_safety_ && sig.mem_op != MemOp::NOP) {
    const Word sp = rf.read(REG_SP);
    if (opcode == Opcode::RET) {
      raise_if_fault(stack_guard.check_pop(sp, regs.pc));
      validate_memory_address(next.mar, sp, stack_guard);
    } else if (opcode == Opcode::CALL) {
      validate_stack_pointer(sp, stack_guard);
      validate_memory_address(next.mar, sp, stack_guard);
    } else {
      validate_memory_address(next.mar, sp, stack_guard);
    }
  }
  mem.tick(next, sig, next);

  // 10. Synchronous state commit: Latch next-cycle registers
  regs = next;
}

}
