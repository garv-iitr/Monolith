// ============================================================
// isa.cpp — RISC201 ISA registry implementation
//
// THE ISA TABLE (kISATable):
//   To add a new instruction, add ONE row here.
//   Columns: mnemonic (upper-case), 6-bit opcode, FmtId.
//   The mnemonic string must be upper-case and unique.
//   The opcode must be unique and fit in 6 bits (0x00..0x3F).
// ============================================================
#include "isa.hpp"
#include <algorithm>
#include <cctype>

// ============================================================
// ISA table — one row per instruction.
// Keep sorted by opcode for easy human reference.
// ============================================================
static const InstructionDef kISATable[] = {
    // ---- No-operation ----------------------------------------
    { "NOP",     0x00, FmtId::F6 },
    // ---- Integer ALU (3-operand) -----------------------------
    { "ADD",     0x01, FmtId::F1 },
    { "SUB",     0x02, FmtId::F1 },
    { "MUL",     0x03, FmtId::F1 },
    { "DIV",     0x04, FmtId::F1 },
    { "MOD",     0x05, FmtId::F1 },
    { "AND",     0x06, FmtId::F1 },
    { "OR",      0x07, FmtId::F1 },
    { "NOT",     0x08, FmtId::F2 }, // 2-operand (no rs1)
    { "XOR",     0x09, FmtId::F1 },
    { "LSL",     0x0A, FmtId::F1 },
    { "LSR",     0x0B, FmtId::F1 },
    { "ASR",     0x0C, FmtId::F1 },
    // ---- Compare (no rd) ------------------------------------
    { "CMP",     0x0D, FmtId::F3 },
    // ---- Data move (2-operand, no rs1) ----------------------
    { "MOV",     0x10, FmtId::F2 },
    // ---- Memory (base + signed-offset) ----------------------
    { "LD",      0x11, FmtId::F4 },
    { "ST",      0x12, FmtId::F4 },
    // ---- Branch / call / return / halt ----------------------
    { "B",       0x20, FmtId::F5 },
    { "BEQ",     0x21, FmtId::F5 },
    { "BNE",     0x22, FmtId::F5 },
    { "BGT",     0x23, FmtId::F5 },
    { "BLT",     0x24, FmtId::F5 },
    { "CALL",    0x25, FmtId::F5 },
    { "RET",     0x26, FmtId::F6 },
    { "BNDCHK",  0x27, FmtId::F4 },
    { "HALT",    0x28, FmtId::F6 },
    // ---- FPU arithmetic (3-register) ------------------------
    { "FADD",    0x30, FmtId::F7 },
    { "FSUB",    0x31, FmtId::F7 },
    { "FMUL",    0x32, FmtId::F7 },
    { "FDIV",    0x33, FmtId::F7 },
    // ---- FPU compare / move / memory -----------------------
    { "FCMP",    0x34, FmtId::F8 },
    { "FLD",     0x35, FmtId::F4 },
    { "FST",     0x36, FmtId::F4 },
    { "FMOV",    0x37, FmtId::F8 },
};

// ============================================================
// ISA constructor — populate lookup maps from the table
// ============================================================
ISA::ISA() {
    constexpr size_t N = sizeof(kISATable) / sizeof(kISATable[0]);
    defs_.reserve(N);
    for (size_t i = 0; i < N; ++i) {
        defs_.push_back(kISATable[i]);
        byMnem_[kISATable[i].mnemonic] = static_cast<uint32_t>(i);
        byOp_[kISATable[i].opcode]     = static_cast<uint32_t>(i);
    }
}

const ISA& ISA::instance() {
    static ISA inst;
    return inst;
}

// ---- Lookup by mnemonic (case-insensitive) ------------------
const InstructionDef& ISA::byMnemonic(const std::string& mnemonic) const {
    std::string up = mnemonic;
    for (char& c : up)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    auto it = byMnem_.find(up);
    if (it == byMnem_.end())
        throw std::runtime_error("unknown mnemonic '" + mnemonic + "'");
    return defs_[it->second];
}

// ---- Lookup by opcode ---------------------------------------
const InstructionDef* ISA::byOpcode(uint8_t opcode) const {
    auto it = byOp_.find(opcode);
    return (it == byOp_.end()) ? nullptr : &defs_[it->second];
}

// ---- Existence check (no throw) ----------------------------
bool ISA::hasMnemonic(const std::string& mnemonic) const {
    std::string up = mnemonic;
    for (char& c : up)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return byMnem_.count(up) > 0;
}

// ============================================================
// Register parsing
// ============================================================
ISA::RegInfo ISA::parseReg(const std::string& name) {
    if (name.empty()) return {RegKind::GPR, -1};

    // Lower-case copy for comparison
    std::string lo = name;
    for (char& c : lo)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    // GPR aliases
    if (lo == "sp") return {RegKind::GPR, 14};
    if (lo == "ra") return {RegKind::GPR, 15};

    // GPR  r0..r15
    if (lo.size() >= 2 && lo[0] == 'r') {
        const std::string rest = lo.substr(1);
        bool ok = !rest.empty();
        for (char c : rest)
            if (!std::isdigit(static_cast<unsigned char>(c))) { ok = false; break; }
        if (ok) {
            int idx = std::stoi(rest);
            if (idx >= 0 && idx <= 15) return {RegKind::GPR, idx};
        }
    }

    // FPR  f0..f15
    if (lo.size() >= 2 && lo[0] == 'f') {
        const std::string rest = lo.substr(1);
        bool ok = !rest.empty();
        for (char c : rest)
            if (!std::isdigit(static_cast<unsigned char>(c))) { ok = false; break; }
        if (ok) {
            int idx = std::stoi(rest);
            if (idx >= 0 && idx <= 15) return {RegKind::FPR, idx};
        }
    }

    return {RegKind::GPR, -1}; // not a recognised register
}

std::string ISA::gprName(int index, bool rawRegs) {
    if (!rawRegs) {
        if (index == 14) return "sp";
        if (index == 15) return "ra";
    }
    return "r" + std::to_string(index);
}

std::string ISA::fprName(int index) {
    return "f" + std::to_string(index);
}
