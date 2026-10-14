#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <cstdint>

using namespace std;

// ============================================================================
// 1. ARCHITECTURE DEFINITIONS
// ============================================================================

const map<uint32_t, string> OPCODES = {
    {0x00, "NOP"},    {0x01, "ADD"},    {0x02, "SUB"},    {0x03, "MUL"},
    {0x04, "DIV"},    {0x05, "MOD"},    {0x06, "AND"},    {0x07, "OR"},
    {0x08, "NOT"},    {0x09, "XOR"},    {0x0A, "LSL"},    {0x0B, "LSR"},
    {0x0C, "ASR"},    {0x0D, "CMP"},    {0x10, "MOV"},    {0x11, "LD"},
    {0x12, "ST"},     {0x20, "B"},      {0x21, "BEQ"},    {0x22, "BNE"},
    {0x23, "BGT"},    {0x24, "BLT"},    {0x25, "CALL"},   {0x26, "RET"},
    {0x27, "BNDCHK"}, {0x28, "HALT"},   {0x30, "FADD"},   {0x31, "FSUB"},
    {0x32, "FMUL"},   {0x33, "FDIV"},   {0x34, "FCMP"},   {0x35, "FLD"},
    {0x36, "FST"},    {0x37, "FMOV"}
};

// Returns standard register name (sp for 14, ra for 15, or r0-r15)
string get_reg(uint32_t r) {
    if (r == 14) return "sp";
    if (r == 15) return "ra";
    return "r" + to_string(r);
}

// Returns floating-point register name (f0-f15)
string get_freg(uint32_t r) {
    return "f" + to_string(r);
}

// ============================================================================
// 2. HELPER FUNCTIONS
// ============================================================================

// Converts a string to lowercase
string to_lower(string s) {
    for (char &c : s) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    return s;
}

// Parses a 32-bit machine word from a hex or binary line
bool parse_line_word(string line, uint32_t &out_word) {
    // Strip comments
    size_t comment_pos = line.find_first_of(";#@");
    if (comment_pos != string::npos) line = line.substr(0, comment_pos);
    size_t slash_pos = line.find("//");
    if (slash_pos != string::npos) line = line.substr(0, slash_pos);

    stringstream ss(line);
    string token;
    if (!(ss >> token)) return false; // empty line

    try {
        if (token.size() >= 2 && (token[0] == '0' && (token[1] == 'x' || token[1] == 'X'))) {
            out_word = static_cast<uint32_t>(stoul(token, nullptr, 16));
        } else if (token.size() == 32 && token.find_first_not_of("01") == string::npos) {
            out_word = static_cast<uint32_t>(stoul(token, nullptr, 2));
        } else {
            out_word = static_cast<uint32_t>(stoul(token, nullptr, 16));
        }
        return true;
    } catch (...) {
        return false;
    }
}

