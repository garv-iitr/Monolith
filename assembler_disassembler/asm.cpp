#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iomanip>

using namespace std;

// ============================================================================
// 1. ARCHITECTURE DEFINITIONS
// ============================================================================

const map<string, uint32_t> OPCODES = {
    {"ADD", 0x01}, {"SUB", 0x02}, {"MUL", 0x03}, {"DIV", 0x04},
    {"MOD", 0x05}, {"AND", 0x06}, {"OR",  0x07}, {"XOR", 0x09},
    {"LSL", 0x0A}, {"LSR", 0x0B}, {"ASR", 0x0C}, {"NOT", 0x08}, 
    {"MOV", 0x10}, {"CMP", 0x0D}, {"LD",  0x11}, {"ST",  0x12}, 
    {"BNDCHK", 0x27}, {"B",   0x20}, {"BEQ", 0x21}, {"BNE", 0x22}, 
    {"BGT", 0x23}, {"BLT", 0x24}, {"CALL",0x25}, {"RET", 0x26}, 
    {"HALT",0x28}, {"NOP", 0x00}, {"FADD", 0x30}, {"FSUB", 0x31}, 
    {"FMUL", 0x32}, {"FDIV", 0x33}, {"FCMP", 0x34}, {"FLD",  0x35}, 
    {"FST",  0x36}, {"FMOV", 0x37}
};

struct SourceInstruction {
    uint32_t line_number;
    vector<string> tokens;
};

// ============================================================================
// 2. HELPER FUNCTIONS
// ============================================================================

// convert string to uppercase 
void to_upper(string &s) {
    for (char &c : s) c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
}

// checks if general-purpose register is valid (R0-R15, SP, RA)
bool is_gpr(const string &s) {
    if (s.empty()) return false;
    string upper_s = s;
    to_upper(upper_s);
    if (upper_s == "SP" || upper_s == "RA") return true;
    
    if (upper_s.length() > 1 && upper_s[0] == 'R') {
        string num_part = upper_s.substr(1);
        for (char c : num_part) {
            if (!isdigit(static_cast<unsigned char>(c))) return false;
        }
        try {
            int reg_num = stoi(num_part);
            return (reg_num >= 0 && reg_num <= 15);
        } catch (...) {
            return false;
        }
    }
    return false;
}

// checks if floating-point register is valid (F0-F15)
bool is_fpr(const string &s) {
    if (s.empty()) return false;
    string upper_s = s;
    to_upper(upper_s);
    
    if (upper_s.length() > 1 && upper_s[0] == 'F') {
        string num_part = upper_s.substr(1);
        for (char c : num_part) {
            if (!isdigit(static_cast<unsigned char>(c))) return false;
        }
        try {
            int reg_num = stoi(num_part);
            return (reg_num >= 0 && reg_num <= 15);
        } catch (...) {
            return false;
        }
    }
    return false;
}

// checks if register is valid
bool is_reg(const string &s) {
    return is_gpr(s) || is_fpr(s);
}

// Parses a general-purpose register
uint32_t parse_gpr(const string &s, uint32_t line_number) {
    string upper_s = s;
    to_upper(upper_s);
    
    if (upper_s == "SP") return 14;
    if (upper_s == "RA") return 15;
    
    if (upper_s.length() > 1 && upper_s[0] == 'R') {
        string num_part = upper_s.substr(1);
        for (char c : num_part) {
            if (!isdigit(static_cast<unsigned char>(c))) {
                cerr << "Error line " << line_number << ": Invalid GPR format '" << s << "'.\n";
                exit(1);
            }
        }
        try {
            int reg_num = stoi(num_part);
            if (reg_num < 0 || reg_num > 15) {
                cerr << "Error line " << line_number << ": GPR '" << s << "' out of bounds (0-15).\n";
                exit(1);
            }
            return static_cast<uint32_t>(reg_num);
        } catch (...) {
            cerr << "Error line " << line_number << ": Invalid GPR format '" << s << "'.\n";
            exit(1);
        }
    }
    if (is_fpr(s)) {
        cerr << "Error line " << line_number << ": Expected GPR (R0-R15, SP, RA), got FPR '" << s << "'.\n";
        exit(1);
    }
    cerr << "Error line " << line_number << ": Expected GPR register, got '" << s << "'.\n";
    exit(1);
}

