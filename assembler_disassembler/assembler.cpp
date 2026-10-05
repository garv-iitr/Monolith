// ============================================================
// assembler.cpp — RISC201 two-pass assembler implementation
//
// Pass 1: tokenise, expand macros/li, record labels, size instructions.
// Pass 2: resolve labels/equs, encode all instructions, build listing.
// ============================================================
#include "assembler.hpp"
#include "formats.hpp"
#include "macros.hpp"
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <stdexcept>

static std::string toHex8(uint32_t v) {
    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setfill('0') << std::setw(8) << v;
    return oss.str();
}

static bool isValidIdentifier(const std::string& s) {
    if (s.empty()) return false;
    if (std::isdigit(static_cast<unsigned char>(s[0]))) return false;
    for (char c : s) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '.')
            return false;
    }
    if (s[0] == '.' && s.size() > 1 && std::isdigit(static_cast<unsigned char>(s[1])))
        return false;
    return true;
}

// ============================================================
// Utility: trim whitespace
// ============================================================
std::string Assembler::trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// ============================================================
// stripComment — remove trailing comment starting with @, ;, //
// ============================================================
std::string Assembler::stripComment(const std::string& line) {
    bool inSingleQuote = false; // not used in RISC201 but defensive
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '@' || c == ';')
            return line.substr(0, i);
        if (c == '/' && i + 1 < line.size() && line[i+1] == '/')
            return line.substr(0, i);
        (void)inSingleQuote;
    }
    return line;
}

// ============================================================
// splitOperands — split comma-separated operands, trim each
// ============================================================
std::vector<std::string> Assembler::splitOperands(const std::string& s) {
    std::vector<std::string> result;
    std::string cur;
    for (char c : s) {
        if (c == ',') {
            result.push_back(trim(cur));
            cur.clear();
        } else {
            cur += c;
        }
    }
    result.push_back(trim(cur));
    return result;
}

// ============================================================
// addError — record a formatted error message
// ============================================================
void Assembler::addError(int line, const std::string& msg) {
    std::ostringstream oss;
    oss << filename_ << ":" << line << ": error: " << msg;
    errors_.push_back(oss.str());
}

// ============================================================
// parseLine — extract label, mnemonic, and operand tokens
// ============================================================
Assembler::ParsedLine Assembler::parseLine(const std::string& rawLine) const {
    ParsedLine pl;
    std::string line = trim(stripComment(rawLine));
    if (line.empty()) return pl;

    // Check for a label: find ':'
    size_t colonPos = line.find(':');
    if (colonPos != std::string::npos) {
        std::string candidate = trim(line.substr(0, colonPos));
        bool hasWhitespace = false;
        for (char c : candidate) {
            if (std::isspace(static_cast<unsigned char>(c))) {
                hasWhitespace = true;
                break;
            }
        }
        if (!hasWhitespace && !candidate.empty()) {
            if (!isValidIdentifier(candidate))
                throw std::runtime_error("invalid label name '" + candidate + "'");
            pl.label = candidate;
            line = trim(line.substr(colonPos + 1));
            if (line.empty()) return pl;
        }
    }

    // Now parse mnemonic (first whitespace-delimited token)
    size_t i = 0;
    while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i])))
        ++i;
    pl.mnemonic = line.substr(0, i);
    std::string rest = trim(line.substr(i));

    // Operands: split by commas
    if (!rest.empty())
        pl.tokens = splitOperands(rest);

    return pl;
}

