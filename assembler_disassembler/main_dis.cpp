#include "disassembler.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static uint32_t parseAddr(const std::string& s) {
    if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        return static_cast<uint32_t>(std::stoul(s.substr(2), nullptr, 16));
    return static_cast<uint32_t>(std::stoul(s, nullptr, 10));
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: risc201dis <input> [--format hex|bin|raw]\n"
                  << "                  [--base <addr>] [--plain] [--raw-regs] [-o <file>]\n";
        return 1;
    }

    std::string inputFile;
    std::string outputFile;
    std::string fmtStr;
    uint32_t baseAddr = 0;
    bool plain   = false;
    bool rawRegs = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg[0] != '-') {
            if (inputFile.empty()) { inputFile = arg; continue; }
            std::cerr << "Unexpected argument: " << arg << "\n"; return 1;
        }
        if (arg == "-o") {
            if (++i >= argc) { std::cerr << "-o needs a filename\n"; return 1; }
            outputFile = argv[i];
        } else if (arg == "--format") {
            if (++i >= argc) { std::cerr << "--format needs hex|bin|raw\n"; return 1; }
            fmtStr = argv[i];
            for (char& ch : fmtStr) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        } else if (arg == "--base") {
            if (++i >= argc) { std::cerr << "--base needs an address\n"; return 1; }
            baseAddr = parseAddr(argv[i]);
        } else if (arg == "--plain") {
            plain = true;
        } else if (arg == "--raw-regs") {
            rawRegs = true;
        } else {
            std::cerr << "Unknown argument: " << arg << "\n"; return 1;
        }
    }

    if (inputFile.empty()) {
        std::cerr << "No input file specified.\n"; return 1;
    }

    DisasmFmt fmt;
    try {
        if (fmtStr.empty()) {
            fmt = detectFormat(inputFile);
        } else if (fmtStr == "hex") { fmt = DisasmFmt::HEX; }
        else if (fmtStr == "bin")  { fmt = DisasmFmt::BIN; }
        else if (fmtStr == "raw")  { fmt = DisasmFmt::RAW; }
        else { std::cerr << "Unknown format: " << fmtStr << "\n"; return 1; }
    } catch (const std::exception& e) {
        std::cerr << "Error detecting format: " << e.what() << "\n"; return 1;
    }

    std::vector<uint32_t> words;
    try {
        words = loadWords(inputFile, fmt);
    } catch (const std::exception& e) {
        std::cerr << "Error loading input: " << e.what() << "\n"; return 1;
    }

    Disassembler dis;
    std::string output;
    try {
        output = dis.disassemble(words, baseAddr, plain, rawRegs);
    } catch (const std::exception& e) {
        std::cerr << "Disassembly error: " << e.what() << "\n"; return 1;
    }

    if (outputFile.empty()) {
        std::cout << output;
    } else {
        std::ofstream outF(outputFile);
        if (!outF.is_open()) {
            std::cerr << "Cannot open output file: " << outputFile << "\n"; return 1;
        }
        outF << output;
    }

    return 0;
}