// Parses a floating-point register
uint32_t parse_fpr(const string &s, uint32_t line_number) {
    string upper_s = s;
    to_upper(upper_s);
    
    if (upper_s.length() > 1 && upper_s[0] == 'F') {
        string num_part = upper_s.substr(1);
        for (char c : num_part) {
            if (!isdigit(static_cast<unsigned char>(c))) {
                cerr << "Error line " << line_number << ": Invalid FPR format '" << s << "'.\n";
                exit(1);
            }
        }
        try {
            int reg_num = stoi(num_part);
            if (reg_num < 0 || reg_num > 15) {
                cerr << "Error line " << line_number << ": FPR '" << s << "' out of bounds (0-15).\n";
                exit(1);
            }
            return static_cast<uint32_t>(reg_num);
        } catch (...) {
            cerr << "Error line " << line_number << ": Invalid FPR format '" << s << "'.\n";
            exit(1);
        }
    }
    if (is_gpr(s)) {
        cerr << "Error line " << line_number << ": Expected FPR (F0-F15), got GPR '" << s << "'.\n";
        exit(1);
    }
    cerr << "Error line " << line_number << ": Expected FPR register, got '" << s << "'.\n";
    exit(1);
}

// Parses a register
uint32_t parse_reg(const string &s, uint32_t line_number) {
    if (is_gpr(s)) return parse_gpr(s, line_number);
    if (is_fpr(s)) return parse_fpr(s, line_number);
    cerr << "Error line " << line_number << ": Expected register, got '" << s << "'.\n";
    exit(1);
}

// Parsing of immediate (supports optional leading '#', decimal, 0x hex, and 0b binary)
int32_t parse_int(string s, uint32_t line_number) {
    if (!s.empty() && s[0] == '#') s = s.substr(1);
    if (s.empty()) {
        cerr << "Syntax Error line " << line_number << ": Empty numeric immediate.\n";
        exit(1);
    }
    try {
        size_t idx = 0;
        string num_part = s;
        bool neg = false;
        if (!num_part.empty() && num_part[0] == '-') {
            neg = true;
            num_part = num_part.substr(1);
        } else if (!num_part.empty() && num_part[0] == '+') {
            num_part = num_part.substr(1);
        }
        
        int64_t val = 0;
        if (num_part.size() >= 2 && num_part[0] == '0' && (num_part[1] == 'x' || num_part[1] == 'X')) {
            val = stoll(num_part.substr(2), &idx, 16);
            if (idx != num_part.size() - 2) {
                cerr << "Syntax Error line " << line_number << ": Trailing characters in hex immediate '" << s << "'.\n";
                exit(1);
            }
        } else if (num_part.size() >= 2 && num_part[0] == '0' && (num_part[1] == 'b' || num_part[1] == 'B')) {
            val = stoll(num_part.substr(2), &idx, 2);
            if (idx != num_part.size() - 2) {
                cerr << "Syntax Error line " << line_number << ": Trailing characters in binary immediate '" << s << "'.\n";
                exit(1);
            }
        } else {
            val = stoll(num_part, &idx, 10);
            if (idx != num_part.size()) {
                cerr << "Syntax Error line " << line_number << ": Trailing characters in numeric immediate '" << s << "'.\n";
                exit(1);
            }
        }
        if (neg) val = -val;
        return static_cast<int32_t>(val); 
    } catch (...) {
        cerr << "Syntax Error line " << line_number << ": Expected numeric immediate, got '" << s << "'.\n";
        exit(1);
    }
}

// validates range and takes lower 16 bits
uint16_t validate_and_mask_imm(int32_t val, uint32_t mode, const string &op, uint32_t line_number) {
    if (mode == 1) { // signed 16-bit
        if (val < -32768 || val > 32767) {
            cerr << "Error line " << line_number << ": " << op 
                 << ": signed imm16 out of range: " << val << " (must be between -32768 and 32767).\n";
            exit(1);
        }
    } else if (mode == 2 || mode == 3) { // unsigned or high 16-bit
        if (val < 0 || val > 65535) {
            cerr << "Error line " << line_number << ": " << op 
                 << ": unsigned imm16 out of range: " << val << " (must be between 0 and 65535).\n";
            exit(1);
        }
    }
    return static_cast<uint16_t>(val & 0xFFFF);
}

