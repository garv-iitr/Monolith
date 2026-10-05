// ============================================================
// formats.cpp — InstructionFormat subclass implementations
//
// Encoding bit-field layout (32-bit word, bit 31 = MSB):
//   F1/F2/F3:  [31:26] op | [25:24] mode | [23:20] rd | [19:16] rs1 | [15:0] operand
//   F4:        [31:26] op | [25:24]=00   | [23:20] rd | [19:16] rs1 | [15:0] signed imm16
//   F5:        [31:26] op | [25:0] signed 26-bit word offset
//   F6:        [31:26] op | [25:0]=0
//   F7:        [31:26] op | [25:24]=00   | [23:20] fd | [19:16] fs1 | [15:12] fs2 | [11:0]=0
//   F8 FCMP:   [31:26] op | [25:24]=00   | [23:20]=0  | [19:16] fs1 | [15:12] fs2 | [11:0]=0
//   F8 FMOV:   [31:26] op | [25:24]=00   | [23:20] fd | [19:16]=0   | [15:12] fs2 | [11:0]=0
// ============================================================
#include "formats.hpp"
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

// ============================================================
// Factory
// ============================================================
std::unique_ptr<InstructionFormat> InstructionFormat::create(FmtId id) {
    switch (id) {
        case FmtId::F1: return std::make_unique<Format1>();
        case FmtId::F2: return std::make_unique<Format2>();
        case FmtId::F3: return std::make_unique<Format3>();
        case FmtId::F4: return std::make_unique<Format4>();
        case FmtId::F5: return std::make_unique<Format5>();
        case FmtId::F6: return std::make_unique<Format6>();
        case FmtId::F7: return std::make_unique<Format7>();
        case FmtId::F8: return std::make_unique<Format8>();
    }
    throw std::runtime_error("InstructionFormat::create: unknown FmtId");
}

// ============================================================
// Shared helpers
// ============================================================

int InstructionFormat::parseGPR(const std::string& tok) {
    auto ri = ISA::parseReg(tok);
    if (ri.index < 0)
        throw std::runtime_error("invalid register '" + tok + "'");
    if (ri.kind != RegKind::GPR)
        throw std::runtime_error("expected GPR but got FPR '" + tok + "'");
    return ri.index;
}

int InstructionFormat::parseFPR(const std::string& tok) {
    auto ri = ISA::parseReg(tok);
    if (ri.index < 0)
        throw std::runtime_error("invalid register '" + tok + "'");
    if (ri.kind != RegKind::FPR)
        throw std::runtime_error("expected FPR but got GPR '" + tok + "'");
    return ri.index;
}

// Helper: strip leading/trailing whitespace from a string
static std::string trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