// ============================================================
// parseNumber — parse a plain number string (dec/0x/0b)
// ============================================================
int64_t Assembler::parseNumber(const std::string& raw) {
    std::string s = trim(raw);
    if (s.empty()) throw std::runtime_error("empty number");
    // Strip leading '#'
    if (s[0] == '#') s = s.substr(1);
    // Strip suffix (u/h/H)
    if (!s.empty()) {
        char last = s.back();
        if (last == 'u' || last == 'U' || last == 'h' || last == 'H')
            s = s.substr(0, s.size() - 1);
    }
    bool neg = false;
    if (!s.empty() && s[0] == '-') { neg = true; s = s.substr(1); }
    if (s.empty()) throw std::runtime_error("malformed number: '" + raw + "'");

    uint64_t uv = 0;
    if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        for (size_t i = 2; i < s.size(); ++i) {
            unsigned char c = static_cast<unsigned char>(s[i]);
            if (!std::isxdigit(c)) throw std::runtime_error("bad hex: '" + raw + "'");
            uv = uv * 16 + (std::isdigit(c) ? (c-'0') : (std::tolower(c)-'a'+10));
        }
    } else if (s.size() >= 2 && s[0] == '0' && (s[1] == 'b' || s[1] == 'B')) {
        for (size_t i = 2; i < s.size(); ++i) {
            if (s[i] != '0' && s[i] != '1') throw std::runtime_error("bad binary: '" + raw + "'");
            uv = uv * 2 + (s[i] - '0');
        }
    } else {
        for (char c : s) {
            if (!std::isdigit(static_cast<unsigned char>(c))) throw std::runtime_error("bad number: '" + raw + "'");
            uv = uv * 10 + static_cast<uint64_t>(c - '0');
        }
    }
    int64_t v = static_cast<int64_t>(uv);
    return neg ? -v : v;
}

// ============================================================
// resolveImm — resolve an immediate token (possibly a .equ name)
// Returns the token with any suffix preserved (for parseImm).
// ============================================================
std::string Assembler::resolveImm(const std::string& tok,
                                  const SymbolTable& syms) const {
    std::string s = trim(tok);
    if (s.empty()) return s;

    // Strip optional '#'
    std::string base = s;
    if (!base.empty() && base[0] == '#') base = base.substr(1);

    // First check if base is already a .equ name (e.g., WIDTH, LENGTH)
    auto it = syms.find(base);
    if (it != syms.end() && it->second.isEqu) {
        return std::to_string(static_cast<int64_t>(static_cast<int32_t>(it->second.address)));
    }

    // Extract suffix if present, checking if prefix is a .equ name
    std::string suffix;
    if (!base.empty()) {
        char last = base.back();
        if (last == 'u' || last == 'U' || last == 'h' || last == 'H') {
            std::string prefix = base.substr(0, base.size() - 1);
            auto pit = syms.find(prefix);
            if (pit != syms.end() && pit->second.isEqu) {
                return std::to_string(static_cast<int64_t>(static_cast<int32_t>(pit->second.address))) + last;
            }
            suffix = last;
            base = prefix;
        }
    }

    // Is base a number?
    // Try parsing: if it fails, it might be a .equ name
    bool isNum = !base.empty();
    {
        bool neg = false;
        std::string nb = base;
        if (!nb.empty() && nb[0] == '-') { neg = true; nb = nb.substr(1); (void)neg; }
        if (nb.empty()) { isNum = false; }
        else if (nb.size() >= 2 && nb[0] == '0' && (nb[1] == 'x' || nb[1] == 'X')) {
            for (size_t i = 2; i < nb.size() && isNum; ++i)
                if (!std::isxdigit(static_cast<unsigned char>(nb[i]))) isNum = false;
        } else if (nb.size() >= 2 && nb[0] == '0' && (nb[1] == 'b' || nb[1] == 'B')) {
            for (size_t i = 2; i < nb.size() && isNum; ++i)
                if (nb[i] != '0' && nb[i] != '1') isNum = false;
        } else {
            for (char c : nb)
                if (!std::isdigit(static_cast<unsigned char>(c))) { isNum = false; break; }
        }
    }

    if (isNum) return s; // already a number; return as-is

    // Try as a .equ name
    it = syms.find(base);
    if (it != syms.end() && it->second.isEqu) {
        // Replace name with its numeric value, reattach suffix
        return std::to_string(static_cast<int64_t>(static_cast<int32_t>(it->second.address))) + suffix;
    }

    // Not a number and not a .equ: return as-is (might be a label for data directives)
    return s;
}

