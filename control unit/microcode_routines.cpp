#include "control_memory.h"
#include "isa_spec/isa.h"
#include "microcode_routines.h"

ControlMemory build_control_memory() {
    ControlMemory cm;

    {
        Microinstruction mi;
        mi.signals.operand_a_src   = BusSrc::RS1;
        mi.signals.operand_b_src   = BusSrc::RS2;
        mi.signals.pc_control      = PCControl::INCREMENT;
        mi.signals.next_addr_mode  = NextMicroAddr::DISPATCH;
        mi.signals.stage_name      = "IF+OF";
        cm.add(mi);
    }

    {
        uint16_t ex_addr;
        {
            Microinstruction mi;
            mi.signals.alu_op          = ALUOp::ADD;
            mi.signals.next_addr_mode  = NextMicroAddr::INCREMENT;
            mi.signals.stage_name      = "EX (add)";
            ex_addr = cm.add(mi);
        }
        {
            Microinstruction mi;
            mi.signals.reg_write_en    = true;
            mi.signals.reg_write_src   = RegWriteSrc::ALU_OUT;
            mi.signals.next_addr_mode  = NextMicroAddr::FETCH;
            mi.signals.stage_name      = "RW (add)";
            cm.add(mi);
        }
        cm.set_entry_point(OP_ADD, ex_addr);
    }

    {
        uint16_t ex_addr;
        {
            Microinstruction mi;
            mi.signals.operand_a_src   = BusSrc::RS1;
            mi.signals.operand_b_src   = BusSrc::IMM;
            mi.signals.alu_op          = ALUOp::ADD;
            mi.signals.next_addr_mode  = NextMicroAddr::INCREMENT;
            mi.signals.stage_name      = "EX (ld addr)";
            ex_addr = cm.add(mi);
        }
        {
            Microinstruction mi;
            mi.signals.mem_op          = MemOp::READ;
            mi.signals.next_addr_mode  = NextMicroAddr::INCREMENT;
            mi.signals.stage_name      = "MA (ld)";
            cm.add(mi);
        }
        {
            Microinstruction mi;
            mi.signals.reg_write_en    = true;
            mi.signals.reg_write_src   = RegWriteSrc::MEM_DATA;
            mi.signals.next_addr_mode  = NextMicroAddr::FETCH;
            mi.signals.stage_name      = "RW (ld)";
            cm.add(mi);
        }
        cm.set_entry_point(OP_LD, ex_addr);
    }

    {
        uint16_t ex_addr;
        {
            Microinstruction mi;
            mi.signals.next_addr_mode  = NextMicroAddr::BRANCH_COND;
            mi.signals.stage_name      = "EX (beq check)";
            ex_addr = cm.add(mi);
        }
        {
            Microinstruction mi;
            mi.signals.next_addr_mode  = NextMicroAddr::FETCH;
            mi.signals.stage_name      = "beq not taken (fetch next)";
            cm.add(mi);
        }
        {
            Microinstruction mi;
            mi.signals.pc_control      = PCControl::LOAD_BRANCH;
            mi.signals.next_addr_mode  = NextMicroAddr::FETCH;
            mi.signals.stage_name      = "beq taken (PC update)";
            uint16_t taken_addr = cm.add(mi);

            cm.at_mut(ex_addr).signals.jump_target = taken_addr;
        }
        cm.set_entry_point(OP_BEQ, ex_addr);
    }

    return cm;
}
