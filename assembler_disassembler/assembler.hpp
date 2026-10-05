// ============================================================
// assembler.hpp — RISC201 two-pass assembler public API.
//
// The assembler is designed to be linked as a library:
//   #include "assembler.hpp"
//   Assembler asm;
//   auto result = asm.assemble(source, opts);
// ============================================================
#pragma once

#include "isa.hpp"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// ============================================================
// SymbolEntry — one entry in the symbol table
// ============================================================
struct SymbolEntry {
    uint32_t    address;  // byte address (or .equ value)
    std::string section;  // "text", "data", or "equ"
    bool        isEqu;    // true for .equ constants
    int         srcLine;  // source line where defined
};

// Symbol table: case-sensitive label names
using SymbolTable = std::map<std::string, SymbolEntry>;

// ============================================================
// AssemblyOptions — settings passed from the CLI
// ============================================================
struct AssemblyOptions {
    uint32_t textBase = 0x00000000; // .text section base address
    uint32_t dataBase = 0x00010000; // .data section base address
    bool     warnShiftRange = true; // warn if shift amount outside 0..31
};

// ============================================================
// AssemblyResult — returned by Assembler::assemble()
// ============================================================
struct AssemblyResult {
    std::vector<uint32_t> code;    // .text section words (in order)
    std::vector<uint32_t> data;    // .data section words (in order)
    SymbolTable           symbols; // all labels and .equ constants
    std::vector<std::string> errors;   // "file:line: error: message"
    std::string           listing; // formatted listing (if requested)
    uint32_t              textBase; // actual .text base used
    uint32_t              dataBase; // actual .data base used
    bool                  hasErrors() const { return !errors.empty(); }
};

// ============================================================
// Assembler — the two-pass assembler
// ============================================================
class Assembler {
public:
    // Assemble 'source' (full text of the .asm file).
    // 'filename' is used only in error messages.
    // If opts.textBase/dataBase overlap, an error is added.
    AssemblyResult assemble(const std::string& source,
                            const std::string& filename = "<stdin>",
                            const AssemblyOptions& opts = {});

private:
    // ---- Internal representation of one expanded line -------
    enum class LineKind {
        INSTR,      // a real instruction (or macro-expanded instruction)
        LI_HIGH,    // first word of a 'li' expansion (mov rd, hi16h)
        LI_LOW,     // second word of a 'li' expansion (or rd, rd, lo16u)
        DATA_WORD,  // .word directive
        DATA_SPACE, // .space directive
    };

    struct ExpandedLine {
        LineKind    kind;
        uint32_t    address;     // byte address of this item
        std::string section;     // "text" or "data"
        std::string mnemonic;    // upper-case instruction mnemonic
        std::vector<std::string> tokens; // operand tokens (strings, not yet resolved)
        // For LI_HIGH/LI_LOW: the label or number whose hi/lo 16 bits we need
        std::string liValue;
        int         liRd;        // destination register for li
        // For DATA_WORD
        std::vector<std::string> dataValues;
        // For DATA_SPACE
        uint32_t    spaceBytes = 0;
        // Source information for listing / errors
        int         srcLine = 0;
        std::string srcText;
    };

    // ---- Internal state reset per assemble() call -----------
    std::string filename_;
    AssemblyOptions opts_;
    std::vector<std::string> errors_;

    // ---- Helpers --------------------------------------------
    void addError(int line, const std::string& msg);

    // Tokenise one source line: return {label, mnemonic, operand_tokens}
    // Strips comments, extracts leading label.
    struct ParsedLine {
        std::string label;
        std::string mnemonic;
        std::vector<std::string> tokens;
    };
    ParsedLine parseLine(const std::string& rawLine) const;

    // Strip a comment from a line (handles @, ;, //)
    static std::string stripComment(const std::string& line);

    // Split operands by comma; each result is trimmed
    static std::vector<std::string> splitOperands(const std::string& s);

    // Trim leading/trailing whitespace
    static std::string trim(const std::string& s);

    // Resolve an immediate token against the symbol table.
    // May be a plain number, or a .equ name (possibly with u/h suffix).
    // Returns the resolved token as a string (number + suffix preserved).
    // Throws on undefined name.
    std::string resolveImm(const std::string& tok, const SymbolTable& syms) const;

    // Resolve a label operand (for branches) to its byte address.
    // Throws on undefined label.
    uint32_t resolveLabel(const std::string& name, const SymbolTable& syms, int srcLine) const;

    // Resolve a value (number or .equ or label) to a 32-bit unsigned integer.
    // Used for .word directives and 'li' expansions.
    uint32_t resolveValue32(const std::string& tok, const SymbolTable& syms) const;

    // Parse a numeric string (dec / 0x / 0b, optional leading #, no suffix).
    static int64_t parseNumber(const std::string& s);

    // Check section overlap
    void checkOverlap(const std::vector<ExpandedLine>& lines,
                      AssemblyResult& result) const;

    // Generate listing text for one expanded line
    std::string listingLine(uint32_t addr, uint32_t word,
                            const std::string& src) const;
};
