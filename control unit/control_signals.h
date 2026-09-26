#ifndef CONTROL_SIGNALS_H
#define CONTROL_SIGNALS_H

#include <cstdint>

enum class ALUOp : uint8_t {
    ADD, SUB, MUL, DIV, MOD,
    AND, OR, NOT,
    LSL, LSR, ASR,
    CMP,
    PASS_A,
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
    INCREMENT,
    LOAD_BRANCH,
    LOAD_ABS,
    HOLD
};

enum class NextMicroAddr : uint8_t {
    INCREMENT,
    DISPATCH,
    JUMP,
    BRANCH_COND,
    FETCH
};

struct ControlSignals {
    ALUOp        alu_op        = ALUOp::NONE;
    BusSrc       operand_a_src = BusSrc::NONE;
    BusSrc       operand_b_src = BusSrc::NONE;
    RegWriteSrc  reg_write_src = RegWriteSrc::NONE;
    bool         reg_write_en  = false;
    MemOp        mem_op        = MemOp::NONE;
    PCControl    pc_control    = PCControl::INCREMENT;
    bool         set_flags     = false;

    NextMicroAddr next_addr_mode = NextMicroAddr::INCREMENT;
    uint16_t      jump_target     = 0;

    const char*  stage_name = "";
};

#endif
