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

map<string, uint32_t> OPCODES = {
    // ALU Operations
    {"ADD", 0x01}, 
    {"SUB", 0x02}, 
    {"MUL", 0x03}, 
    {"DIV", 0x04},
    {"MOD", 0x05}, 
    {"AND", 0x06}, 
    {"OR",  0x07}, 
    {"XOR", 0x09},
    {"LSL", 0x0A}, 
    {"LSR", 0x0B}, 
    {"ASR", 0x0C},
    
    // Unary & Move
    {"NOT", 0x08}, 
    {"MOV", 0x10},
    
    // Compare
    {"CMP", 0x0D},
    
    // Memory Operations
    {"LD",  0x11}, 
    {"ST",  0x12}, 
    {"BNDCHK", 0x27},
    
    // Control Flow
    {"B",   0x20}, 
    {"BEQ", 0x21}, 
    {"BNE", 0x22}, 
    {"BGT", 0x23},
    {"BLT", 0x24}, 
    {"CALL",0x25}, 
    {"RET", 0x26}, 
    {"HALT",0x28},
    {"NOP", 0x00},
    
    // Floating Point Unit (FPU)
    {"FADD", 0x30}, 
    {"FSUB", 0x31}, 
    {"FMUL", 0x32}, 
    {"FDIV", 0x33},
    {"FCMP", 0x34}, 
    {"FLD",  0x35}, 
    {"FST",  0x36}, 
    {"FMOV", 0x37}
};

// ============================================================================
// 2. HELPER FUNCTIONS
// ============================================================================

void to_upper(string &s) {
    for (char &c : s) {
        c = toupper(c);
    }
}

bool is_reg(const string &s) {
    if (s.empty()) return false;
    
    string upper_s = s;
    to_upper(upper_s);
    
    if (upper_s == "SP") return true;
    
    // Now recognizes both 'R' (R1) and 'F' (F1) as valid registers
    if (upper_s[0] == 'R' || upper_s[0] == 'F') return true;
    
    return false;
}

uint32_t parse_reg(const string &s) {
    string upper_s = s;
    to_upper(upper_s);
    
    if (upper_s == "SP") return 14;
    
    // If it starts with 'R' or 'F', just strip the first letter and return the number
    if (upper_s[0] == 'R' || upper_s[0] == 'F') {
        return stoi(upper_s.substr(1));
    }
    
    return 0;
}

int32_t parse_int(const string &s) {
    try {
        return stoi(s, nullptr, 0); // Handles both decimal and 0x hex
    } catch (...) {
        return 0;
    }
}

