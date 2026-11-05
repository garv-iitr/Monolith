#include "macros.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>

// full descending stack, sp is r14, each word is 4 bytes
static const MacroDef kMacroTable[] = {
    { "PUSH",   RegKind::GPR, 2,
      { "sub sp, sp, 4",
        "st %0, 0[sp]", nullptr, nullptr } },

    { "POP",    RegKind::GPR, 2,
      { "ld %0, 0[sp]",
        "add sp, sp, 4", nullptr, nullptr } },

    { "SPUSH",  RegKind::GPR, 3,
      { "bndchk sp, -4",
        "sub sp, sp, 4",
        "st %0, 0[sp]", nullptr } },

    { "SPOP",   RegKind::GPR, 3,
      { "bndchk sp, 4",
        "ld %0, 0[sp]",
        "add sp, sp, 4", nullptr } },

    { "FPUSH",  RegKind::FPR, 2,
      { "sub sp, sp, 4",
        "fst %0, 0[sp]", nullptr, nullptr } },

    { "FPOP",   RegKind::FPR, 2,
      { "fld %0, 0[sp]",
        "add sp, sp, 4", nullptr, nullptr } },

    { "SFPUSH", RegKind::FPR, 3,
      { "bndchk sp, -4",
        "sub sp, sp, 4",
        "fst %0, 0[sp]", nullptr } },

    { "SFPOP",  RegKind::FPR, 3,
      { "bndchk sp, 4",
        "fld %0, 0[sp]",
        "add sp, sp, 4", nullptr } },
};

MacroExpander::MacroExpander() {
    constexpr size_t N = sizeof(kMacroTable) / sizeof(kMacroTable[0]);
    for (size_t i = 0; i < N; ++i)
        index_[kMacroTable[i].name] = static_cast<int>(i);
}

bool MacroExpander::isMacro(const std::string& name) const {
    std::string up = name;
    for (char& c : up) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return index_.count(up) > 0;
}

const MacroDef& MacroExpander::getDef(const std::string& name) const {
    std::string up = name;
    for (char& c : up) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    auto it = index_.find(up);
    if (it == index_.end())
        throw std::runtime_error("unknown macro '" + name + "'");
    return kMacroTable[it->second];
}

std::vector<std::string> MacroExpander::expand(const std::string& name,
                                               const std::string& operand) const {
    const MacroDef& def = getDef(name);

    auto ri = ISA::parseReg(operand);
    if (ri.index < 0)
        throw std::runtime_error("macro " + name + ": invalid register '" + operand + "'");
    if (ri.kind != def.operandClass) {
        std::string expected = (def.operandClass == RegKind::GPR) ? "GPR" : "FPR";
        throw std::runtime_error("macro " + name + ": expected " + expected
                                 + " but got '" + operand + "'");
    }

    std::vector<std::string> result;
    result.reserve(static_cast<size_t>(def.lineCount));
    for (int i = 0; i < def.lineCount; ++i) {
        std::string line = def.lines[i];
        size_t pos = 0;
        while ((pos = line.find("%0", pos)) != std::string::npos) {
            line.replace(pos, 2, operand);
            pos += operand.size();
        }
        result.push_back(line);
    }
    return result;
}

int MacroExpander::expansionSize(const std::string& name) const {
    return getDef(name).lineCount;
}
