#ifndef CONTROL_MEMORY_H
#define CONTROL_MEMORY_H

#include <vector>
#include <unordered_map>
#include <cstdint>
#include "microinstruction.h"

class ControlMemory {
public:
    uint16_t add(const Microinstruction& instr) {
        cm_.push_back(instr);
        return static_cast<uint16_t>(cm_.size() - 1);
    }

    const Microinstruction& at(uint16_t addr) const {
        return cm_.at(addr);
    }

    Microinstruction& at_mut(uint16_t addr) {
        return cm_.at(addr);
    }

    size_t size() const { return cm_.size(); }

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

#endif