// ============================================================
// resolveLabel — look up a label, return its byte address
// ============================================================
uint32_t Assembler::resolveLabel(const std::string& name,
                                 const SymbolTable& syms,
                                 int /*srcLine*/) const {
    auto it = syms.find(name);
    if (it == syms.end())
        throw std::runtime_error("undefined label '" + name + "'");
    if (it->second.isEqu)
        return it->second.address; // .equ used as a label value is its value
    return it->second.address;
}

// ============================================================
// resolveValue32 — resolve a token to a 32-bit value
// ============================================================
uint32_t Assembler::resolveValue32(const std::string& tok,
                                   const SymbolTable& syms) const {
    std::string s = trim(tok);
    if (s.empty()) throw std::runtime_error("empty value");

    bool neg = false;
    if (s[0] == '-') { neg = true; s = trim(s.substr(1)); }
    else if (s[0] == '+') { s = trim(s.substr(1)); }
    if (!s.empty() && s[0] == '#') s = s.substr(1);

    // Try as .equ or label first
    auto it = syms.find(s);
    if (it != syms.end()) {
        uint32_t val = it->second.address;
        return neg ? static_cast<uint32_t>(-static_cast<int64_t>(val)) : val;
    }

    // Try parsing as a number
    try {
        int64_t v = parseNumber(s);
        if (neg) v = -v;
        return static_cast<uint32_t>(v);
    } catch (...) {
        throw std::runtime_error("undefined label or constant '" + tok + "'");
    }
}

