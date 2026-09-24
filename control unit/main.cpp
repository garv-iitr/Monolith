#include <iostream>
#include <iomanip>
#include "isa_spec/isa.h"
#include "control_memory.h"
#include "micro_sequencer.h"
#include "microcode_routines.h"

// Minimal stand-in for the datapath: just enough state to prove
// the control unit sequences correctly. Aditya's simulator_core
// and Atharva's ALU replace this once the team integrates.
struct FakeDatapath {
    int32_t regs[NUM_REGISTERS] = {0};
    int32_t mem[64] = {0};
    Word    pc = 0;
    bool    flag_zero = false;
};

const char* alu_op_name(ALUOp op) {
    switch (op) {
        case ALUOp::ADD: return "ADD";
        case ALUOp::SUB: return "SUB";
        case ALUOp::NONE: return "-";
        default: return "?";
    }
}

void print_signals(uint16_t addr, const Microinstruction& mi) {
    std::cout << "  microPC=" << std::setw(2) << addr
              << "  [" << mi.signals.stage_name << "]"
              << "  alu=" << alu_op_name(mi.signals.alu_op)
              << "  reg_write=" << (mi.signals.reg_write_en ? "Y" : "N")
              << "  mem_op=" << (mi.signals.mem_op == MemOp::READ ? "READ" :
                                   mi.signals.mem_op == MemOp::WRITE ? "WRITE" : "-")
              << "\n";
}

// Run one instruction's worth of microcode: from dispatch (post-fetch)
// through to the routine's FETCH-return, printing every microstep.
void run_instruction(MicroSequencer& seq, uint8_t opcode, const char* name) {
    std::cout << "-- executing " << name << " --\n";
    // addr 0 is the shared IF+OF; simulate one fetch cycle first:
    print_signals(seq.micro_pc(), seq.current());
    seq.advance(opcode); // dispatches into the routine

    // Walk the routine until we loop back to fetch (addr 0)
    do {
        print_signals(seq.micro_pc(), seq.current());
        bool branch_taken = false; // for beq demo, toggle this to see both paths
        seq.advance(opcode, branch_taken);
    } while (seq.micro_pc() != 0);
}

int main() {
    ControlMemory cm = build_control_memory();
    MicroSequencer seq(cm, /*fetch_addr=*/0);

    std::cout << "Control Memory built with " << cm.size() << " microinstructions.\n\n";

    run_instruction(seq, OP_ADD, "ADD");
    std::cout << "\n";
    run_instruction(seq, OP_LD, "LD");
    std::cout << "\n";
    run_instruction(seq, OP_BEQ, "BEQ (not taken)");

    return 0;
}
