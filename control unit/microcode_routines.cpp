#include "control_memory.h"
#include "isa_spec/isa.h"
#include "microcode_routines.h"

// ============================================================
// Microcode routines: builds the Control Memory by hand.
// Layout:
//   addr 0             -> shared IF+OF (all instructions enter here)
//   addr 1..           -> per-instruction EX -> MA -> RW chains
// ============================================================

ControlMemory build_control_memory() {
    ControlMemory cm;

    // ---------- addr 0: shared IF+OF ----------
    // Fetch instruction at PC, read rs1/rs2 (fixed field positions),
    // then dispatch to the opcode's EX routine.
    {
        Microinstruction mi;
        mi.signals.operand_a_src   = BusSrc::RS1;
        mi.signals.operand_b_src   = BusSrc::RS2;   // overridden per-instruction if I-bit=1 (imm)
        mi.signals.pc_control      = PCControl::INCREMENT;
        mi.signals.next_addr_mode  = NextMicroAddr::DISPATCH;
        mi.signals.stage_name      = "IF+OF";
        cm.add(mi);
    }

    // ---------- ADD routine ----------
    // EX: ALU add(rs1, rs2/imm) -> RW: write rd
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

    // ---------- LD routine ----------
    // ld rd, imm[rs1]
    // EX: ALU add(rs1, imm) -> address -> MA: mem read -> RW: write rd
    {
        uint16_t ex_addr;
        {
            Microinstruction mi;
            mi.signals.operand_a_src   = BusSrc::RS1;
            mi.signals.operand_b_src   = BusSrc::IMM;
            mi.signals.alu_op          = ALUOp::ADD;   // computes effective address
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

    // ---------- BEQ routine ----------
    // beq offset  (branch if FLAGS.Z set, from a prior cmp)
    // EX: evaluate condition, fall through to "not taken" (fetch next)
    //     or jump to "taken" (update PC, then fetch)
    {
        uint16_t ex_addr;
        {
            Microinstruction mi;
            mi.signals.next_addr_mode  = NextMicroAddr::BRANCH_COND;
            // jump_target patched below, once the taken-path address is known
            mi.signals.stage_name      = "EX (beq check)";
            ex_addr = cm.add(mi);
        }
        {
            // Not-taken path lives immediately after EX (micro_pc_ + 1 in
            // BRANCH_COND's else-branch), so nothing to add here explicitly
            // -- the sequencer falls through to FETCH via this microinstruction:
            Microinstruction mi;
            mi.signals.next_addr_mode  = NextMicroAddr::FETCH;
            mi.signals.stage_name      = "beq not taken (fetch next)";
            cm.add(mi);
        }
        {
            // Taken path: PC = PC + offset*4, return to fetch
            Microinstruction mi;
            mi.signals.pc_control      = PCControl::LOAD_BRANCH;
            mi.signals.next_addr_mode  = NextMicroAddr::FETCH;
            mi.signals.stage_name      = "beq taken (PC update)";
            uint16_t taken_addr = cm.add(mi);

            // Now that taken_addr is known, patch the EX microinstruction's
            // jump_target so BRANCH_COND can actually reach it.
            cm.at_mut(ex_addr).signals.jump_target = taken_addr;
        }
        cm.set_entry_point(OP_BEQ, ex_addr);
    }

    return cm;
}
