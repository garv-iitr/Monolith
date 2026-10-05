// ============================================================
// formats.hpp — Abstract InstructionFormat base class and
//               Format1..Format8 subclasses for RISC201.
//
// HOW TO ADD A NEW FORMAT:
//   1. Create a subclass of InstructionFormat.
//   2. Implement all five virtual methods.
//   3. Add a case to InstructionFormat::create().
//   4. Add the new FmtId value to isa.hpp.
// ============================================================
#pragma once

#include "isa.hpp"
#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// ============================================================
// ILabelResolver — callback interface to resolve branch target
// addresses to label strings without requiring <functional>.
// ============================================================
class ILabelResolver {
public:
    virtual ~ILabelResolver() = default;
    virtual std::string resolve(int32_t wordOffset) const = 0;
};

// ============================================================
// FieldMap — key/value pairs of decoded instruction fields.
// All values are int64_t so that both signed and unsigned
// 32-bit quantities fit without loss.
// ============================================================
using FieldMap = std::map<std::string, int64_t>;

// ============================================================
// ImmResult — result of parsing one immediate operand token.
// mode: 1=signed, 2=unsigned, 3=high (imm16 << 16 by sim).
// ============================================================
struct ImmResult {
    int     mode;    // 1, 2, or 3
    int64_t value;   // numeric value (before range check)
};

// ============================================================
// InstructionFormat — abstract base class
//
// Every format subclass is STATELESS and REENTRANT.
// Use InstructionFormat::create(FmtId) to obtain an instance.
// ============================================================
class InstructionFormat {
public:
    virtual ~InstructionFormat() = default;

    // ----------------------------------------------------------
    // parseOperands()
    //   Called during pass 2 with fully-tokenised operands.
    //   For branches (Format5): the assembler pre-resolves the
    //   label to a decimal string of the word offset BEFORE
    //   calling this method, so it only sees numbers.
    //   For .equ references: the assembler substitutes the
    //   numeric value before calling.
    //   Throws std::runtime_error on any parse error.
    // ----------------------------------------------------------
    virtual FieldMap parseOperands(
        const std::vector<std::string>& tokens,
        const std::string& mnemonic) const = 0;

    // ----------------------------------------------------------
    // encode()
    //   Build a 32-bit instruction word from a FieldMap.
    //   The opcode is passed separately so one format class
    //   can serve multiple mnemonics (e.g., Format4 for LD/ST).
    // ----------------------------------------------------------
    virtual uint32_t encode(const FieldMap& fields,
                            uint8_t opcode) const = 0;

    // ----------------------------------------------------------
    // decode()
    //   Extract the fields of a 32-bit instruction word.
    //   The opcode is already known (extracted by the caller).
    // ----------------------------------------------------------
    virtual FieldMap decode(uint32_t word, uint8_t opcode) const = 0;

    // ----------------------------------------------------------
    // formatOperands()
    //   Turn a decoded FieldMap back into an assembly operand
    //   string (the part after the mnemonic).
    //   labelResolver: given a signed word offset, returns the
    //     matching label string (or "L_XXXXXXXX" if synthetic).
    // ----------------------------------------------------------
    virtual std::string formatOperands(
        const FieldMap& fields,
        const std::string& mnemonic,
        bool              rawRegs,
        const ILabelResolver& labelResolver) const = 0;

    // ----------------------------------------------------------
    // validateReservedBits()
    //   Return an error message if any "must-be-zero" field in
    //   the FieldMap is non-zero. Empty string = OK.
    // ----------------------------------------------------------
    virtual std::string validateReservedBits(
        const FieldMap& fields) const = 0;

    // ----------------------------------------------------------
    // expectedOperandCount()
    //   Number of comma-separated operand tokens this mnemonic
    //   expects (used in pass-1 operand-count validation).
    // ----------------------------------------------------------
    virtual int expectedOperandCount(
        const std::string& mnemonic) const = 0;

    // Factory: return a heap-allocated format object for fmtId.
    static std::unique_ptr<InstructionFormat> create(FmtId fmtId);

protected:
    // ---- Shared helpers used by all format subclasses --------

    // Parse a GPR token (case-insensitive). Returns 0..15.
    // Throws on bad register name or wrong register class.
    static int parseGPR(const std::string& tok);