ImmResult InstructionFormat::parseImm(const std::string& rawTok) {
    std::string tok = trim(rawTok);
    if (tok.empty())
        throw std::runtime_error("empty immediate value");

    // Strip optional leading '#'
    if (tok[0] == '#') tok = tok.substr(1);
    if (tok.empty())
        throw std::runtime_error("empty immediate value after '#'");

    // Extract suffix: last char is 'u'/'U' or 'h'/'H'
    // We must be careful not to eat hex digits. The suffix only applies
    // when the token is NOT a bare hex literal (0x...). A bare 0x literal
    // ends with hex digits, not with 'h'/'u'. We distinguish by checking
    // whether the second-to-last chars are all valid hex after '0x'.
    // Sarangi notation: "0x1234h" means high-form of 0x1234.
    // "0x1234" (no suffix) means signed default.
    int mode = 1; // default: signed
    {
        char last = tok.back();
        bool hasSuffix = false;
        if (last == 'u' || last == 'U') { mode = 2; hasSuffix = true; }
        if (last == 'h' || last == 'H') {
            // Check if this 'h' is a hex digit in a 0x... literal.
            // If token starts with "0x" or "-0x" then 'h' could be hex.
            // We apply the suffix only if it is NOT part of a 0x number.
            // Rule: strip suffix only when the remaining string after
            // removing 'h' is a valid number (decimal or 0b or 0x without 'h').
            std::string withoutH = tok.substr(0, tok.size() - 1);
            // If withoutH starts with "0x"/"-0x" and all chars after 0x are hex digits,
            // then this 'h' is a Sarangi high-form suffix (even for 0x numbers).
            // The spec example "0x1234h" uses 'h' as a high-form suffix.
            mode = 3; hasSuffix = true;
        }
        if (hasSuffix) tok = tok.substr(0, tok.size() - 1);
    }

    // Parse the number (decimal, 0x hex, 0b binary)
    bool negative = false;
    if (!tok.empty() && tok[0] == '-') { negative = true; tok = trim(tok.substr(1)); }
    if (tok.empty())
        throw std::runtime_error("malformed immediate: '" + rawTok + "'");

    uint64_t uval = 0;
    if (tok.size() >= 2 && tok[0] == '0' && (tok[1] == 'x' || tok[1] == 'X')) {
        // Hex
        std::string hexPart = tok.substr(2);
        if (hexPart.empty())
            throw std::runtime_error("malformed hex immediate: '" + rawTok + "'");
        for (char c : hexPart) {
            if (!std::isxdigit(static_cast<unsigned char>(c)))
                throw std::runtime_error("malformed hex immediate: '" + rawTok + "'");
            uval = uval * 16 + static_cast<uint64_t>(
                std::isdigit(static_cast<unsigned char>(c))
                    ? (c - '0')
                    : (std::tolower(static_cast<unsigned char>(c)) - 'a' + 10));
        }
    } else if (tok.size() >= 2 && tok[0] == '0' && (tok[1] == 'b' || tok[1] == 'B')) {
        // Binary
        std::string binPart = tok.substr(2);
        if (binPart.empty())
            throw std::runtime_error("malformed binary immediate: '" + rawTok + "'");
        for (char c : binPart) {
            if (c != '0' && c != '1')
                throw std::runtime_error("malformed binary immediate: '" + rawTok + "'");
            uval = uval * 2 + static_cast<uint64_t>(c - '0');
        }
    } else {
        // Decimal
        for (char c : tok) {
            if (!std::isdigit(static_cast<unsigned char>(c)))
                throw std::runtime_error("malformed immediate: '" + rawTok + "'");
            uval = uval * 10 + static_cast<uint64_t>(c - '0');
        }
    }

    int64_t val = static_cast<int64_t>(uval);
    if (negative) val = -val;
    return {mode, val};
}

// Parse "offset[base]" or "offset(base)" or "[base]" (offset=0)
std::pair<int64_t, int> InstructionFormat::parseMemOperand(const std::string& tok) {
    std::string s = trim(tok);

    // Find opening bracket: '[' or '('
    char close = ']';
    size_t openPos = s.find('[');
    if (openPos == std::string::npos) {
        openPos = s.find('(');
        close = ')';
    }
    if (openPos == std::string::npos)
        throw std::runtime_error("malformed memory operand '" + tok + "': missing '[' or '('");

    size_t closePos = s.find(close, openPos);
    if (closePos == std::string::npos)
        throw std::runtime_error("malformed memory operand '" + tok + "': missing closing bracket");

    std::string afterClose = trim(s.substr(closePos + 1));
    if (!afterClose.empty())
        throw std::runtime_error("malformed memory operand '" + tok + "': unexpected characters after closing bracket");

    // Offset part: everything before the '['
    std::string offStr = trim(s.substr(0, openPos));
    // Base part: between brackets
    std::string baseStr = trim(s.substr(openPos + 1, closePos - openPos - 1));

    int64_t offset = 0;
    if (!offStr.empty()) {
        // Parse as a plain signed decimal/hex/binary number (no suffix)
        ImmResult ir = parseImm(offStr);
        offset = ir.value;
        // mode is ignored for memory offsets (always signed)
    }

    int baseReg = parseGPR(baseStr);
    return {offset, baseReg};
}

int64_t InstructionFormat::signExtend(uint64_t val, int nbits) {
    uint64_t signBit = UINT64_C(1) << (nbits - 1);
    if (val & signBit)
        return static_cast<int64_t>(val | (~((signBit << 1) - 1)));
    return static_cast<int64_t>(val);
}

