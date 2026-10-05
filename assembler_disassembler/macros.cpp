// ============================================================
// macros.cpp — RISC201 macro table and expander implementation
//
// MACRO TABLE (kMacroTable):
//   Stack convention: full-descending, sp = r14, word = 4 bytes.
//   To add a macro: add ONE row here.
// ============================================================
#include "macros.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>

// ============================================================
// Macro table — one row per macro.
// %0 is replaced by the macro's register operand.
// ============================================================
static const MacroDef kMacroTable[] = {
    // ---- Integer push/pop (GPR operand) ---------------------
    { "PUSH",   RegKind::GPR, 2,
      { "sub sp, sp, 4",
        "st %0, 0[sp]", nullptr, nullptr } },

    { "POP",    RegKind::GPR, 2,
      { "ld %0, 0[sp]",
        "add sp, sp, 4", nullptr, nullptr } },

    // ---- Safe integer push/pop (with BNDCHK) ----------------
    { "SPUSH",  RegKind::GPR, 3,
      { "bndchk sp, -4",
        "sub sp, sp, 4",
        "st %0, 0[sp]", nullptr } },

    { "SPOP",   RegKind::GPR, 3,
      { "bndchk sp, 4",
        "ld %0, 0[sp]",
        "add sp, sp, 4", nullptr } },

    // ---- FPU push/pop (FPR operand) -------------------------
    { "FPUSH",  RegKind::FPR, 2,
      { "sub sp, sp, 4",
        "fst %0, 0[sp]", nullptr, nullptr } },

    { "FPOP",   RegKind::FPR, 2,
      { "fld %0, 0[sp]",
        "add sp, sp, 4", nullptr, nullptr } },

    // ---- Safe FPU push/pop (with BNDCHK) --------------------
    { "SFPUSH", RegKind::FPR, 3,
      { "bndchk sp, -4",
        "sub sp, sp, 4",
        "fst %0, 0[sp]", nullptr } },

    { "SFPOP",  RegKind::FPR, 3,
      { "bndchk sp, 4",
        "fld %0, 0[sp]",
        "add sp, sp, 4", nullptr } },
};

// ============================================================
// MacroExpander constructor — build the lookup index
// ============================================================
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

    // Validate register class
    auto ri = ISA::parseReg(operand);
    if (ri.index < 0)
        throw std::runtime_error("macro " + name + ": invalid register '" + operand + "'");
    if (ri.kind != def.operandClass) {
        std::string expected = (def.operandClass == RegKind::GPR) ? "GPR" : "FPR";
        throw std::runtime_error("macro " + name + ": expected " + expected
                                 + " but got '" + operand + "'");
    }

    // Expand: substitute %0 with operand in each template line
    std::vector<std::string> result;
    result.reserve(static_cast<size_t>(def.lineCount));
    for (int i = 0; i < def.lineCount; ++i) {
        std::string line = def.lines[i];
        // Replace all occurrences of %0
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
