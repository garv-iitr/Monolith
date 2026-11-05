#include "disassembler.hpp"
#include "formats.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

static std::string hexAddr(uint32_t addr) {
    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setfill('0') << std::setw(8) << addr;
    return oss.str();
}

std::string Disassembler::disassemble(const std::vector<uint32_t>& words,
                                      uint32_t baseAddr,
                                      bool plain,
                                      bool rawRegs) const {
    const ISA& isa = ISA::instance();

    std::map<uint32_t, std::string> labels;

    for (size_t i = 0; i < words.size(); ++i) {
        uint32_t word = words[i];
        uint8_t  op   = static_cast<uint8_t>(word >> 26);
        const InstructionDef* def = isa.byOpcode(op);
        if (!def) continue;
        if (def->fmt != FmtId::F5) continue;

        uint32_t raw  = word & 0x3FFFFFF;
        // bit 25 is the sign, so sign-extend the 26-bit offset
        int32_t  off  = static_cast<int32_t>(
            (raw & 0x2000000) ? (raw | 0xFC000000u) : raw);
        uint32_t thisAddress = baseAddr + static_cast<uint32_t>(i) * 4;
        int64_t  target = static_cast<int64_t>(thisAddress) + 4 + static_cast<int64_t>(off) * 4;
        uint32_t targetAddr = static_cast<uint32_t>(target);

        std::string lbl = "L_" + hexAddr(targetAddr);
        labels[targetAddr] = lbl;
    }

    std::ostringstream out;

    for (size_t i = 0; i < words.size(); ++i) {
        uint32_t addr = baseAddr + static_cast<uint32_t>(i) * 4;
        uint32_t word = words[i];
        uint8_t  op   = static_cast<uint8_t>(word >> 26);

        auto lit = labels.find(addr);
        if (lit != labels.end()) {
            out << lit->second << ":\n";
        }

        const InstructionDef* def = isa.byOpcode(op);

        if (!def) {
            if (plain) {
                out << ".word 0x" << hexAddr(word)
                    << " ; invalid encoding (unknown opcode 0x"
                    << std::uppercase << std::hex << static_cast<int>(op) << ")\n";
            } else {
                out << "0x" << hexAddr(addr) << ":  " << hexAddr(word)
                    << "    .word 0x" << hexAddr(word)
                    << " ; invalid encoding\n";
            }
            continue;
        }

        auto fmt = InstructionFormat::create(def->fmt);
        FieldMap fields;
        std::string rsvErr;
        try {
            fields = fmt->decode(word, def->opcode);
            rsvErr = fmt->validateReservedBits(fields);
        } catch (...) {
            rsvErr = "decode error";
        }

        if (!rsvErr.empty()) {
            if (plain) {
                out << ".word 0x" << hexAddr(word) << " ; invalid encoding\n";
            } else {
                out << "0x" << hexAddr(addr) << ":  " << hexAddr(word)
                    << "    .word 0x" << hexAddr(word) << " ; invalid encoding\n";
            }
            continue;
        }

        std::string mnem = def->mnemonic;
        for (char& c : mnem) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        struct LocalResolver : public ILabelResolver {
            uint32_t addr_;
            uint32_t baseAddr_;
            size_t wordCount_;
            const std::map<uint32_t, std::string>& labels_;
            bool plain_;

            LocalResolver(uint32_t a, uint32_t b, size_t wc,
                          const std::map<uint32_t, std::string>& l, bool plain)
                : addr_(a), baseAddr_(b), wordCount_(wc), labels_(l), plain_(plain) {}

            std::string resolve(int32_t wordOffset) const override {
                int64_t target = static_cast<int64_t>(addr_) + 4
                               + static_cast<int64_t>(wordOffset) * 4;
                uint32_t targetAddr32 = static_cast<uint32_t>(target);
                bool inImage = (targetAddr32 >= baseAddr_ &&
                                targetAddr32 < baseAddr_ + static_cast<uint32_t>(wordCount_) * 4);
                if (!inImage) {
                    if (plain_) {
                        return std::to_string(wordOffset);
                    }
                    auto it = labels_.find(targetAddr32);
                    if (it != labels_.end())
                        return it->second + " ; external target";
                    return "L_" + hexAddr(targetAddr32) + " ; external target";
                }
                auto it = labels_.find(targetAddr32);
                if (it != labels_.end()) {
                    return it->second;
                }
                return "L_" + hexAddr(targetAddr32);
            }
        };

        LocalResolver labelResolver(addr, baseAddr, words.size(), labels, plain);

        std::string operands;
        try {
            operands = fmt->formatOperands(fields, def->mnemonic, rawRegs, labelResolver);
        } catch (...) {
            operands = "???";
        }

        std::string asmText = mnem + (operands.empty() ? "" : " " + operands);

        if (plain) {
            out << asmText << "\n";
        } else {
            out << "0x" << hexAddr(addr) << ":  " << hexAddr(word)
                << "    " << asmText << "\n";
        }
    }

    if (!plain) {
        uint32_t endAddr = baseAddr + static_cast<uint32_t>(words.size()) * 4;
        for (auto& [targetAddr, lbl] : labels) {
            if (targetAddr < baseAddr || targetAddr >= endAddr) {
                out << lbl << ": ; external target (outside image)\n";
            }
        }
    }

    return out.str();
}