// ============================================================================
// 3. MAIN DISASSEMBLER LOGIC
// ============================================================================

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <input.hex|input.bin> [<output.asm>] [-o <output.asm>]\n";
        return 1;
    }

    string input_file;
    string output_file;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "-o") {
            if (++i < argc) output_file = argv[i];
        } else if (arg[0] != '-') {
            if (input_file.empty()) input_file = arg;
            else if (output_file.empty()) output_file = arg;
        }
    }

    if (input_file.empty()) {
        cerr << "Error: No input file specified.\n";
        return 1;
    }

    ifstream infile(input_file);
    if (!infile.is_open()) {
        cerr << "Error: Could not open " << input_file << "\n";
        return 1;
    }

    ofstream outfile;
    if (!output_file.empty()) {
        outfile.open(output_file);
        if (!outfile.is_open()) {
            cerr << "Error: Could not open output file '" << output_file << "'.\n";
            return 1;
        }
    }

    ostream &out = output_file.empty() ? cout : outfile;

    string line;
    uint32_t line_number = 0;

    // Single-pass: read line, decode word, output assembly
    while (getline(infile, line)) {
        line_number++;
        uint32_t word = 0;
        if (!parse_line_word(line, word)) continue;

        uint32_t opcode = (word >> 26) & 0x3F;
        uint32_t mode   = (word >> 24) & 0x03;
        uint32_t rd     = (word >> 20) & 0x0F;
        uint32_t rs1    = (word >> 16) & 0x0F;
        uint32_t rs2    = (word >> 12) & 0x0F;
        int16_t  imm16  = static_cast<int16_t>(word & 0xFFFF);
        uint16_t uimm16 = static_cast<uint16_t>(word & 0xFFFF);

        if (!OPCODES.count(opcode)) {
            out << ".word 0x" << hex << uppercase << word << dec << "\n";
            continue;
        }

        string op = to_lower(OPCODES.at(opcode));

        // Format 1: ALU 3-Operand (ADD, SUB, MUL, DIV, MOD, AND, OR, XOR, LSL, LSR, ASR)
        if ((opcode >= 0x01 && opcode <= 0x07) || (opcode >= 0x09 && opcode <= 0x0C)) {
            if (mode == 0) {
                out << op << " " << get_reg(rd) << ", " << get_reg(rs1) << ", " << get_reg(rs2) << "\n";
            } else if (mode == 1) {
                out << op << " " << get_reg(rd) << ", " << get_reg(rs1) << ", " << imm16 << "\n";
            } else if (mode == 2) {
                out << op << "u " << get_reg(rd) << ", " << get_reg(rs1) << ", " << uimm16 << "\n";
            } else if (mode == 3) {
                out << op << "h " << get_reg(rd) << ", " << get_reg(rs1) << ", " << uimm16 << "\n";
            }
        }
        // Format 2: Unary (NOT, MOV)
        else if (opcode == 0x08 || opcode == 0x10) {
            if (mode == 0) {
                out << op << " " << get_reg(rd) << ", " << get_reg(rs2) << "\n";
            } else if (mode == 1) {
                out << op << " " << get_reg(rd) << ", " << imm16 << "\n";
            } else if (mode == 2) {
                out << op << "u " << get_reg(rd) << ", " << uimm16 << "\n";
            } else if (mode == 3) {
                out << op << "h " << get_reg(rd) << ", " << uimm16 << "\n";
            }
        }
        // Format 3: Compare (CMP)
        else if (opcode == 0x0D) {
            if (mode == 0) {
                out << op << " " << get_reg(rs1) << ", " << get_reg(rs2) << "\n";
            } else if (mode == 1) {
                out << op << " " << get_reg(rs1) << ", " << imm16 << "\n";
            } else if (mode == 2) {
                out << op << "u " << get_reg(rs1) << ", " << uimm16 << "\n";
            } else if (mode == 3) {
                out << op << "h " << get_reg(rs1) << ", " << uimm16 << "\n";
            }
        }
        // Format 4: Memory & Bounds (LD, ST, FLD, FST, BNDCHK)
        else if (opcode == 0x11 || opcode == 0x12 || opcode == 0x35 || opcode == 0x36 || opcode == 0x27) {
            if (opcode == 0x27) { // BNDCHK
                out << "bndchk " << get_reg(rs1) << ", " << imm16 << "\n";
            } else if (opcode == 0x35 || opcode == 0x36) { // FLD, FST
                out << op << " " << get_freg(rd) << ", " << imm16 << "[" << get_reg(rs1) << "]\n";
            } else { // LD, ST
                out << op << " " << get_reg(rd) << ", " << imm16 << "[" << get_reg(rs1) << "]\n";
            }
        }
        // Format 5: Branch & Call (B, BEQ, BNE, BGT, BLT, CALL)
        else if (opcode >= 0x20 && opcode <= 0x25) {
            uint32_t raw_offset = word & 0x3FFFFFF;
            int32_t word_offset = (raw_offset & 0x2000000) 
                                ? static_cast<int32_t>(raw_offset | 0xFC000000) 
                                : static_cast<int32_t>(raw_offset);
            out << op << " " << word_offset << "\n";
        }
        // Format 6: No Operands (NOP, RET, HALT)
        else if (opcode == 0x00 || opcode == 0x26 || opcode == 0x28) {
            out << op << "\n";
        }
        // Format 7: FPU 3-Register (FADD, FSUB, FMUL, FDIV)
        else if (opcode >= 0x30 && opcode <= 0x33) {
            out << op << " " << get_freg(rd) << ", " << get_freg(rs1) << ", " << get_freg(rs2) << "\n";
        }
        // Format 8: FPU 2-Register (FCMP, FMOV)
        else if (opcode == 0x34) {
            out << "fcmp " << get_freg(rs1) << ", " << get_freg(rs2) << "\n";
        } else if (opcode == 0x37) {
            out << "fmov " << get_freg(rd) << ", " << get_freg(rs2) << "\n";
        }
    }

    infile.close();
    if (outfile.is_open()) outfile.close();

    return 0;
}