vector<string> tokenize(string line) {
    // Replace punctuation with spaces for clean parsing
    replace(line.begin(), line.end(), ',', ' ');
    replace(line.begin(), line.end(), '[', ' ');
    replace(line.begin(), line.end(), ']', ' ');
    
    vector<string> tokens;
    stringstream ss(line);
    string token;
    
    while (ss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

// ============================================================================
// 3. MAIN ASSEMBLER LOGIC
// ============================================================================

int main(int argc, char* argv[]) {
    
    // --- Parse Command Line Arguments ---
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <input.asm> <output> [--hex] [--symbols]\n";
        return 1;
    }

    string input_file  = argv[1];
    string output_file = argv[2];
    bool as_hex        = false;
    bool print_symbols = false;

    for (int i = 3; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--hex")     as_hex = true;
        if (arg == "--symbols") print_symbols = true;
    }

    // --- Open Input File ---
    ifstream infile(input_file);
    if (!infile.is_open()) {
        cerr << "Error: Could not open " << input_file << "\n";
        return 1;
    }

    uint32_t address_counter = 0;
    map<string, uint32_t> label_map;
    vector<vector<string>> clean_instructions;
    string line;

    // ========================================================================
    // PASS 1: Macro Expansion and Label Mapping
    // ========================================================================
    while (getline(infile, line)) {
        
        // 1. Strip comments
        size_t comment_pos = line.find('#');
        if (comment_pos != string::npos) {
            line = line.substr(0, comment_pos);
        }

        // 2. Process and store labels SAFELY
        size_t colon_pos = line.find(':');
        if (colon_pos != string::npos) {
            string potential_label = line.substr(0, colon_pos);
            
            // Trim whitespace around the potential label
            if (!potential_label.empty()) {
                potential_label.erase(0, potential_label.find_first_not_of(" \t"));
                size_t last_char = potential_label.find_last_not_of(" \t");
                if (last_char != string::npos) {
                    potential_label.erase(last_char + 1);
                }
            }
            
            // A valid label is a single word. If it contains spaces, it's not a label.
            if (potential_label.find_first_of(" \t") == string::npos && !potential_label.empty()) {
                label_map[potential_label] = address_counter;
                line = line.substr(colon_pos + 1); // Remove the label part from the line
            }
        }

        // 3. Tokenize remaining instruction
        vector<string> tokens = tokenize(line);
        if (tokens.empty()) continue;

        string mnemonic = tokens[0];
        to_upper(mnemonic);

        // 4. Macro Expansion
        if (mnemonic == "PUSH") {
            clean_instructions.push_back({"SUBI", "SP", "SP", "4"});
            clean_instructions.push_back({"ST", tokens[1], "0", "SP"});
            address_counter += 8;
            
        } else if (mnemonic == "POP") {
            clean_instructions.push_back({"LD", tokens[1], "0", "SP"});
            clean_instructions.push_back({"ADDI", "SP", "SP", "4"});
            address_counter += 8;
            
        } else if (mnemonic == "LI") {
            int32_t val    = parse_int(tokens[2]);
            uint16_t lower = val & 0xFFFF;
            uint16_t upper = (val >> 16) & 0xFFFF;
            
            clean_instructions.push_back({"ADDU", tokens[1], "R0", to_string(lower)});
            clean_instructions.push_back({"ADDH", tokens[1], tokens[1], to_string(upper)});
            address_counter += 8;
            
        } else if (mnemonic == "FMOVI") {
            float fval = stof(tokens[2]);
            uint32_t ival;
            memcpy(&ival, &fval, sizeof(ival));
            
            uint16_t lower = ival & 0xFFFF;
            uint16_t upper = (ival >> 16) & 0xFFFF;
            
            clean_instructions.push_back({"ADDU", "R15", "R0", to_string(lower)});
            clean_instructions.push_back({"ADDH", "R15", "R15", to_string(upper)});
            clean_instructions.push_back({"SUBI", "SP", "SP", "4"});
            clean_instructions.push_back({"ST", "R15", "0", "SP"});
            clean_instructions.push_back({"FLD", tokens[1], "0", "SP"});
            clean_instructions.push_back({"ADDI", "SP", "SP", "4"});
            address_counter += 24;
            
        } else {
            // Standard Hardware Instruction
            clean_instructions.push_back(tokens);
            address_counter += 4;
        }
    }
    infile.close();

    // ========================================================================
    // PASS 2: Binary Translation and Packing
    // ========================================================================
    ofstream outfile(output_file);
    if (!outfile.is_open()) {
        cerr << "Error: Could not create " << output_file << "\n";
        return 1;
    }

    uint32_t current_address = 0;

    for (auto &tokens : clean_instructions) {
        string raw_mnemonic = tokens[0];
        to_upper(raw_mnemonic);
        
        uint32_t opcode = 0;
        uint32_t mode   = 0;
        string base_op  = raw_mnemonic;

        // --- SAFE Explicit Mode & Suffix Handling ---
        char last_char = raw_mnemonic.back();
        bool has_mode_suffix = (last_char == 'I' || last_char == 'U' || last_char == 'H') && raw_mnemonic.length() > 1;

        if (OPCODES.count(raw_mnemonic)) {
            // Case 1: Exact match in the OPCODES map (e.g., ADD, LD, B)
            base_op = raw_mnemonic;
            opcode  = OPCODES[base_op];
            mode    = 0; 
            
            // Auto-upgrade to Immediate Mode (1) if the user forgot the 'I' suffix
            bool is_alu   = (opcode >= 0x01 && opcode <= 0x0D);
            bool is_unary = (opcode == 0x10 || opcode == 0x08);
            
            if (is_alu || is_unary) { 
                if (tokens.size() > 1 && !is_reg(tokens.back())) {
                    mode = 1; 
                }
            }
        } 
        else if (has_mode_suffix) {
            // Case 2: Instruction has a mode suffix (e.g., ADDI, SUBU, MOVH)
            base_op = raw_mnemonic.substr(0, raw_mnemonic.length() - 1);
            
            if (OPCODES.count(base_op)) {
                opcode = OPCODES[base_op];
                if      (last_char == 'I') mode = 1;
                else if (last_char == 'U') mode = 2;
                else if (last_char == 'H') mode = 3;
            } else {
                cerr << "Error: Unknown base instruction for mnemonic: " << raw_mnemonic << "\n";
                return 1;
            }
        } 
        else {
            cerr << "Error: Unknown instruction: " << raw_mnemonic << "\n";
            return 1;
        }

        uint32_t instr_32 = 0;

        // --- Format 1 (ALU 3-Operand) ---
        if (base_op == "ADD" || base_op == "SUB" || base_op == "MUL" || 
            base_op == "DIV" || base_op == "MOD" || base_op == "AND" || 
            base_op == "OR"  || base_op == "XOR" || base_op == "LSL" || 
            base_op == "LSR" || base_op == "ASR") {
            
            uint32_t rd         = parse_reg(tokens[1]);
            uint32_t rs1        = parse_reg(tokens[2]);
            uint32_t rs2_or_imm = (mode == 0) ? parse_reg(tokens[3]) : (parse_int(tokens[3]) & 0xFFFF);
            
            instr_32 = (opcode << 26) | (mode << 24) | (rd << 20) | (rs1 << 16) | 
                       (mode == 0 ? (rs2_or_imm << 12) : rs2_or_imm);
        }
        
        // --- Format 2 & 3 (Unary / Compare) ---
        else if (base_op == "NOT" || base_op == "MOV" || base_op == "CMP") {
            uint32_t r_target = parse_reg(tokens[1]);
            uint32_t last_val = (mode == 0) ? parse_reg(tokens[2]) : (parse_int(tokens[2]) & 0xFFFF);
            
            if (base_op == "CMP") {
                instr_32 = (opcode << 26) | (mode << 24) | (0 << 20) | (r_target << 16) | 
                           (mode == 0 ? (last_val << 12) : last_val);
            } else {
                instr_32 = (opcode << 26) | (mode << 24) | (r_target << 20) | (0 << 16) | 
                           (mode == 0 ? (last_val << 12) : last_val);
            }
        }
        
        // --- Format 4 (Memory) ---
        else if (base_op == "LD" || base_op == "ST" || base_op == "FLD" || 
                 base_op == "FST" || base_op == "BNDCHK") {
            
            uint32_t rd    = (base_op == "BNDCHK") ? 0 : parse_reg(tokens[1]);
            uint32_t imm16 = parse_int(tokens[2]) & 0xFFFF;
            uint32_t rs1   = parse_reg(tokens[3]);
            
            instr_32 = (opcode << 26) | (0 << 24) | (rd << 20) | (rs1 << 16) | imm16;
        }
        
        // --- Format 5 (Branch & Call) ---
        else if (base_op == "B"   || base_op == "BEQ" || base_op == "BNE" || 
                 base_op == "BGT" || base_op == "BLT" || base_op == "CALL") {
            
            string target = tokens[1];
            int32_t offset_words = 0;
            
            if (label_map.count(target)) {
                // PC-relative offset formula: (Target - PC - 4) / 4
                offset_words = (label_map[target] - current_address - 4) / 4;
            } else {
                offset_words = parse_int(target);
            }
            
            uint32_t offset26 = offset_words & 0x3FFFFFF;
            instr_32 = (opcode << 26) | offset26;
        }
        
        // --- Format 6 (No Operand) ---
        else if (base_op == "NOP" || base_op == "RET" || base_op == "HALT") {
            instr_32 = opcode << 26;
        }
        
        // --- Format 7 & 8 (Floating Point Unit) ---
        else if (base_op == "FADD" || base_op == "FSUB" || base_op == "FMUL" || base_op == "FDIV") {
            instr_32 = (opcode << 26) | (parse_reg(tokens[1]) << 20) | 
                       (parse_reg(tokens[2]) << 16) | (parse_reg(tokens[3]) << 12);
                       
        } else if (base_op == "FCMP") {
            instr_32 = (opcode << 26) | (parse_reg(tokens[1]) << 16) | (parse_reg(tokens[2]) << 12);
            
        } else if (base_op == "FMOV") {
            instr_32 = (opcode << 26) | (parse_reg(tokens[1]) << 20) | (parse_reg(tokens[2]) << 12);
        }

        // --- Write Instruction to File ---
        if (as_hex) {
            outfile << uppercase << setfill('0') << setw(8) << hex << instr_32 << "\n";
        } else {
            for (int bit = 31; bit >= 0; --bit) {
                outfile << ((instr_32 >> bit) & 1);
            }
            outfile << "\n";
        }

        current_address += 4;
    }

    outfile.close();

    // ========================================================================
    // OPTIONAL: Print Symbol Table
    // ========================================================================
    if (print_symbols) {
        cout << "\n--- Symbol Table ---\n";
        for (auto const& [label, addr] : label_map) {
            cout << left << setw(20) << label 
                 << " | DEC: " << setw(8) << dec << addr 
                 << " | HEX: 0x" << uppercase << setfill('0') << setw(4) << hex << addr << dec << "\n";
        }
        cout << "--------------------\n";
    }

    cout << "Assembly successful! Output written to " << output_file << "\n";
    return 0;
}