// ============================================================
// Helpers: format an immediate for disassembly output
// ============================================================
static std::string fmtImm(int64_t val, int mode) {
    switch (mode) {
        case 1: return std::to_string(val);                   // signed decimal
        case 2: return std::to_string(static_cast<uint16_t>(val)) + "u"; // unsigned decimal
        case 3: {
            std::ostringstream oss;
            oss << "0x" << std::uppercase << std::hex
                << std::setfill('0') << std::setw(4) << (val & 0xFFFF) << "h";
            return oss.str();
        }
        default: return std::to_string(val);
    }
}

// Format a signed imm16 for memory instructions (no mode bits, always signed)
static std::string fmtSignedImm16(int64_t val) {
    return std::to_string(static_cast<int32_t>(val));
}

// ============================================================
// Format1 — ALU 3-operand
// ============================================================
FieldMap Format1::parseOperands(const std::vector<std::string>& tokens,
                                const std::string& mnemonic) const {
    if (tokens.size() != 3)
        throw std::runtime_error(mnemonic + " requires 3 operands (rd, rs1, operand)");

    FieldMap f;
    f["rd"]  = parseGPR(tokens[0]);
    f["rs1"] = parseGPR(tokens[1]);

    // Third operand: register or immediate
    auto ri = ISA::parseReg(tokens[2]);
    if (ri.index >= 0) {
        // Register form
        if (ri.kind != RegKind::GPR)
            throw std::runtime_error(mnemonic + ": third operand must be GPR, got FPR '" + tokens[2] + "'");
        f["mode"] = 0;
        f["rs2"]  = ri.index;
        f["imm16"] = 0;
        // Warn about shift amount for LSL/LSR/ASR (not an error, just a note; omit for now)
    } else {
        // Immediate form
        ImmResult ir = parseImm(tokens[2]);
        // Range check
        if (ir.mode == 1 && (ir.value < -32768 || ir.value > 32767))
            throw std::runtime_error(mnemonic + ": signed imm16 out of range: " + std::to_string(ir.value));
        if (ir.mode == 2 && (ir.value < 0 || ir.value > 65535))
            throw std::runtime_error(mnemonic + ": unsigned imm16 out of range: " + std::to_string(ir.value));
        if (ir.mode == 3 && (ir.value < 0 || ir.value > 65535))
            throw std::runtime_error(mnemonic + ": high-form imm16 out of range: " + std::to_string(ir.value));
        // Note: shift amounts outside 0..31 produce a warning in the assembler, not an error.
        f["mode"]  = ir.mode;
        f["rs2"]   = 0;
        f["imm16"] = ir.value & 0xFFFF;
    }
    return f;
}

uint32_t Format1::encode(const FieldMap& f, uint8_t op) const {
    uint32_t word = (static_cast<uint32_t>(op) << 26)
                  | (static_cast<uint32_t>(f.at("mode"))  << 24)
                  | (static_cast<uint32_t>(f.at("rd"))    << 20)
                  | (static_cast<uint32_t>(f.at("rs1"))   << 16);
    if (f.at("mode") == 0)
        word |= (static_cast<uint32_t>(f.at("rs2")) << 12);
    else
        word |= (static_cast<uint32_t>(f.at("imm16")) & 0xFFFF);
    return word;
}

FieldMap Format1::decode(uint32_t word, uint8_t /*op*/) const {
    FieldMap f;
    f["mode"]  = (word >> 24) & 0x3;
    f["rd"]    = (word >> 20) & 0xF;
    f["rs1"]   = (word >> 16) & 0xF;
    if (f["mode"] == 0) {
        f["rs2"]   = (word >> 12) & 0xF;
        f["imm16"] = 0;
        // Check reserved bits [11:0]
        f["reserved"] = word & 0xFFF;
    } else if (f["mode"] == 1) {
        f["rs2"]   = 0;
        f["imm16"] = signExtend(word & 0xFFFF, 16);
    } else {
        f["rs2"]   = 0;
        f["imm16"] = static_cast<int64_t>(word & 0xFFFF);
    }
    return f;
}

