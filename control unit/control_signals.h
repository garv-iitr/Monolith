#ifndef CONTROL_SIGNALS_H
#define CONTROL_SIGNALS_H

#include <cstdint>

// ============================================================
// Control signals — one struct per cycle, produced by decoding
// a horizontal microinstruction. The datapath (ALU, register
// file, memory, PC) reads this struct to know what to do.
// ============================================================

enum class ALUOp : uint8_t {
    ADD, SUB, MUL, DIV, MOD,
    AND, OR, NOT,
    LSL, LSR, ASR,
    CMP,
    PASS_A,   // pass operand A through unchanged (used by mov)
    NONE
};

enum class BusSrc : uint8_t {
    RS1, RS2, IMM, PC, ALU_OUT, MEM_DATA, NONE
};

enum class RegWriteSrc : uint8_t {
    ALU_OUT, MEM_DATA, PC_PLUS4, NONE
};

enum class MemOp : uint8_t {
    READ, WRITE, NONE
};

enum class PCControl : uint8_t {
    INCREMENT,      // PC = PC + 4 (normal sequencing)
    LOAD_BRANCH,    // PC = PC + offset*4  (branch taken)
    LOAD_ABS,       // PC = target (call)
    HOLD            // no change (e.g. mid multi-cycle op)
};

enum class NextMicroAddr : uint8_t {
    INCREMENT,   // next sequential microinstruction
    DISPATCH,    // jump via opcode -> entry-point table (end of IF+OF)
    JUMP,        // unconditional jump to a fixed microaddress
    BRANCH_COND, // conditional jump based on FLAGS (used mid-routine)
    FETCH        // return to the shared fetch entry (end of routine)
};

struct ControlSignals {
    // --- datapath ---
    ALUOp        alu_op        = ALUOp::NONE;
    BusSrc       operand_a_src = BusSrc::NONE;   // usually RS1
    BusSrc       operand_b_src = BusSrc::NONE;   // RS2 or IMM depending on I-bit
    RegWriteSrc  reg_write_src = RegWriteSrc::NONE;
    bool         reg_write_en  = false;          // write to rd this cycle?
    MemOp        mem_op        = MemOp::NONE;
    PCControl    pc_control    = PCControl::INCREMENT;
    bool         set_flags     = false;          // update FLAGS (used by cmp)

    // --- sequencing (microPC) ---
    NextMicroAddr next_addr_mode = NextMicroAddr::INCREMENT;
    uint16_t      jump_target     = 0;   // valid when next_addr_mode == JUMP

    // --- stage tag, for debugging / CLI display ---
    const char*  stage_name = "";
};

#endif // CONTROL_SIGNALS_H
