#pragma once

#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

enum class FmtId : uint8_t {
    F1 = 1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
};

enum class RegKind { GPR, FPR };

struct InstructionDef {
    const char* mnemonic;
    uint8_t     opcode;
    FmtId       fmt;
};

class ISA {
public:
    static const ISA& instance();

    const InstructionDef& byMnemonic(const std::string& mnemonic) const;

    const InstructionDef* byOpcode(uint8_t opcode) const;

    const std::vector<InstructionDef>& all() const { return defs_; }

    bool hasMnemonic(const std::string& mnemonic) const;


    struct RegInfo {
        RegKind kind;
        int     index;
    };

    static RegInfo parseReg(const std::string& name);

    static std::string gprName(int index, bool rawRegs = false);

    static std::string fprName(int index);

private:
    ISA();

    std::vector<InstructionDef>      defs_;
    std::map<std::string, uint32_t>  byMnem_;
    std::map<uint8_t,     uint32_t>  byOp_;
};