std::string Format1::formatOperands(const FieldMap& f,
    const std::string& /*mnemonic*/, bool rawRegs,
    const ILabelResolver&) const {
    std::string rd  = ISA::gprName(static_cast<int>(f.at("rd")),  rawRegs);
    std::string rs1 = ISA::gprName(static_cast<int>(f.at("rs1")), rawRegs);
    if (f.at("mode") == 0) {
        return rd + ", " + rs1 + ", " + ISA::gprName(static_cast<int>(f.at("rs2")), rawRegs);
    }
    return rd + ", " + rs1 + ", " + fmtImm(f.at("imm16"), static_cast<int>(f.at("mode")));
}

std::string Format1::validateReservedBits(const FieldMap& f) const {
    if (f.count("reserved") && f.at("reserved") != 0)
        return "non-zero reserved bits [11:0] in register-mode Format1 instruction";
    return "";
}

// ============================================================
// Format2 — ALU 2-operand (NOT MOV), rs1 field = 0
// ============================================================
FieldMap Format2::parseOperands(const std::vector<std::string>& tokens,
                                const std::string& mnemonic) const {
    if (tokens.size() != 2)
        throw std::runtime_error(mnemonic + " requires 2 operands (rd, operand)");

    FieldMap f;
    f["rd"] = parseGPR(tokens[0]);

    auto ri = ISA::parseReg(tokens[1]);
    if (ri.index >= 0) {
        if (ri.kind != RegKind::GPR)
            throw std::runtime_error(mnemonic + ": source operand must be GPR, got FPR '" + tokens[1] + "'");
        f["mode"]  = 0;
        f["rs2"]   = ri.index;
        f["imm16"] = 0;
    } else {
        ImmResult ir = parseImm(tokens[1]);
        if (ir.mode == 1 && (ir.value < -32768 || ir.value > 32767))
            throw std::runtime_error(mnemonic + ": signed imm16 out of range: " + std::to_string(ir.value));
        if (ir.mode == 2 && (ir.value < 0 || ir.value > 65535))
            throw std::runtime_error(mnemonic + ": unsigned imm16 out of range: " + std::to_string(ir.value));
        if (ir.mode == 3 && (ir.value < 0 || ir.value > 65535))
            throw std::runtime_error(mnemonic + ": high-form imm16 out of range: " + std::to_string(ir.value));
        f["mode"]  = ir.mode;
        f["rs2"]   = 0;
        f["imm16"] = ir.value & 0xFFFF;
    }
    return f;
}

uint32_t Format2::encode(const FieldMap& f, uint8_t op) const {
    uint32_t word = (static_cast<uint32_t>(op) << 26)
                  | (static_cast<uint32_t>(f.at("mode")) << 24)
                  | (static_cast<uint32_t>(f.at("rd"))   << 20);
    // rs1 field [19:16] = 0
    if (f.at("mode") == 0)
        word |= (static_cast<uint32_t>(f.at("rs2")) << 12);
    else
        word |= (static_cast<uint32_t>(f.at("imm16")) & 0xFFFF);
    return word;
}

FieldMap Format2::decode(uint32_t word, uint8_t /*op*/) const {
    FieldMap f;
    f["mode"]     = (word >> 24) & 0x3;
    f["rd"]       = (word >> 20) & 0xF;
    f["rs1_zero"] = (word >> 16) & 0xF; // must be 0
    if (f["mode"] == 0) {
        f["rs2"]      = (word >> 12) & 0xF;
        f["imm16"]    = 0;
        f["reserved"] = word & 0xFFF;
    } else if (f["mode"] == 1) {
        f["rs2"]   = 0;
        f["imm16"] = signExtend(word & 0xFFFF, 16);
    } else {
        f["rs2"]   = 0;
        f["imm16"] = static_cast<int64_t>(word & 0xFFFF);
    }
    return f;
}

std::string Format2::formatOperands(const FieldMap& f,
    const std::string& /*mnemonic*/, bool rawRegs,
    const ILabelResolver&) const {
    std::string rd = ISA::gprName(static_cast<int>(f.at("rd")), rawRegs);
    if (f.at("mode") == 0)
        return rd + ", " + ISA::gprName(static_cast<int>(f.at("rs2")), rawRegs);
    return rd + ", " + fmtImm(f.at("imm16"), static_cast<int>(f.at("mode")));
}