    // Parse an FPR token (case-insensitive). Returns 0..15.
    static int parseFPR(const std::string& tok);

    // Parse an immediate token.
    //   Accepted forms (# prefix is optional everywhere):
    //     decimal:   [-]NNN
    //     hex:       [-]0xNNN
    //     binary:    [-]0bNNN
    //   Suffixes (must be the last character after digits):
    //     'u' / 'U' -> mode 2 (unsigned imm16)
    //     'h' / 'H' -> mode 3 (high form, raw 16-bit stored)
    //     none       -> mode 1 (signed imm16)
    //   Does NOT perform range checking (caller's responsibility).
    //   Throws on malformed token.
    static ImmResult parseImm(const std::string& tok);

    // Parse a memory operand token of the form:
    //   "offset[base]" or "offset(base)" or "[base]" or "(base)"
    // Returns {offset, base_register_index}.
    // Throws on malformed token.
    static std::pair<int64_t, int> parseMemOperand(const std::string& tok);

    // Sign-extend val from nbits to 64 bits
    static int64_t signExtend(uint64_t val, int nbits);
};

// ============================================================
// Format1 — ALU 3-operand
//   Instructions: ADD SUB MUL DIV MOD AND OR XOR LSL LSR ASR
//   [31:26] opcode | [25:24] mode | [23:20] rd | [19:16] rs1
//   Register form (mode=00): [15:12] rs2, [11:0]=0
//   Immediate form (mode=01/10/11): [15:0] imm16
// ============================================================
class Format1 : public InstructionFormat {
public:
    FieldMap parseOperands(const std::vector<std::string>& tokens,
                           const std::string& mnemonic) const override;
    uint32_t encode(const FieldMap& fields, uint8_t opcode) const override;
    FieldMap decode(uint32_t word, uint8_t opcode) const override;
    std::string formatOperands(const FieldMap& fields,
        const std::string& mnemonic, bool rawRegs,
        const ILabelResolver&) const override;
    std::string validateReservedBits(const FieldMap& fields) const override;
    int expectedOperandCount(const std::string&) const override { return 3; }
};

// ============================================================
// Format2 — ALU 2-operand, rs1 field always 0
//   Instructions: NOT MOV
//   [31:26] opcode | [25:24] mode | [23:20] rd | [19:16]=0
//   Register form: [15:12]=rs2, [11:0]=0
//   Immediate form: [15:0] imm16
// ============================================================
class Format2 : public InstructionFormat {
public:
    FieldMap parseOperands(const std::vector<std::string>& tokens,
                           const std::string& mnemonic) const override;
    uint32_t encode(const FieldMap& fields, uint8_t opcode) const override;
    FieldMap decode(uint32_t word, uint8_t opcode) const override;
    std::string formatOperands(const FieldMap& fields,
        const std::string& mnemonic, bool rawRegs,
        const ILabelResolver&) const override;
    std::string validateReservedBits(const FieldMap& fields) const override;
    int expectedOperandCount(const std::string&) const override { return 2; }
};

// ============================================================
// Format3 — Compare, rd field always 0
//   Instruction: CMP
//   [31:26] opcode | [25:24] mode | [23:20]=0 | [19:16] rs1
//   Register form: [15:12]=rs2  |  Immediate: [15:0] imm16
// ============================================================
class Format3 : public InstructionFormat {
public:
    FieldMap parseOperands(const std::vector<std::string>& tokens,
                           const std::string& mnemonic) const override;
    uint32_t encode(const FieldMap& fields, uint8_t opcode) const override;
    FieldMap decode(uint32_t word, uint8_t opcode) const override;
    std::string formatOperands(const FieldMap& fields,
        const std::string& mnemonic, bool rawRegs,
        const ILabelResolver&) const override;
    std::string validateReservedBits(const FieldMap& fields) const override;
    int expectedOperandCount(const std::string&) const override { return 2; }
};

