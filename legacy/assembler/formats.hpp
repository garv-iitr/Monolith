#pragma once

#include "isa.hpp"
#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

class ILabelResolver {
public:
    virtual ~ILabelResolver() = default;
    virtual std::string resolve(int32_t wordOffset) const = 0;
};

using FieldMap = std::map<std::string, int64_t>;

struct ImmResult {
    int     mode;
    int64_t value;
};

class InstructionFormat {
public:
    virtual ~InstructionFormat() = default;

    virtual FieldMap parseOperands(
        const std::vector<std::string>& tokens,
        const std::string& mnemonic) const = 0;

    virtual uint32_t encode(const FieldMap& fields,
                            uint8_t opcode) const = 0;

    virtual FieldMap decode(uint32_t word, uint8_t opcode) const = 0;

    virtual std::string formatOperands(
        const FieldMap& fields,
        const std::string& mnemonic,
        bool              rawRegs,
        const ILabelResolver& labelResolver) const = 0;

    virtual std::string validateReservedBits(
        const FieldMap& fields) const = 0;

    virtual int expectedOperandCount(
        const std::string& mnemonic) const = 0;

    static std::unique_ptr<InstructionFormat> create(FmtId fmtId);

protected:

    static int parseGPR(const std::string& tok);

    static int parseFPR(const std::string& tok);

    static ImmResult parseImm(const std::string& tok);

    static std::pair<int64_t, int> parseMemOperand(const std::string& tok);

    static int64_t signExtend(uint64_t val, int nbits);
};

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