// ============================================================
// assemble() — main entry point
// ============================================================
AssemblyResult Assembler::assemble(const std::string& source,
                                   const std::string& filename,
                                   const AssemblyOptions& opts) {
    filename_ = filename;
    opts_     = opts;
    errors_.clear();

    AssemblyResult result;
    result.textBase = opts.textBase;
    result.dataBase = opts.dataBase;

    MacroExpander macExp;
    const ISA& isa = ISA::instance();

    // ================================================================
    // PASS 1: tokenise, expand macros/li, record labels, build
    //         the expanded-line list, build the symbol table.
    // ================================================================
    std::vector<ExpandedLine> lines; // the expanded instruction list

    // Location counters per section
    uint32_t textLC = opts.textBase;
    uint32_t dataLC = opts.dataBase;

    // Current section: "text" or "data"
    std::string curSection = "text";

    // Symbol table (built during pass 1)
    SymbolTable syms;

    // Split source into individual lines
    std::vector<std::string> srcLines;
    {
        std::istringstream ss(source);
        std::string ln;
        while (std::getline(ss, ln))
            srcLines.push_back(ln);
    }

    // Helper to record a label
    auto defineLabel = [&](const std::string& label, int lineNum) {
        if (label.empty()) return;
        if (syms.count(label)) {
            addError(lineNum, "duplicate label '" + label + "'");
            return;
        }
        uint32_t addr = (curSection == "text") ? textLC : dataLC;
        syms[label] = {addr, curSection, false, lineNum};
    };

    // Inline lambda to add an instruction ExpandedLine
    auto addInstr = [&](const std::string& mnem,
                        const std::vector<std::string>& toks,
                        int lineNum, const std::string& srcText) {
        uint32_t& lc = (curSection == "text") ? textLC : dataLC;
        ExpandedLine el;
        el.kind    = LineKind::INSTR;
        el.address = lc;
        el.section = curSection;
        el.mnemonic = mnem;
        el.tokens   = toks;
        el.srcLine  = lineNum;
        el.srcText  = srcText;
        lines.push_back(el);
        lc += 4;
    };

    for (int lineNum = 1; lineNum <= static_cast<int>(srcLines.size()); ++lineNum) {
        const std::string& rawLine = srcLines[lineNum - 1];
        ParsedLine pl;
        try {
            pl = parseLine(rawLine);
        } catch (const std::exception& e) {
            addError(lineNum, e.what());
            continue;
        }

        // Define label (before advancing LC)
        if (!pl.label.empty())
            defineLabel(pl.label, lineNum);

        if (pl.mnemonic.empty()) continue; // label-only or blank line

        // Upper-case mnemonic for comparison
        std::string up = pl.mnemonic;
        for (char& c : up) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

        // ---- Directives ----------------------------------------
        if (up == ".TEXT") {
            curSection = "text";
            continue;
        }
        if (up == ".DATA") {
            curSection = "data";
            continue;
        }
        if (up == ".EQU") {
            // .equ NAME, value
            if (pl.tokens.size() < 2) {
                addError(lineNum, ".equ requires NAME and value");
                continue;
            }
            std::string equName = trim(pl.tokens[0]);
            std::string equVal  = trim(pl.tokens[1]);
            // Value must be a literal or an earlier .equ
            int64_t val = 0;
            bool ok = false;
            // Try as earlier .equ
            auto sit = syms.find(equVal);
            if (sit != syms.end() && sit->second.isEqu) {
                val = static_cast<int64_t>(sit->second.address);
                ok = true;
            }
            if (!ok) {
                try { val = parseNumber(equVal); ok = true; }
                catch (...) {}
            }
            if (!ok) {
                addError(lineNum, ".equ value must be a literal or earlier .equ constant");
                continue;
            }
            if (syms.count(equName)) {
                addError(lineNum, "duplicate symbol '" + equName + "'");
                continue;
            }
            syms[equName] = {static_cast<uint32_t>(val), "equ", true, lineNum};
            continue;
        }
        if (up == ".ORG") {
            if (pl.tokens.empty()) { addError(lineNum, ".org requires an address"); continue; }
            try {
                int64_t addr = parseNumber(pl.tokens[0]);
                if (addr < 0) { addError(lineNum, ".org address must be non-negative"); continue; }
                if (addr % 4 != 0) { addError(lineNum, ".org address must be word-aligned"); continue; }
                uint32_t& lc = (curSection == "text") ? textLC : dataLC;
                if (static_cast<uint32_t>(addr) < lc) {
                    addError(lineNum, ".org cannot move location counter backwards (from 0x"
                             + toHex8(lc) + " to 0x" + toHex8(static_cast<uint32_t>(addr)) + ")");
                } else {
                    lc = static_cast<uint32_t>(addr);
                }
            } catch (const std::exception& e) {
                addError(lineNum, std::string(".org: ") + e.what());
            }
            continue;
        }
        if (up == ".WORD") {
            if (pl.tokens.empty()) { addError(lineNum, ".word requires at least one value"); continue; }
            uint32_t& lc = (curSection == "text") ? textLC : dataLC;
            ExpandedLine el;
            el.kind       = LineKind::DATA_WORD;
            el.address    = lc;
            el.section    = curSection;
            el.dataValues = pl.tokens;
            el.srcLine    = lineNum;
            el.srcText    = rawLine;
            lines.push_back(el);
            lc += static_cast<uint32_t>(pl.tokens.size()) * 4;
            continue;
        }
        if (up == ".SPACE") {
            if (pl.tokens.empty()) { addError(lineNum, ".space requires a count"); continue; }
            try {
                int64_t n = parseNumber(pl.tokens[0]);
                if (n < 0) { addError(lineNum, ".space count must be non-negative"); continue; }
                uint32_t& lc = (curSection == "text") ? textLC : dataLC;
                ExpandedLine el;
                el.kind       = LineKind::DATA_SPACE;
                el.address    = lc;
                el.section    = curSection;
                el.spaceBytes = static_cast<uint32_t>(n);
                el.srcLine    = lineNum;
                el.srcText    = rawLine;
                lines.push_back(el);
                lc += static_cast<uint32_t>(n);
            } catch (const std::exception& e) {
                addError(lineNum, std::string(".space: ") + e.what());
            }
            continue;
        }

        // ---- LI pseudo-instruction -----------------------------
        if (up == "LI") {
            // li rd, value32
            if (pl.tokens.size() != 2) {
                addError(lineNum, "li requires 2 operands (rd, value32)");
                continue;
            }
            std::string rdTok  = trim(pl.tokens[0]);
            std::string valTok = trim(pl.tokens[1]);

            auto ri = ISA::parseReg(rdTok);
            if (ri.index < 0 || ri.kind != RegKind::GPR) {
                addError(lineNum, "li: first operand must be a GPR, got '" + rdTok + "'");
                continue;
            }

            uint32_t& lc = (curSection == "text") ? textLC : dataLC;

            // LI_HIGH: mov rd, (value32 >> 16)h
            {
                ExpandedLine el;
                el.kind    = LineKind::LI_HIGH;
                el.address = lc;
                el.section = curSection;
                el.liRd    = ri.index;
                el.liValue = valTok;
                el.srcLine = lineNum;
                el.srcText = rawLine;
                lines.push_back(el);
                lc += 4;
            }
            // LI_LOW: or rd, rd, (value32 & 0xFFFF)u
            {
                ExpandedLine el;
                el.kind    = LineKind::LI_LOW;
                el.address = lc;
                el.section = curSection;
                el.liRd    = ri.index;
                el.liValue = valTok;
                el.srcLine = lineNum;
                el.srcText = rawLine;
                lines.push_back(el);
                lc += 4;
            }
            continue;
        }

        // ---- Macros --------------------------------------------
        if (macExp.isMacro(up)) {
            if (pl.tokens.size() != 1) {
                addError(lineNum, "macro " + up + " requires exactly 1 register operand");
                continue;
            }
            // Validate register class in pass 1
            bool regOk = true;
            try {
                const MacroDef& md = macExp.getDef(up);
                auto ri2 = ISA::parseReg(pl.tokens[0]);
                if (ri2.index < 0) {
                    addError(lineNum, "macro " + up + ": invalid register '" + pl.tokens[0] + "'");
                    regOk = false;
                } else if (ri2.kind != md.operandClass) {
                    std::string expected = (md.operandClass == RegKind::GPR) ? "GPR" : "FPR";
                    addError(lineNum, "macro " + up + ": wrong register class for '" + pl.tokens[0] + "' (expected " + expected + ")");
                    regOk = false;
                }
            } catch (...) {
                regOk = false;
            }

            if (!regOk) {
                uint32_t& lc = (curSection == "text") ? textLC : dataLC;
                lc += static_cast<uint32_t>(macExp.expansionSize(up)) * 4;
                continue;
            }

            // Expand and add instructions
            try {
                std::vector<std::string> expanded = macExp.expand(up, pl.tokens[0]);
                for (size_t ei = 0; ei < expanded.size(); ++ei) {
                    ParsedLine ep = parseLine(expanded[ei]);
                    std::string eup = ep.mnemonic;
                    for (char& c : eup) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                    addInstr(eup, ep.tokens, lineNum,
                             (ei == 0) ? rawLine : ("  ; " + expanded[ei]));
                }
            } catch (const std::exception& e) {
                addError(lineNum, e.what());
                uint32_t& lc = (curSection == "text") ? textLC : dataLC;
                lc += static_cast<uint32_t>(macExp.expansionSize(up)) * 4;
            }
            continue;
        }

        // ---- Regular instruction --------------------------------
        // Validate mnemonic exists
        if (!isa.hasMnemonic(up)) {
            addError(lineNum, "unknown mnemonic '" + pl.mnemonic + "'");
            // Still advance LC by 4 to keep offsets consistent
            uint32_t& lc = (curSection == "text") ? textLC : dataLC;
            lc += 4;
            continue;
        }

        // Validate operand count
        {
            const InstructionDef& def = isa.byMnemonic(up);
            auto fmt = InstructionFormat::create(def.fmt);
            int expected = fmt->expectedOperandCount(up);
            if (static_cast<int>(pl.tokens.size()) != expected) {
                addError(lineNum, up + ": expected " + std::to_string(expected)
                         + " operand(s), got " + std::to_string(pl.tokens.size()));
                uint32_t& lc = (curSection == "text") ? textLC : dataLC;
                lc += 4;
                continue;
            }
        }

        addInstr(up, pl.tokens, lineNum, rawLine);
    }

    // ================================================================
    // Check for section overlap
    // ================================================================
    // Text section: [textBase, textLC)  Data section: [dataBase, dataLC)
    if (textLC > opts.textBase && dataLC > opts.dataBase) {
        // Check if ranges overlap
        uint32_t ts = opts.textBase, te = textLC;
        uint32_t ds = opts.dataBase, de = dataLC;
        if (!(te <= ds || de <= ts))
            addError(0, "text section [0x" + [](uint32_t v){ std::ostringstream o; o << std::hex << v; return o.str(); }(ts)
                       + ", 0x" + [](uint32_t v){ std::ostringstream o; o << std::hex << v; return o.str(); }(te)
                       + ") overlaps data section [0x" + [](uint32_t v){ std::ostringstream o; o << std::hex << v; return o.str(); }(ds)
                       + ", 0x" + [](uint32_t v){ std::ostringstream o; o << std::hex << v; return o.str(); }(de) + ")");
    }

    // If pass 1 produced errors, stop here (no output)
    // (We continue to collect as many pass-2 errors as possible)

    result.symbols = syms;

    // ================================================================
    // PASS 2: resolve labels, encode instructions, build output
    // ================================================================
    std::ostringstream listStream;
    uint32_t curTextAddr = opts.textBase;
    uint32_t curDataAddr = opts.dataBase;

    for (auto& el : lines) {
        // Pad with zeros if location counter was advanced by .org
        if (el.section == "text") {
            if (el.address > curTextAddr) {
                uint32_t padWords = (el.address - curTextAddr) / 4;
                for (uint32_t p = 0; p < padWords; ++p)
                    result.code.push_back(0);
                curTextAddr = el.address;
            }
        } else {
            if (el.address > curDataAddr) {
                uint32_t padWords = (el.address - curDataAddr) / 4;
                for (uint32_t p = 0; p < padWords; ++p)
                    result.data.push_back(0);
                curDataAddr = el.address;
            }
        }
        if (el.kind == LineKind::DATA_WORD) {
            // Emit one 32-bit word per value
            for (const auto& v : el.dataValues) {
                uint32_t word = 0;
                try {
                    word = resolveValue32(v, syms);
                } catch (const std::exception& e) {
                    addError(el.srcLine, std::string(".word: ") + e.what());
                }
                if (el.section == "text") {
                    result.code.push_back(word);
                    curTextAddr += 4;
                } else {
                    result.data.push_back(word);
                    curDataAddr += 4;
                }
                listStream << listingLine(el.address, word, el.srcText) << "\n";
                el.address += 4; // advance for listing only
            }
            continue;
        }

        if (el.kind == LineKind::DATA_SPACE) {
            // Emit zero words
            uint32_t words = (el.spaceBytes + 3) / 4;
            for (uint32_t i = 0; i < words; ++i) {
                if (el.section == "text") {
                    result.code.push_back(0);
                    curTextAddr += 4;
                } else {
                    result.data.push_back(0);
                    curDataAddr += 4;
                }
            }
            listStream << listingLine(el.address, 0,
                "                  ; .space " + std::to_string(el.spaceBytes) + " bytes") << "\n";
            continue;
        }

        // ---- LI_HIGH -------------------------------------------
        if (el.kind == LineKind::LI_HIGH) {
            uint32_t val32 = 0;
            try { val32 = resolveValue32(el.liValue, syms); }
            catch (const std::exception& e) { addError(el.srcLine, std::string("li: ") + e.what()); }
            uint16_t hi16 = static_cast<uint16_t>(val32 >> 16);
            // Encode as: mov rd, hi16h  (Format2, mode=3)
            std::string rdStr = ISA::gprName(el.liRd, true); // raw name for encoding
            const InstructionDef& def = isa.byMnemonic("MOV");
            auto fmt = InstructionFormat::create(def.fmt);
            uint32_t word = 0;
            try {
                // Build token: "hi16h"
                std::ostringstream tok;
                tok << "0x" << std::uppercase << std::hex
                    << std::setfill('0') << std::setw(4) << hi16 << "h";
                std::vector<std::string> toks = { rdStr, tok.str() };
                FieldMap fm = fmt->parseOperands(toks, "MOV");
                word = fmt->encode(fm, def.opcode);
            } catch (const std::exception& e) {
                addError(el.srcLine, std::string("li (high): ") + e.what());
            }
            if (el.section == "text") { result.code.push_back(word); curTextAddr += 4; }
            else { result.data.push_back(word); curDataAddr += 4; }
            listStream << listingLine(el.address, word,
                el.srcText + " ; [hi]") << "\n";
            continue;
        }

        // ---- LI_LOW --------------------------------------------
        if (el.kind == LineKind::LI_LOW) {
            uint32_t val32 = 0;
            try { val32 = resolveValue32(el.liValue, syms); }
            catch (...) { /* already reported in LI_HIGH */ }
            uint16_t lo16 = static_cast<uint16_t>(val32 & 0xFFFF);
            // Encode as: or rd, rd, lo16u  (Format1, mode=2)
            std::string rdStr = ISA::gprName(el.liRd, true);
            const InstructionDef& def = isa.byMnemonic("OR");
            auto fmt = InstructionFormat::create(def.fmt);
            uint32_t word = 0;
            try {
                std::ostringstream tok;
                tok << lo16 << "u";
                std::vector<std::string> toks = { rdStr, rdStr, tok.str() };
                FieldMap fm = fmt->parseOperands(toks, "OR");
                word = fmt->encode(fm, def.opcode);
            } catch (const std::exception& e) {
                addError(el.srcLine, std::string("li (low): ") + e.what());
            }
            if (el.section == "text") { result.code.push_back(word); curTextAddr += 4; }
            else { result.data.push_back(word); curDataAddr += 4; }
            listStream << listingLine(el.address, word,
                el.srcText + " ; [lo]") << "\n";
            continue;
        }

        // ---- Regular INSTR -------------------------------------
        // Get instruction definition
        const InstructionDef* defPtr = nullptr;
        try {
            defPtr = &isa.byMnemonic(el.mnemonic);
        } catch (...) {
            addError(el.srcLine, "unknown mnemonic '" + el.mnemonic + "'");
            if (el.section == "text") { result.code.push_back(0); curTextAddr += 4; }
            else { result.data.push_back(0); curDataAddr += 4; }
            continue;
        }
        const InstructionDef& def = *defPtr;
        auto fmt = InstructionFormat::create(def.fmt);

        // Resolve operands: branches get their label converted to offset;
        // other immediate tokens get .equ substitution.
        std::vector<std::string> resolvedTokens = el.tokens;

        bool isBranch = (def.fmt == FmtId::F5);
        if (isBranch) {
            // Single operand is a label; convert to word offset
            if (!el.tokens.empty()) {
                try {
                    uint32_t targetAddr = resolveLabel(el.tokens[0], syms, el.srcLine);
                    // Verify target is word-aligned
                    if (targetAddr % 4 != 0) {
                        addError(el.srcLine, el.mnemonic + ": branch target '"
                                 + el.tokens[0] + "' is not word-aligned");
                    }
                    int64_t wordOff = (static_cast<int64_t>(targetAddr)
                                      - static_cast<int64_t>(el.address)) / 4;
                    if (wordOff < -(1LL << 25) || wordOff > (1LL << 25) - 1) {
                        addError(el.srcLine, el.mnemonic + ": branch offset "
                                 + std::to_string(wordOff) + " is out of 26-bit range");
                        resolvedTokens = { "0" };
                    } else {
                        resolvedTokens = { std::to_string(wordOff) };
                    }
                } catch (const std::exception& e) {
                    addError(el.srcLine, el.mnemonic + ": " + e.what());
                    resolvedTokens = { "0" };
                }
            }
        } else {
            // Resolve .equ references in all tokens (including memory operands & signed/suffixed)
            for (auto& tok : resolvedTokens) {
                std::string t = trim(tok);
                if (t.empty()) continue;

                // Memory operand: offset[base] or offset(base)
                size_t openB = t.find_first_of("[(");
                if (openB != std::string::npos) {
                    std::string offPart = trim(t.substr(0, openB));
                    std::string basePart = t.substr(openB);
                    if (!offPart.empty()) {
                        bool neg = false;
                        if (offPart[0] == '-') { neg = true; offPart = trim(offPart.substr(1)); }
                        else if (offPart[0] == '+') { offPart = trim(offPart.substr(1)); }
                        if (!offPart.empty() && offPart[0] == '#') offPart = offPart.substr(1);
                        auto sit = syms.find(offPart);
                        if (sit != syms.end() && sit->second.isEqu) {
                            int64_t v = static_cast<int64_t>(static_cast<int32_t>(sit->second.address));
                            if (neg) v = -v;
                            tok = std::to_string(v) + basePart;
                        }
                    }
                    continue;
                }

                // Non-memory operand
                std::string base = t;
                if (!base.empty() && base[0] == '#') base = base.substr(1);
                bool neg = false;
                if (!base.empty() && base[0] == '-') { neg = true; base = trim(base.substr(1)); }
                else if (!base.empty() && base[0] == '+') { base = trim(base.substr(1)); }
                if (!base.empty() && base[0] == '#') base = base.substr(1);

                // Check if base itself is in syms
                auto sit = syms.find(base);
                if (sit != syms.end() && sit->second.isEqu) {
                    int64_t v = static_cast<int64_t>(static_cast<int32_t>(sit->second.address));
                    if (neg) v = -v;
                    tok = std::to_string(v);
                } else if (!base.empty()) {
                    // Check if base ends in u/h suffix attached to a .equ name (e.g., WIDTHu, HIGHPARTh)
                    char last = base.back();
                    if (last == 'u' || last == 'U' || last == 'h' || last == 'H') {
                        std::string prefix = base.substr(0, base.size() - 1);
                        auto psit = syms.find(prefix);
                        if (psit != syms.end() && psit->second.isEqu) {
                            int64_t v = static_cast<int64_t>(static_cast<int32_t>(psit->second.address));
                            if (neg) v = -v;
                            tok = std::to_string(v) + last;
                        }
                    }
                }
            }
        }

        // Encode
        uint32_t word = 0;
        try {
            FieldMap fm = fmt->parseOperands(resolvedTokens, el.mnemonic);
            word = fmt->encode(fm, def.opcode);
            std::string rsvErr = fmt->validateReservedBits(fm);
            if (!rsvErr.empty())
                addError(el.srcLine, rsvErr);
        } catch (const std::exception& e) {
            addError(el.srcLine, std::string(e.what()));
        }

        if (el.section == "text") { result.code.push_back(word); curTextAddr += 4; }
        else { result.data.push_back(word); curDataAddr += 4; }
        listStream << listingLine(el.address, word, el.srcText) << "\n";
    }

    result.errors  = errors_;
    result.listing = listStream.str();
    return result;
}

// ============================================================
// listingLine — format one listing line
//   "00000000  05120005    add r1, r2, 5"
// ============================================================
std::string Assembler::listingLine(uint32_t addr, uint32_t word,
                                   const std::string& src) const {
    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setfill('0')
        << std::setw(8) << addr << "  "
        << std::setw(8) << word << "    "
        << trim(src);
    return oss.str();
}