std::string Format2::validateReservedBits(const FieldMap& f) const {
    std::string err;
    if (f.count("rs1_zero") && f.at("rs1_zero") != 0)
        err += "non-zero rs1 field (must be 0) in Format2 instruction; ";
    if (f.count("reserved") && f.at("reserved") != 0)
        err += "non-zero reserved bits [11:0] in reg-mode Format2; ";
    return err;
}

// ============================================================
// Format3 — Compare (CMP), rd field = 0
// ============================================================
FieldMap Format3::parseOperands(const std::vector<std::string>& tokens,
                                const std::string& mnemonic) const {
    if (tokens.size() != 2)
        throw std::runtime_error(mnemonic + " requires 2 operands (rs1, operand)");

    FieldMap f;
    f["rs1"] = parseGPR(tokens[0]);

    auto ri = ISA::parseReg(tokens[1]);
    if (ri.index >= 0) {
        if (ri.kind != RegKind::GPR)
            throw std::runtime_error(mnemonic + ": second operand must be GPR, got FPR '" + tokens[1] + "'");
        f["mode"]  = 0;
        f["rs2"]   = ri.index;
        f["imm16"] = 0;
    } else {
        ImmResult ir = parseImm(tokens[1]);
        if (ir.mode == 1 && (ir.value < -32768 || ir.value > 32767))
            throw std::runtime_error(mnemonic + ": signed imm16 out of range: " + std::to_string(ir.value));
        if (ir.mode == 2 && (ir.value < 0 || ir.value > 65535))
            throw std::runtime_error(mnemonic + ": unsigned imm16 out of range: " + std::to_string(ir.value));
        if (ir.mode == 3 && (ir.value < 0 || ir.value > 65535))
            throw std::runtime_error(mnemonic + ": high-form imm16 out of range: " + std::to_string(ir.value));
        f["mode"]  = ir.mode;
        f["rs2"]   = 0;
        f["imm16"] = ir.value & 0xFFFF;
    }
    return f;
}

uint32_t Format3::encode(const FieldMap& f, uint8_t op) const {
    uint32_t word = (static_cast<uint32_t>(op) << 26)
                  | (static_cast<uint32_t>(f.at("mode")) << 24)
                  // rd field [23:20] = 0
                  | (static_cast<uint32_t>(f.at("rs1"))  << 16);
    if (f.at("mode") == 0)
        word |= (static_cast<uint32_t>(f.at("rs2")) << 12);
    else
        word |= (static_cast<uint32_t>(f.at("imm16")) & 0xFFFF);
    return word;
}

FieldMap Format3::decode(uint32_t word, uint8_t /*op*/) const {
    FieldMap f;
    f["mode"]     = (word >> 24) & 0x3;
    f["rd_zero"]  = (word >> 20) & 0xF; // must be 0
    f["rs1"]      = (word >> 16) & 0xF;
    if (f["mode"] == 0) {
        f["rs2"]      = (word >> 12) & 0xF;
        f["imm16"]    = 0;
        f["reserved"] = word & 0xFFF;
    } else if (f["mode"] == 1) {
        f["rs2"]   = 0;
        f["imm16"] = signExtend(word & 0xFFFF, 16);
    } else {
        f["rs2"]   = 0;
        f["imm16"] = static_cast<int64_t>(word & 0xFFFF);
    }
    return f;
}

std::string Format3::formatOperands(const FieldMap& f,
    const std::string& /*mnemonic*/, bool rawRegs,
    const ILabelResolver&) const {
    std::string rs1 = ISA::gprName(static_cast<int>(f.at("rs1")), rawRegs);
    if (f.at("mode") == 0)
        return rs1 + ", " + ISA::gprName(static_cast<int>(f.at("rs2")), rawRegs);
    return rs1 + ", " + fmtImm(f.at("imm16"), static_cast<int>(f.at("mode")));
}

std::string Format3::validateReservedBits(const FieldMap& f) const {
    std::string err;
    if (f.count("rd_zero") && f.at("rd_zero") != 0)
        err += "non-zero rd field (must be 0) in CMP instruction; ";
    if (f.count("reserved") && f.at("reserved") != 0)
        err += "non-zero reserved bits [11:0] in reg-mode CMP; ";
    return err;
}

// ============================================================
// Format4 — Memory base+offset
// ============================================================