// Splits an assembly line into clean whitespace-delimited tokens while stripping delimiters (brackets, parens, commas).
vector<string> tokenize(string line) {
    replace(line.begin(), line.end(), ',', ' ');
    replace(line.begin(), line.end(), '[', ' ');
    replace(line.begin(), line.end(), ']', ' ');
    replace(line.begin(), line.end(), '(', ' ');
    replace(line.begin(), line.end(), ')', ' ');
    vector<string> tokens;
    stringstream ss(line);
    string token;
    while (ss >> token) tokens.push_back(token);
    return tokens;
}

// ============================================================================
// 3. MAIN ASSEMBLER LOGIC
// ============================================================================

// Drives the two-pass assembly process: expands macros, resolves symbols, and outputs binary/hex machine code.
int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <input.asm> [<output>] [-o <output>] [--hex] [--symbols] [--format hex|bin]\n";
        return 1;
    }

    string input_file;
    string output_file;
    bool as_hex = false, print_symbols = false;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--hex") {
            as_hex = true;
        } else if (arg == "--symbols") {
            print_symbols = true;
        } else if (arg == "--format") {
            if (++i < argc && string(argv[i]) == "hex") as_hex = true;
        } else if (arg == "-o") {
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
    if (output_file.empty()) {
        output_file = as_hex ? "out.hex" : "out.bin";
    }

    ifstream infile(input_file);
    if (!infile.is_open()) {
        cerr << "Error: Could not open " << input_file << "\n";
        return 1;
    }

    uint32_t address_counter = 0;
    map<string, uint32_t> label_map;
    vector<SourceInstruction> clean_instructions;
    string line;
    uint32_t line_number = 0;

    // PASS 1: Macro Expansion and Label Mapping
    while (getline(infile, line)) {
        line_number++;
        // Remove line-start comments first
        size_t first_non_space = line.find_first_not_of(" \t\r\n");
        if (first_non_space != string::npos && line[first_non_space] == '#') {
            line = "";
        }
        // Remove comments (supports ';', '@', '//', and '#' comments while preserving '#imm')
        size_t comment_pos = string::npos;
        for (size_t i = 0; i < line.size(); ++i) {
            char c = line[i];
            if (c == ';' || c == '@') { comment_pos = i; break; }
            if (c == '/' && i + 1 < line.size() && line[i+1] == '/') { comment_pos = i; break; }
            if (c == '#') {
                if (i + 1 < line.size() && (isdigit(static_cast<unsigned char>(line[i+1])) || line[i+1] == '-' || line[i+1] == '+')) {
                    continue;
                }
                comment_pos = i;
                break;
            }
        }
        if (comment_pos != string::npos) line = line.substr(0, comment_pos);

        //label define
        size_t colon_pos;
        while ((colon_pos = line.find(':')) != string::npos) {
            string potential_label = line.substr(0, colon_pos);
            size_t first = potential_label.find_first_not_of(" \t");
            if (first == string::npos) {
                cerr << "Error line " << line_number << ": Empty label definition.\n";
                return 1;
            }
            size_t last = potential_label.find_last_not_of(" \t");
            potential_label = potential_label.substr(first, last - first + 1);

            if (potential_label.find_first_of(" \t") != string::npos) {
                cerr << "Error line " << line_number << ": Label '" << potential_label << "' contains whitespace.\n";
                return 1;
            }
            if (isdigit(static_cast<unsigned char>(potential_label[0]))) {
                cerr << "Error line " << line_number << ": Label '" << potential_label << "' cannot start with a digit.\n";
                return 1;
            }
            for (char c : potential_label) {
                if (!isalnum(static_cast<unsigned char>(c)) && c != '_') {
                    cerr << "Error line " << line_number << ": Invalid character '" << c << "' in label '" << potential_label << "'.\n";
                    return 1;
                }
            }

            string upper_lbl = potential_label;
            to_upper(upper_lbl); 
            
            if (is_reg(upper_lbl) || OPCODES.count(upper_lbl)) {
                cerr << "Error line " << line_number << ": Label '" << potential_label << "' conflicts with reserved keyword.\n";
                return 1;
            }

            if (label_map.count(upper_lbl)) {
                cerr << "Error line " << line_number << ": Duplicate label '" << potential_label << "'.\n";
                return 1;
            }
            label_map[upper_lbl] = address_counter;
            line = line.substr(colon_pos + 1); 
        }

        vector<string> tokens = tokenize(line);
        if (tokens.empty()) continue;

        string mnemonic = tokens[0];
        to_upper(mnemonic);

        // Macro Expansion
        if (mnemonic == "PUSH" || mnemonic == "POP" || mnemonic == "SPUSH" || mnemonic == "SPOP" ||
            mnemonic == "FPUSH" || mnemonic == "FPOP" || mnemonic == "SFPUSH" || mnemonic == "SFPOP") {
            if (tokens.size() != 2) { 
                cerr << "Error line " << line_number << ": " << mnemonic << " requires exactly 1 register operand.\n"; 
                return 1; 
            }
            bool is_fp_macro = (mnemonic == "FPUSH" || mnemonic == "FPOP" || mnemonic == "SFPUSH" || mnemonic == "SFPOP");
            if (is_fp_macro) {
                if (!is_fpr(tokens[1])) {
                    cerr << "Error line " << line_number << ": " << mnemonic << " requires an FPR (F0-F15), got '" << tokens[1] << "'.\n";
                    return 1;
                }
            } else {
                if (!is_gpr(tokens[1])) {
                    cerr << "Error line " << line_number << ": " << mnemonic << " requires a GPR (R0-R15, SP, RA), got '" << tokens[1] << "'.\n";
                    return 1;
                }
            }
            if (mnemonic == "PUSH") {
                clean_instructions.push_back({line_number, {"SUBI", "SP", "SP", "4"}});
                clean_instructions.push_back({line_number, {"ST", tokens[1], "0", "SP"}});
                address_counter += 8;
            } else if (mnemonic == "POP") {
                clean_instructions.push_back({line_number, {"LD", tokens[1], "0", "SP"}});
                clean_instructions.push_back({line_number, {"ADDI", "SP", "SP", "4"}});
                address_counter += 8;
            } else if (mnemonic == "SPUSH") {
                clean_instructions.push_back({line_number, {"BNDCHK", "SP", "-4"}});
                clean_instructions.push_back({line_number, {"SUBI", "SP", "SP", "4"}});
                clean_instructions.push_back({line_number, {"ST", tokens[1], "0", "SP"}});
                address_counter += 12;
            } else if (mnemonic == "SPOP") {
                clean_instructions.push_back({line_number, {"BNDCHK", "SP", "4"}});
                clean_instructions.push_back({line_number, {"LD", tokens[1], "0", "SP"}});
                clean_instructions.push_back({line_number, {"ADDI", "SP", "SP", "4"}});
                address_counter += 12;
            } else if (mnemonic == "FPUSH") {
                clean_instructions.push_back({line_number, {"SUBI", "SP", "SP", "4"}});
                clean_instructions.push_back({line_number, {"FST", tokens[1], "0", "SP"}});
                address_counter += 8;
            } else if (mnemonic == "FPOP") {
                clean_instructions.push_back({line_number, {"FLD", tokens[1], "0", "SP"}});
                clean_instructions.push_back({line_number, {"ADDI", "SP", "SP", "4"}});
                address_counter += 8;
            } else if (mnemonic == "SFPUSH") {
                clean_instructions.push_back({line_number, {"BNDCHK", "SP", "-4"}});
                clean_instructions.push_back({line_number, {"SUBI", "SP", "SP", "4"}});
                clean_instructions.push_back({line_number, {"FST", tokens[1], "0", "SP"}});
                address_counter += 12;
            } else if (mnemonic == "SFPOP") {
                clean_instructions.push_back({line_number, {"BNDCHK", "SP", "4"}});
                clean_instructions.push_back({line_number, {"FLD", tokens[1], "0", "SP"}});
                clean_instructions.push_back({line_number, {"ADDI", "SP", "SP", "4"}});
                address_counter += 12;
            }
        } else if (mnemonic == "LI") {
            if (tokens.size() != 3) { 
                cerr << "Error line " << line_number << ": LI requires 2 operands (register, 32-bit immediate).\n"; 
                return 1; 
            }
            if (!is_gpr(tokens[1])) {
                cerr << "Error line " << line_number << ": LI requires a destination GPR (R0-R15, SP, RA), got '" << tokens[1] << "'.\n";
                return 1;
            }
            int32_t val    = parse_int(tokens[2], line_number);
            uint32_t uval  = static_cast<uint32_t>(val);
            uint16_t lower = static_cast<uint16_t>(uval & 0xFFFF); 
            uint16_t upper = static_cast<uint16_t>((uval >> 16) & 0xFFFF);
            
            clean_instructions.push_back({line_number, {"MOVH", tokens[1], to_string(upper)}});
            clean_instructions.push_back({line_number, {"ORU", tokens[1], tokens[1], to_string(lower)}});
            address_counter += 8;
        } else {
            clean_instructions.push_back({line_number, tokens});
            address_counter += 4;
        }
    }
    infile.close();

    // PASS 2: Binary Translation and Packing
    ofstream outfile(output_file);
    if (!outfile.is_open()) {
        cerr << "Error: Could not open output file '" << output_file << "'.\n";
        return 1;
    }

    uint32_t current_address = 0;

    for (auto &item : clean_instructions) {
        auto &tokens = item.tokens;
        uint32_t line_num = item.line_number;
        string raw_mnemonic = tokens[0];
        to_upper(raw_mnemonic);

        uint32_t opcode = 0;
        uint32_t mode   = 0;
        string base_op  = raw_mnemonic;
        char last_char = raw_mnemonic.back();
        bool has_mode_suffix = (last_char == 'I' || last_char == 'U' || last_char == 'H') && raw_mnemonic.length() > 1;

        if (OPCODES.count(raw_mnemonic)) {
            base_op = raw_mnemonic;
            opcode  = OPCODES.at(base_op);
            mode    = 0; 
        } else if (has_mode_suffix) {
            base_op = raw_mnemonic.substr(0, raw_mnemonic.length() - 1);
            if (OPCODES.count(base_op)) {
                opcode = OPCODES.at(base_op);
                bool supports_modes = (opcode >= 0x01 && opcode <= 0x0D) || (opcode == 0x10);
                if (!supports_modes) {
                    cerr << "Error line " << line_num << ": Instruction '" << base_op 
                         << "' does not support immediate mode suffix '" << last_char << "'.\n";
                    return 1;
                }
                if      (last_char == 'I') mode = 1;
                else if (last_char == 'U') mode = 2;
                else if (last_char == 'H') mode = 3;
            } else { 
                cerr << "Error line " << line_num << ": Unknown base instruction: " << raw_mnemonic << "\n"; 
                return 1; 
            }
        } else { 
            cerr << "Error line " << line_num << ": Unknown instruction: " << raw_mnemonic << "\n"; 
            return 1; 
        }

        bool is_alu   = (opcode >= 0x01 && opcode <= 0x0D);
        bool is_unary = (opcode == 0x10 || opcode == 0x08);
        if ((is_alu || is_unary) && tokens.size() > 1 && !is_reg(tokens.back())) {
            if (mode == 0) mode = 1; 
        }

        uint32_t instr_32 = 0;

        // --- Format 1 (ALU 3-Operand) ---
        if (base_op == "ADD" || base_op == "SUB" || base_op == "MUL" || base_op == "DIV" || 
            base_op == "MOD" || base_op == "AND" || base_op == "OR"  || base_op == "XOR" || 
            base_op == "LSL" || base_op == "LSR" || base_op == "ASR") {
            
            if (tokens.size() != 4) { 
                cerr << "Error line " << line_num << ": " << base_op 
                     << " requires exactly 3 operands (rd, rs1, rs2/imm).\n"; 
                return 1; 
            }
            
            uint32_t rd  = parse_gpr(tokens[1], line_num);
            uint32_t rs1 = parse_gpr(tokens[2], line_num);
            uint32_t rs2_or_imm = 0;
            
            if (mode == 0) {
                rs2_or_imm = parse_gpr(tokens[3], line_num);
            } else {
                int32_t val = parse_int(tokens[3], line_num);
                rs2_or_imm = validate_and_mask_imm(val, mode, base_op, line_num);
            }
            
            instr_32 = (opcode << 26) | (mode << 24) | (rd << 20) | (rs1 << 16) | 
                       (mode == 0 ? (rs2_or_imm << 12) : rs2_or_imm);
        }
        
        // --- Format 2 & 3 (Unary / Compare) ---
        else if (base_op == "NOT" || base_op == "MOV" || base_op == "CMP") {
            if (tokens.size() != 3) { 
                cerr << "Error line " << line_num << ": " << base_op 
                     << " requires exactly 2 operands.\n"; 
                return 1; 
            }
            
            if (base_op == "CMP") {
                uint32_t rs1 = parse_gpr(tokens[1], line_num);
                uint32_t last_val = 0;
                
                if (mode == 0) {
                    last_val = parse_gpr(tokens[2], line_num);
                } else {
                    int32_t val = parse_int(tokens[2], line_num);
                    last_val = validate_and_mask_imm(val, mode, base_op, line_num);
                }
                
                instr_32 = (opcode << 26) | (mode << 24) | (0 << 20) | (rs1 << 16) | 
                           (mode == 0 ? (last_val << 12) : last_val);
            } else {
                uint32_t r_target = parse_gpr(tokens[1], line_num);
                uint32_t last_val = 0;
                
                if (mode == 0) {
                    last_val = parse_gpr(tokens[2], line_num);
                } else {
                    int32_t val = parse_int(tokens[2], line_num);
                    last_val = validate_and_mask_imm(val, mode, base_op, line_num);
                }
                
                instr_32 = (opcode << 26) | (mode << 24) | (r_target << 20) | (0 << 16) | 
                           (mode == 0 ? (last_val << 12) : last_val);
            }
        }
        
        // --- Format 4 (Memory) ---
        else if (base_op == "LD" || base_op == "ST" || base_op == "FLD" || base_op == "FST" || base_op == "BNDCHK") {
            uint32_t rd = 0, imm16 = 0, rs1 = 0;

            if (base_op == "BNDCHK") {
                if (tokens.size() != 3) {
                    cerr << "Error line " << line_num << ": BNDCHK requires 2 operands (rs1, signed imm16).\n";
                    return 1;
                }
                rs1 = parse_gpr(tokens[1], line_num);
                int32_t val = parse_int(tokens[2], line_num);
                imm16 = validate_and_mask_imm(val, 1, base_op, line_num);
            } else {
                if (tokens.size() != 3 && tokens.size() != 4) {
                    cerr << "Error line " << line_num << ": Invalid syntax for " << base_op 
                         << " (expected " << base_op << " rd, offset[rs1] or " << base_op << " rd, [rs1]).\n";
                    return 1;
                }
                bool is_fp_mem = (base_op == "FLD" || base_op == "FST");
                if (is_fp_mem) {
                    rd = parse_fpr(tokens[1], line_num);
                } else {
                    rd = parse_gpr(tokens[1], line_num);
                }
                
                if (tokens.size() == 4) {
                    int32_t val = parse_int(tokens[2], line_num);
                    imm16 = validate_and_mask_imm(val, 1, base_op, line_num);
                    rs1   = parse_gpr(tokens[3], line_num);
                } else if (tokens.size() == 3) {
                    imm16 = 0;
                    rs1   = parse_gpr(tokens[2], line_num);
                }
            }
            instr_32 = (opcode << 26) | (0 << 24) | (rd << 20) | (rs1 << 16) | imm16;
        }
        
        // --- Format 5 (Branch & Call) ---
        else if (base_op == "B"   || base_op == "BEQ" || base_op == "BNE" || 
                 base_op == "BGT" || base_op == "BLT" || base_op == "CALL") {
            if (tokens.size() != 2) { 
                cerr << "Error line " << line_num << ": " << base_op 
                     << " requires exactly 1 target operand (label or word offset).\n"; 
                return 1; 
            }
            string target = tokens[1];
            if (!target.empty() && target[0] == '#') target = target.substr(1);
            to_upper(target); 
            int32_t offset_words = 0;
            
            if (label_map.count(target)) {
                uint32_t target_addr = label_map.at(target);
                if (target_addr % 4 != 0) {
                    cerr << "Error line " << line_num << ": Branch target address 0x" << hex 
                         << target_addr << " is not word-aligned.\n";
                    return 1;
                }
                int64_t diff = static_cast<int64_t>(target_addr) - static_cast<int64_t>(current_address + 4);
                int64_t word_off = diff / 4;
                if (word_off < -33554432LL || word_off > 33554431LL) {
                    cerr << "Error line " << line_num << ": Branch offset " << dec << word_off 
                         << " words out of 26-bit range [-33554432, 33554431].\n";
                    return 1;
                }
                offset_words = static_cast<int32_t>(word_off);
            } else {
                if (isdigit(static_cast<unsigned char>(target[0])) || target[0] == '-' || target[0] == '+') {
                    int32_t raw_val = parse_int(target, line_num);
                    if (raw_val < -33554432 || raw_val > 33554431) {
                        cerr << "Error line " << line_num << ": Branch word offset " << raw_val 
                             << " out of 26-bit range.\n";
                        return 1;
                    }
                    offset_words = raw_val;
                } else {
                    cerr << "Error line " << line_num << ": Undefined label '" << target << "'.\n";
                    return 1;
                }
            }
            uint32_t offset26 = static_cast<uint32_t>(offset_words) & 0x3FFFFFF;
            instr_32 = (opcode << 26) | offset26;
        }
        
        // --- Format 6 (No Operand) ---
        else if (base_op == "NOP" || base_op == "RET" || base_op == "HALT") {
            if (tokens.size() != 1) {
                cerr << "Error line " << line_num << ": " << base_op << " takes no operands.\n";
                return 1;
            }
            instr_32 = opcode << 26;
        }
        
        // --- Format 7 & 8 (Floating Point Unit) ---
        else if (base_op == "FADD" || base_op == "FSUB" || base_op == "FMUL" || base_op == "FDIV") {
            if (tokens.size() != 4) { 
                cerr << "Error line " << line_num << ": " << base_op 
                     << " requires exactly 3 operands (fd, fs1, fs2).\n"; 
                return 1; 
            }
            instr_32 = (opcode << 26) | (parse_fpr(tokens[1], line_num) << 20) | 
                       (parse_fpr(tokens[2], line_num) << 16) | (parse_fpr(tokens[3], line_num) << 12);
        } else if (base_op == "FCMP") {
            if (tokens.size() != 3) { 
                cerr << "Error line " << line_num << ": FCMP requires exactly 2 operands (fs1, fs2).\n"; 
                return 1; 
            }
            instr_32 = (opcode << 26) | (parse_fpr(tokens[1], line_num) << 16) | (parse_fpr(tokens[2], line_num) << 12);
        } else if (base_op == "FMOV") {
            if (tokens.size() != 3) { 
                cerr << "Error line " << line_num << ": FMOV requires exactly 2 operands (fd, fs2).\n"; 
                return 1; 
            }
            instr_32 = (opcode << 26) | (parse_fpr(tokens[1], line_num) << 20) | (parse_fpr(tokens[2], line_num) << 12);
        }

        if (as_hex) {
            outfile << uppercase << setfill('0') << setw(8) << hex << instr_32 << "\n";
        } else {
            for (int bit = 31; bit >= 0; --bit) outfile << ((instr_32 >> bit) & 1);
            outfile << "\n";
        }
        current_address += 4;
    }

    outfile.close();

    if (print_symbols) {
        cout << "\nSymbol Table \n";
        for (auto const& [label, addr] : label_map) {
            cout << setfill(' ') << left << setw(20) << label 
                 << " | DEC: " << setw(8) << dec << addr 
                 << " | HEX: 0x" << uppercase << setfill('0') << right << setw(4) << hex << addr << dec << "\n";
        }
        cout << setfill(' ') << "\n";
    }

    cout << "Output written to " << output_file << "\n";
    return 0;
}