DisasmFmt detectFormat(const std::string& filename) {
    std::ifstream f(filename, std::ios::binary);
    if (!f.is_open())
        throw std::runtime_error("cannot open '" + filename + "'");

    std::vector<char> buf(4096);
    f.read(buf.data(), static_cast<std::streamsize>(buf.size()));
    auto n = static_cast<size_t>(f.gcount());

    for (size_t i = 0; i < n; ++i) {
        unsigned char c = static_cast<unsigned char>(buf[i]);
        if (c < 0x09 || (c > 0x0D && c < 0x20) || c >= 0x7F) {
            return DisasmFmt::RAW;
        }
    }

    std::istringstream ss(std::string(buf.data(), n));
    std::string line;
    bool allBinary = true;
    bool hasNonBlank = false;
    while (std::getline(ss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t a = line.find_first_not_of(" \t");
        if (a == std::string::npos) continue;
        size_t b = line.find_last_not_of(" \t");
        std::string t = line.substr(a, b - a + 1);
        if (t.size() >= 1 && (t[0] == ';' || t[0] == '#')) continue;
        if (t.size() >= 2 && t[0] == '/' && t[1] == '/') continue;
        hasNonBlank = true;
        if (t.size() != 32) { allBinary = false; break; }
        for (char c : t)
            if (c != '0' && c != '1') { allBinary = false; break; }
        if (!allBinary) break;
    }
    if (hasNonBlank && allBinary) return DisasmFmt::BIN;
    return DisasmFmt::HEX;
}

static std::string trimStr(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

std::vector<uint32_t> loadWords(const std::string& filename, DisasmFmt fmt) {
    std::vector<uint32_t> words;

    if (fmt == DisasmFmt::RAW) {
        std::ifstream f(filename, std::ios::binary);
        if (!f.is_open())
            throw std::runtime_error("cannot open '" + filename + "'");
        uint8_t buf[4];
        while (f.read(reinterpret_cast<char*>(buf), 4)) {
            uint32_t w = (static_cast<uint32_t>(buf[0]) << 24)
                       | (static_cast<uint32_t>(buf[1]) << 16)
                       | (static_cast<uint32_t>(buf[2]) << 8)
                       |  static_cast<uint32_t>(buf[3]);
            words.push_back(w);
        }
        if (f.gcount() > 0) {
            throw std::runtime_error("raw binary file size is not a multiple of 4 bytes (trailing "
                                     + std::to_string(f.gcount()) + " byte(s))");
        }
        return words;
    }

    std::ifstream f(filename);
    if (!f.is_open())
        throw std::runtime_error("cannot open '" + filename + "'");

    std::string line;
    while (std::getline(f, line)) {
        std::string t = trimStr(line);
        if (t.empty()) continue;
        if (t[0] == ';' || t[0] == '#') continue;
        if (t.size() >= 2 && t[0] == '/' && t[1] == '/') continue;

        if (fmt == DisasmFmt::HEX) {
            if (t.size() >= 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X'))
                t = t.substr(2);
            if (t.size() > 8)
                throw std::runtime_error("hex line too long: '" + line + "'");
            uint32_t w = 0;
            for (char c : t) {
                unsigned char uc = static_cast<unsigned char>(c);
                if (!std::isxdigit(uc))
                    throw std::runtime_error("invalid hex character in '" + line + "'");
                w = w * 16 + (std::isdigit(uc) ? (uc - '0')
                    : (std::tolower(uc) - 'a' + 10));
            }
            words.push_back(w);
        } else {
            if (t.size() != 32)
                throw std::runtime_error("binary line must be exactly 32 chars: '" + line + "'");
            uint32_t w = 0;
            for (char c : t) {
                if (c != '0' && c != '1')
                    throw std::runtime_error("invalid bit character in '" + line + "'");
                w = (w << 1) | static_cast<uint32_t>(c - '0');
            }
            words.push_back(w);
        }
    }
    return words;
}