// Helper: is this mnemonic FPU memory (FLD/FST)?
static bool isFpuMem(const std::string& mn) {
    std::string up = mn;
    for (char& c : up) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return up == "FLD" || up == "FST";
}
// Helper: is this BNDCHK?
static bool isBndchk(const std::string& mn) {
    std::string up = mn;
    for (char& c : up) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return up == "BNDCHK";
}

FieldMap Format4::parseOperands(const std::vector<std::string>& tokens,
                                const std::string& mnemonic) const {
    if (tokens.size() != 2)
        throw std::runtime_error(mnemonic + " requires 2 operands");

    FieldMap f;
    bool fpuMem = isFpuMem(mnemonic);
    bool bnd    = isBndchk(mnemonic);

    if (bnd) {
        // bndchk rs1, imm  →  rd=0, rs1=tokens[0], imm16=tokens[1]
        f["rd"]   = 0;
        f["rs1"]  = parseGPR(tokens[0]);
        // Parse the immediate (signed, no suffix in practice)
        ImmResult ir = parseImm(tokens[1]);
        int64_t off = ir.value;
        if (off < -32768 || off > 32767)
            throw std::runtime_error("bndchk: offset out of range: " + std::to_string(off));
        f["imm16"] = off & 0xFFFF;
    } else {
        // ld/st/fld/fst rd, offset[rs1]
        int rdReg = fpuMem ? parseFPR(tokens[0]) : parseGPR(tokens[0]);
        f["rd"] = rdReg;

        // tokens[1] might be "8[r2]", "8(r2)", "[r2]", or plain "-4"
        // Check for bracket
        bool hasBracket = (tokens[1].find('[') != std::string::npos ||
                           tokens[1].find('(') != std::string::npos);
        if (!hasBracket)
            throw std::runtime_error(mnemonic + ": expected memory operand like 'offset[base]', got '" + tokens[1] + "'");

        auto [off, baseReg] = parseMemOperand(tokens[1]);
        if (off < -32768 || off > 32767)
            throw std::runtime_error(mnemonic + ": signed offset out of range: " + std::to_string(off));
        f["rs1"]   = baseReg;
        f["imm16"] = off & 0xFFFF;
    }
    return f;
}

uint32_t Format4::encode(const FieldMap& f, uint8_t op) const {
    // mode bits [25:24] = 00 always for Format4
    return (static_cast<uint32_t>(op)           << 26)
         | (static_cast<uint32_t>(f.at("rd"))   << 20)
         | (static_cast<uint32_t>(f.at("rs1"))  << 16)
         | (static_cast<uint32_t>(f.at("imm16")) & 0xFFFF);
}

FieldMap Format4::decode(uint32_t word, uint8_t op) const {
    FieldMap f;
    f["op"]        = op;
    f["mode_zero"] = (word >> 24) & 0x3; // must be 0
    f["rd"]        = (word >> 20) & 0xF;
    f["rs1"]       = (word >> 16) & 0xF;
    f["imm16"]     = signExtend(word & 0xFFFF, 16);
    return f;
}

std::string Format4::formatOperands(const FieldMap& f,
    const std::string& mnemonic, bool rawRegs,
    const ILabelResolver&) const {
    bool fpuMem = isFpuMem(mnemonic);
    bool bnd    = isBndchk(mnemonic);

    int64_t imm = f.at("imm16");
    std::string immStr = fmtSignedImm16(imm);
    std::string rs1Str = ISA::gprName(static_cast<int>(f.at("rs1")), rawRegs);

    if (bnd) {
        // bndchk rs1, imm
        return rs1Str + ", " + immStr;
    }
    // rd/fd, offset[base]
    std::string rdStr = fpuMem
        ? ISA::fprName(static_cast<int>(f.at("rd")))
        : ISA::gprName(static_cast<int>(f.at("rd")), rawRegs);
    return rdStr + ", " + immStr + "[" + rs1Str + "]";
}

std::string Format4::validateReservedBits(const FieldMap& f) const {
    std::string err;
    if (f.count("mode_zero") && f.at("mode_zero") != 0)
        err += "non-zero mode bits in Format4 instruction (must be 00); ";
    if (f.count("op") && f.at("op") == 0x27) { // BNDCHK opcode = 0x27
        if (f.count("rd") && f.at("rd") != 0)
            err += "non-zero rd field in BNDCHK instruction (must be 0); ";
    }
    return err;
}

