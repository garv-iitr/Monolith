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

void Datapath::reset() {
  regs.reset();
  bus = SharedBus{};
  rf.reset();
  mem.reset();
  halted = false;
  halt_reason = "";
  last_stage = StageClass::FETCH;
}

Datapath::Snapshot Datapath::snapshot() const {
  return Snapshot{regs, bus, rf, halted, halt_reason, last_stage};
}

void Datapath::restore(const Snapshot& snapshot) {
  regs = snapshot.regs;
  bus = snapshot.bus;
  rf = snapshot.rf;
  halted = snapshot.halted;
  halt_reason = snapshot.halt_reason;
  last_stage = snapshot.last_stage;
}

void Datapath::execute(const ControlSignals& sig, Word muimm) {
  last_stage = sig.stage;
  if (sig.halt) {
    halted = true;
    return;
  }

  InternalRegisters next = regs;

  if (sig.fetch_ir) {
    next.ir = mem.fetch(regs.pc);
  }

  decode.eval(regs, next);

  const Word write_val =
      (sig.write_src == InternalReg::NONE) ? Word{0} : regs.get(sig.write_src);

  bus.eval(write_val, muimm, sig.transfer_sel);

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

  regs = next;
}

}