// ============================================================
// Format4 — Memory base+offset
//   Instructions: LD ST FLD FST BNDCHK
//   [31:26] opcode | [25:24]=00 | [23:20] rd | [19:16] rs1
//                 | [15:0] signed imm16
//   Syntax:  ld rd, imm[rs1]  /  ld rd, imm(rs1)  /  ld rd, [rs1]
//            bndchk rs1, imm  (rd=0)
//   FLD/FST: rd/rs (field [23:20]) is an FPR; base [19:16] is a GPR.
//   LD/ST/BNDCHK: both registers are GPRs.
// ============================================================
class Format4 : public InstructionFormat {
public:
    FieldMap parseOperands(const std::vector<std::string>& tokens,
                           const std::string& mnemonic) const override;
    uint32_t encode(const FieldMap& fields, uint8_t opcode) const override;
    FieldMap decode(uint32_t word, uint8_t opcode) const override;
    std::string formatOperands(const FieldMap& fields,
        const std::string& mnemonic, bool rawRegs,
        const ILabelResolver&) const override;
    std::string validateReservedBits(const FieldMap& fields) const override;
    int expectedOperandCount(const std::string&) const override { return 2; }
};

// ============================================================
// Format5 — Branch / call (PC-relative, word offset)
//   Instructions: B BEQ BNE BGT BLT CALL
//   [31:26] opcode | [25:0] signed 26-bit word offset
//   offset = (target_addr - branch_addr) / 4
//   The assembler pre-converts the label to a decimal offset
//   string before calling parseOperands.
// ============================================================
class Format5 : public InstructionFormat {
public:
    FieldMap parseOperands(const std::vector<std::string>& tokens,
                           const std::string& mnemonic) const override;
    uint32_t encode(const FieldMap& fields, uint8_t opcode) const override;
    FieldMap decode(uint32_t word, uint8_t opcode) const override;
    std::string formatOperands(const FieldMap& fields,
        const std::string& mnemonic, bool rawRegs,
        const ILabelResolver& labelResolver) const override;
    std::string validateReservedBits(const FieldMap& fields) const override;
    int expectedOperandCount(const std::string&) const override { return 1; }
};

// ============================================================
// Format6 — No operands
//   Instructions: NOP RET HALT
//   [31:26] opcode | [25:0]=0
// ============================================================
class Format6 : public InstructionFormat {
public:
    FieldMap parseOperands(const std::vector<std::string>& tokens,
                           const std::string& mnemonic) const override;
    uint32_t encode(const FieldMap& fields, uint8_t opcode) const override;
    FieldMap decode(uint32_t word, uint8_t opcode) const override;
    std::string formatOperands(const FieldMap& fields,
        const std::string& mnemonic, bool rawRegs,
        const ILabelResolver&) const override;
    std::string validateReservedBits(const FieldMap& fields) const override;
    int expectedOperandCount(const std::string&) const override { return 0; }
};

// ============================================================
// Format7 — FPU 3-register
//   Instructions: FADD FSUB FMUL FDIV
//   [31:26] opcode | [25:24]=00 | [23:20] fd | [19:16] fs1
//                 | [15:12] fs2 | [11:0]=0
// ============================================================
class Format7 : public InstructionFormat {
public:
    FieldMap parseOperands(const std::vector<std::string>& tokens,
                           const std::string& mnemonic) const override;
    uint32_t encode(const FieldMap& fields, uint8_t opcode) const override;
    FieldMap decode(uint32_t word, uint8_t opcode) const override;
    std::string formatOperands(const FieldMap& fields,
        const std::string& mnemonic, bool rawRegs,
        const ILabelResolver&) const override;
    std::string validateReservedBits(const FieldMap& fields) const override;
    int expectedOperandCount(const std::string&) const override { return 3; }
};

// ============================================================
// Format8 — FPU compare / move
//   FCMP: [31:26]=0x34 | 00 | [23:20]=0 | [19:16] fs1 | [15:12] fs2 | [11:0]=0
//   FMOV: [31:26]=0x37 | 00 | [23:20] fd | [19:16]=0 | [15:12] fs2 | [11:0]=0
// ============================================================
class Format8 : public InstructionFormat {
public:
    FieldMap parseOperands(const std::vector<std::string>& tokens,
                           const std::string& mnemonic) const override;
    uint32_t encode(const FieldMap& fields, uint8_t opcode) const override;
    FieldMap decode(uint32_t word, uint8_t opcode) const override;
    std::string formatOperands(const FieldMap& fields,
        const std::string& mnemonic, bool rawRegs,
        const ILabelResolver&) const override;
    std::string validateReservedBits(const FieldMap& fields) const override;
    int expectedOperandCount(const std::string& mnemonic) const override;
};