// ============================================================
// Format5 — Branch/call (26-bit signed word offset)
// ============================================================
FieldMap Format5::parseOperands(const std::vector<std::string>& tokens,
                                const std::string& mnemonic) const {
    if (tokens.size() != 1)
        throw std::runtime_error(mnemonic + " requires exactly 1 operand (label/offset)");

    // The assembler has pre-converted the label to a decimal offset string.
    ImmResult ir = parseImm(tokens[0]);
    int64_t off = ir.value;
    if (off < -(1LL << 25) || off > (1LL << 25) - 1)
        throw std::runtime_error(mnemonic + ": branch offset out of 26-bit range: " + std::to_string(off));

    FieldMap f;
    f["offset26"] = off & 0x3FFFFFF;
    return f;
}

uint32_t Format5::encode(const FieldMap& f, uint8_t op) const {
    return (static_cast<uint32_t>(op) << 26)
         | (static_cast<uint32_t>(f.at("offset26")) & 0x3FFFFFFU);
}

FieldMap Format5::decode(uint32_t word, uint8_t /*op*/) const {
    FieldMap f;
    uint32_t raw = word & 0x3FFFFFF;
    f["offset26"] = signExtend(raw, 26);
    return f;
}

std::string Format5::formatOperands(const FieldMap& f,
    const std::string& /*mnemonic*/, bool /*rawRegs*/,
    const ILabelResolver& labelResolver) const {
    int32_t off = static_cast<int32_t>(f.at("offset26"));
    return labelResolver.resolve(off);
}

std::string Format5::validateReservedBits(const FieldMap& /*f*/) const {
    return ""; // no reserved bits in Format5
}

// ============================================================
// Format6 — No operands (NOP, RET, HALT)
// ============================================================
FieldMap Format6::parseOperands(const std::vector<std::string>& tokens,
                                const std::string& mnemonic) const {
    if (!tokens.empty())
        throw std::runtime_error(mnemonic + " takes no operands");
    return {};
}

uint32_t Format6::encode(const FieldMap& /*f*/, uint8_t op) const {
    return static_cast<uint32_t>(op) << 26;
}

FieldMap Format6::decode(uint32_t word, uint8_t /*op*/) const {
    FieldMap f;
    f["reserved"] = word & 0x3FFFFFF;
    return f;
}

std::string Format6::formatOperands(const FieldMap& /*f*/,
    const std::string& /*mnemonic*/, bool /*rawRegs*/,
    const ILabelResolver&) const {
    return "";
}

std::string Format6::validateReservedBits(const FieldMap& f) const {
    if (f.count("reserved") && f.at("reserved") != 0)
        return "non-zero bits [25:0] in Format6 instruction (must be 0)";
    return "";
}

// ============================================================
// Format7 — FPU 3-register (FADD FSUB FMUL FDIV)
// ============================================================
FieldMap Format7::parseOperands(const std::vector<std::string>& tokens,
                                const std::string& mnemonic) const {
    if (tokens.size() != 3)
        throw std::runtime_error(mnemonic + " requires 3 operands (fd, fs1, fs2)");
    FieldMap f;
    f["fd"]  = parseFPR(tokens[0]);
    f["fs1"] = parseFPR(tokens[1]);
    f["fs2"] = parseFPR(tokens[2]);
    return f;
}

uint32_t Format7::encode(const FieldMap& f, uint8_t op) const {
    // mode bits [25:24] = 00
    return (static_cast<uint32_t>(op)          << 26)
         | (static_cast<uint32_t>(f.at("fd"))  << 20)
         | (static_cast<uint32_t>(f.at("fs1")) << 16)
         | (static_cast<uint32_t>(f.at("fs2")) << 12);
}

FieldMap Format7::decode(uint32_t word, uint8_t /*op*/) const {
    FieldMap f;
    f["mode_zero"] = (word >> 24) & 0x3;
    f["fd"]        = (word >> 20) & 0xF;
    f["fs1"]       = (word >> 16) & 0xF;
    f["fs2"]       = (word >> 12) & 0xF;
    f["reserved"]  = word & 0xFFF;
    return f;
}

