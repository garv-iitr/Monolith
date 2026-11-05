#pragma once

#include "isa.hpp"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

struct SymbolEntry {
    uint32_t    address;
    std::string section;
    bool        isEqu;
    int         srcLine;
};

using SymbolTable = std::map<std::string, SymbolEntry>;

struct AssemblyOptions {
    uint32_t textBase = 0x00000000;
    uint32_t dataBase = 0x00010000;
    bool     warnShiftRange = true;
};

struct AssemblySourceWord {
    uint32_t address;
    int sourceLine;
    std::string sourceText;
    std::string section;
    bool executable;
};

struct AssemblyResult {
    std::vector<uint32_t> code;
    std::vector<uint32_t> data;
    std::vector<AssemblySourceWord> sourceWords;
    SymbolTable           symbols;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::string           listing;
    uint32_t              textBase;
    uint32_t              dataBase;
    bool                  hasErrors() const { return !errors.empty(); }
};

class Assembler {
public:
    AssemblyResult assemble(const std::string& source,
                            const std::string& filename = "<stdin>",
                            const AssemblyOptions& opts = {});

private:
    enum class LineKind {
        INSTR,
        LI_HIGH,
        LI_LOW,
        DATA_WORD,
        DATA_SPACE,
    };

    struct ExpandedLine {
        LineKind    kind;
        uint32_t    address;
        std::string section;
        std::string mnemonic;
        std::vector<std::string> tokens;
        std::string liValue;
        int         liRd;
        std::vector<std::string> dataValues;
        uint32_t    spaceBytes = 0;
        int         srcLine = 0;
        std::string srcText;
    };

    std::string filename_;
    AssemblyOptions opts_;
    std::vector<std::string> errors_;
    std::vector<std::string> warnings_;

    void addError(int line, const std::string& msg);
    void addWarning(int line, const std::string& msg);

    struct ParsedLine {
        std::string label;
        std::string mnemonic;
        std::vector<std::string> tokens;
    };
    ParsedLine parseLine(const std::string& rawLine) const;

    static std::string stripComment(const std::string& line);

    static std::vector<std::string> splitOperands(const std::string& s);

    static std::string trim(const std::string& s);

    std::string resolveImm(const std::string& tok, const SymbolTable& syms) const;

    uint32_t resolveLabel(const std::string& name, const SymbolTable& syms, int srcLine) const;

    uint32_t resolveValue32(const std::string& tok, const SymbolTable& syms) const;

    static int64_t parseNumber(const std::string& s);

    void checkOverlap(const std::vector<ExpandedLine>& lines,
                      AssemblyResult& result) const;

    std::string listingLine(uint32_t addr, uint32_t word,
                            const std::string& src) const;
};
