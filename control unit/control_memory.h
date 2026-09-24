#ifndef CONTROL_MEMORY_H
#define CONTROL_MEMORY_H

#include <vector>
#include <unordered_map>
#include <cstdint>
#include "microinstruction.h"

// ============================================================
// Control Memory: a flat array of microinstructions, plus a
// dispatch table mapping opcode -> starting microaddress.
//
// Layout convention used here:
//   address 0        -> shared IF+OF microinstruction (all opcodes)
//   address 1..N      -> per-instruction EX/MA/RW routines,
//                        one contiguous block per opcode
// ============================================================

class ControlMemory {
public:
    // Add a microinstruction, returns its assigned address.
    uint16_t add(const Microinstruction& instr) {
        cm_.push_back(instr);
        return static_cast<uint16_t>(cm_.size() - 1);
    }

    const Microinstruction& at(uint16_t addr) const {
        return cm_.at(addr);
    }

    // Mutable access for back-patching forward references (e.g. a branch
    // microinstruction whose jump_target is only known once the target
    // microinstruction has itself been added).
    Microinstruction& at_mut(uint16_t addr) {
        return cm_.at(addr);
    }

    size_t size() const { return cm_.size(); }

    // Dispatch table: opcode -> microaddress of that opcode's
    // first EX-stage microinstruction (after shared IF+OF).
    void set_entry_point(uint8_t opcode, uint16_t addr) {
        entry_points_[opcode] = addr;
    }

    uint16_t entry_point(uint8_t opcode) const {
        return entry_points_.at(opcode);
    }

private:
    std::vector<Microinstruction> cm_;
    std::unordered_map<uint8_t, uint16_t> entry_points_;
};

#endif // CONTROL_MEMORY_H