std::string Format7::formatOperands(const FieldMap& f,
    const std::string& /*mnemonic*/, bool /*rawRegs*/,
    const ILabelResolver&) const {
    return ISA::fprName(static_cast<int>(f.at("fd")))  + ", "
         + ISA::fprName(static_cast<int>(f.at("fs1"))) + ", "
         + ISA::fprName(static_cast<int>(f.at("fs2")));
}

std::string Format7::validateReservedBits(const FieldMap& f) const {
    std::string err;
    if (f.count("mode_zero") && f.at("mode_zero") != 0)
        err += "non-zero mode bits in Format7; ";
    if (f.count("reserved") && f.at("reserved") != 0)
        err += "non-zero reserved bits [11:0] in Format7; ";
    return err;
}

// ============================================================
// Format8 — FPU compare / move (FCMP / FMOV)
// ============================================================
int Format8::expectedOperandCount(const std::string& mnemonic) const {
    // Both FCMP and FMOV take 2 operands
    (void)mnemonic;
    return 2;
}

FieldMap Format8::parseOperands(const std::vector<std::string>& tokens,
                                const std::string& mnemonic) const {
    if (tokens.size() != 2)
        throw std::runtime_error(mnemonic + " requires 2 operands");

    std::string up = mnemonic;
    for (char& c : up) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    FieldMap f;
    if (up == "FCMP") {
        // fcmp fs1, fs2
        f["fd"]  = 0; // fd field = 0
        f["fs1"] = parseFPR(tokens[0]);
        f["fs2"] = parseFPR(tokens[1]);
        f["is_fcmp"] = 1;
    } else { // FMOV
        // fmov fd, fs2
        f["fd"]  = parseFPR(tokens[0]);
        f["fs1"] = 0; // fs1 field = 0
        f["fs2"] = parseFPR(tokens[1]);
        f["is_fcmp"] = 0;
    }
    return f;
}

uint32_t Format8::encode(const FieldMap& f, uint8_t op) const {
    // mode bits [25:24] = 00
    return (static_cast<uint32_t>(op)          << 26)
         | (static_cast<uint32_t>(f.at("fd"))  << 20)
         | (static_cast<uint32_t>(f.at("fs1")) << 16)
         | (static_cast<uint32_t>(f.at("fs2")) << 12);
}

FieldMap Format8::decode(uint32_t word, uint8_t op) const {
    FieldMap f;
    f["mode_zero"] = (word >> 24) & 0x3;
    f["fd"]        = (word >> 20) & 0xF;
    f["fs1"]       = (word >> 16) & 0xF;
    f["fs2"]       = (word >> 12) & 0xF;
    f["reserved"]  = word & 0xFFF;
    // Distinguish FCMP (op=0x34) from FMOV (op=0x37) for validation
    f["is_fcmp"]   = (op == 0x34) ? 1 : 0;
    return f;
}

std::string Format8::formatOperands(const FieldMap& f,
    const std::string& mnemonic, bool /*rawRegs*/,
    const ILabelResolver&) const {
    std::string up = mnemonic;
    for (char& c : up) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (up == "FCMP") {
        return ISA::fprName(static_cast<int>(f.at("fs1"))) + ", "
             + ISA::fprName(static_cast<int>(f.at("fs2")));
    }
    // FMOV
    return ISA::fprName(static_cast<int>(f.at("fd")))  + ", "
         + ISA::fprName(static_cast<int>(f.at("fs2")));
}

std::string Format8::validateReservedBits(const FieldMap& f) const {
    std::string err;
    if (f.count("mode_zero") && f.at("mode_zero") != 0)
        err += "non-zero mode bits in Format8; ";
    if (f.count("reserved") && f.at("reserved") != 0)
        err += "non-zero reserved bits [11:0] in Format8; ";
    if (f.count("is_fcmp") && f.at("is_fcmp") == 1 &&
        f.count("fd") && f.at("fd") != 0)
        err += "non-zero fd field in FCMP instruction (must be 0); ";
    if (f.count("is_fcmp") && f.at("is_fcmp") == 0 &&
        f.count("fs1") && f.at("fs1") != 0)
        err += "non-zero fs1 field in FMOV instruction (must be 0); ";
    return err;
}
