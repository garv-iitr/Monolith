#ifndef MICRO_SEQUENCER_H
#define MICRO_SEQUENCER_H

#include <cstdint>
#include "control_memory.h"
#include "isa.h"

// ============================================================
// MicroSequencer: owns the microPC and decides the next
// microaddress each cycle, based on the current
// microinstruction's NextMicroAddr field.
// ============================================================

class MicroSequencer {
public:
    explicit MicroSequencer(const ControlMemory& cm, uint16_t fetch_addr = 0)
        : cm_(cm), fetch_addr_(fetch_addr), micro_pc_(fetch_addr) {}

    uint16_t micro_pc() const { return micro_pc_; }

    const Microinstruction& current() const { return cm_.at(micro_pc_); }

    // Advance the microPC. `opcode` is needed for DISPATCH,
    // `branch_taken` is needed for BRANCH_COND.
    void advance(uint8_t opcode, bool branch_taken = false) {
        const auto& mi = cm_.at(micro_pc_);
        switch (mi.signals.next_addr_mode) {
            case NextMicroAddr::INCREMENT:
                micro_pc_ = micro_pc_ + 1;
                break;
            case NextMicroAddr::DISPATCH:
                micro_pc_ = cm_.entry_point(opcode);
                break;
            case NextMicroAddr::JUMP:
                micro_pc_ = mi.signals.jump_target;
                break;
            case NextMicroAddr::BRANCH_COND:
                micro_pc_ = branch_taken ? mi.signals.jump_target : micro_pc_ + 1;
                break;
            case NextMicroAddr::FETCH:
                micro_pc_ = fetch_addr_;
                break;
        }
    }

private:
    const ControlMemory& cm_;
    uint16_t fetch_addr_;
    uint16_t micro_pc_;
};

#endif // MICRO_SEQUENCER_H
