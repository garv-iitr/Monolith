// ============================================================
// main_asm.cpp — RISC201 assembler CLI
//
// Usage:
//   risc201asm <input.asm> [-o <output>] [--format hex|bin|raw]
//              [--list <file>] [--symbols]
//              [--text-base <addr>] [--data-base <addr>]
// ============================================================
#include "assembler.hpp"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// ============================================================
// Write words in the requested format to a stream
// ============================================================
static void writeWords(std::ostream& out,
                       const std::vector<uint32_t>& words,
                       const std::string& fmt) {
    if (fmt == "hex") {
        for (uint32_t w : words) {
            out << std::uppercase << std::hex
                << std::setfill('0') << std::setw(8) << w << "\n";
        }
    } else if (fmt == "bin") {
        for (uint32_t w : words) {
            for (int bit = 31; bit >= 0; --bit)
                out << ((w >> bit) & 1);
            out << "\n";
        }
    } else { // raw
        for (uint32_t w : words) {
            out.put(static_cast<char>((w >> 24) & 0xFF));
            out.put(static_cast<char>((w >> 16) & 0xFF));
            out.put(static_cast<char>((w >>  8) & 0xFF));
            out.put(static_cast<char>( w        & 0xFF));
        }
    }
}

// ============================================================
// Derive default output path from input path
// ============================================================
static std::string deriveOutput(const std::string& input,
                                const std::string& ext) {
    size_t dot = input.rfind('.');
    std::string stem = (dot != std::string::npos) ? input.substr(0, dot) : input;
    return stem + "." + ext;
}

// ============================================================
// Parse a hex address from a CLI argument
// ============================================================
static uint32_t parseAddr(const std::string& s) {
    if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        return static_cast<uint32_t>(std::stoul(s.substr(2), nullptr, 16));
    }
    return static_cast<uint32_t>(std::stoul(s, nullptr, 10));
}

// ============================================================
// main
// ============================================================
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: risc201asm <input.asm> [-o <output>] [--format hex|bin|raw]\n"
                  << "                  [--list <file>] [--symbols] [--image]\n"
                  << "                  [--text-base <addr>] [--data-base <addr>]\n"
                  << "Note: --image creates a single combined file (text + zero padding + data).\n"
                  << "      The default data base 0x10000 makes the image 64 KB of mostly zeros;\n"
                  << "      use --data-base 0x1000 (or similar) to keep it small.\n";
        return 1;
    }

    std::string inputFile;
    std::string outputFile;
    std::string format = "hex";
    std::string listFile;
    bool printSymbols = false;
    bool imageMode = false;
    AssemblyOptions opts;

    // ---- Argument parsing -----------------------------------
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg[0] != '-') {
            if (inputFile.empty()) { inputFile = arg; continue; }
            std::cerr << "Unexpected positional argument: " << arg << "\n";
            return 1;
        }
        if (arg == "-o") {
            if (++i >= argc) { std::cerr << "-o requires an argument\n"; return 1; }
            outputFile = argv[i];
        } else if (arg == "--format") {
            if (++i >= argc) { std::cerr << "--format requires hex|bin|raw\n"; return 1; }
            format = argv[i];
            for (char& ch : format) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            if (format != "hex" && format != "bin" && format != "raw") {
                std::cerr << "--format must be hex, bin, or raw\n"; return 1;
            }
        } else if (arg == "--list") {
            if (++i >= argc) { std::cerr << "--list requires a filename\n"; return 1; }
            listFile = argv[i];
        } else if (arg == "--symbols") {
            printSymbols = true;
        } else if (arg == "--image") {
            imageMode = true;
        } else if (arg == "--text-base") {
            if (++i >= argc) { std::cerr << "--text-base requires an address\n"; return 1; }
            opts.textBase = parseAddr(argv[i]);
        } else if (arg == "--data-base") {
            if (++i >= argc) { std::cerr << "--data-base requires an address\n"; return 1; }
            opts.dataBase = parseAddr(argv[i]);
        } else {
            std::cerr << "Unknown argument: " << arg << "\n";
            return 1;
        }
    }

    if (inputFile.empty()) {
        std::cerr << "No input file specified.\n";
        return 1;
    }

    // Derive default output filename
    if (outputFile.empty())
        outputFile = deriveOutput(inputFile, format);

    // ---- Read source file -----------------------------------
    std::ifstream inF(inputFile);
    if (!inF.is_open()) {
        std::cerr << "Cannot open input file: " << inputFile << "\n";
        return 1;
    }
    std::ostringstream srcBuf;
    srcBuf << inF.rdbuf();
    std::string source = srcBuf.str();

    // ---- Assemble ------------------------------------------
    Assembler assembler;
    AssemblyResult result = assembler.assemble(source, inputFile, opts);

    // ---- Print warnings ------------------------------------
    for (const auto& w : result.warnings)
        std::cerr << w << "\n";

    // ---- Print errors --------------------------------------
    if (result.hasErrors()) {
        for (const auto& err : result.errors)
            std::cerr << err << "\n";
        return 1;
    }

    // ---- Write output --------------------------------------
    if (imageMode) {
        // Build one word list that starts at the text base address:
        // the text words, then zero words until the data base address, then the data words.
        std::vector<uint32_t> imageWords = result.code;
        size_t dataStart = (result.dataBase - result.textBase) / 4;
        if (!result.data.empty()) {
            if (imageWords.size() < dataStart) {
                imageWords.resize(dataStart, 0);
            }
            imageWords.insert(imageWords.end(), result.data.begin(), result.data.end());
        } else {
            dataStart = imageWords.size();
        }

        std::ios::openmode mode = std::ios::out;
        if (format == "raw") mode |= std::ios::binary;
        std::ofstream outF(outputFile, mode);
        if (!outF.is_open()) {
            std::cerr << "Cannot open output file: " << outputFile << "\n";
            return 1;
        }
        writeWords(outF, imageWords, format);

        std::ostringstream baseOss;
        baseOss << "0x" << std::uppercase << std::hex
                << std::setfill('0') << std::setw(8) << result.textBase;
        std::cout << "image: base " << baseOss.str() << ", "
                  << std::dec << imageWords.size() << " words (data starts at word "
                  << dataStart << ")\n";
    } else {
        // ---- Write .text output --------------------------------
        {
            std::ios::openmode mode = std::ios::out;
            if (format == "raw") mode |= std::ios::binary;
            std::ofstream outF(outputFile, mode);
            if (!outF.is_open()) {
                std::cerr << "Cannot open output file: " << outputFile << "\n";
                return 1;
            }
            writeWords(outF, result.code, format);
        }

        // ---- Write .data output (if non-empty) -----------------
        if (!result.data.empty()) {
            // Derive data output filename: e.g., prog.hex -> prog_data.hex
            size_t dot = outputFile.rfind('.');
            std::string stem = (dot != std::string::npos)
                ? outputFile.substr(0, dot) : outputFile;
            std::string ext  = (dot != std::string::npos)
                ? outputFile.substr(dot + 1) : format;
            std::string dataOut = stem + "_data." + ext;

            std::ios::openmode mode = std::ios::out;
            if (format == "raw") mode |= std::ios::binary;
            std::ofstream outF(dataOut, mode);
            if (!outF.is_open()) {
                std::cerr << "Cannot open data output file: " << dataOut << "\n";
                return 1;
            }
            writeWords(outF, result.data, format);
            std::cout << "Data section written to: " << dataOut
                      << " (base 0x" << std::uppercase << std::hex
                      << std::setfill('0') << std::setw(8) << result.dataBase << ")\n";
        }
    }

    // ---- Write listing file --------------------------------
    if (!listFile.empty()) {
        std::ofstream lf(listFile);
        if (!lf.is_open()) {
            std::cerr << "Cannot open listing file: " << listFile << "\n";
            return 1;
        }
        lf << result.listing;
    }

    // ---- Print symbol table --------------------------------
    if (printSymbols) {
        std::cout << "\nSymbol Table:\n";
        // Use fixed-width strings for clean columnar output
        auto padRight = [](const std::string& s, int w) {
            return s.size() < static_cast<size_t>(w)
                ? s + std::string(static_cast<size_t>(w) - s.size(), ' ')
                : s;
        };
        std::cout << padRight("Name", 24)
                  << padRight("Address", 14)
                  << padRight("Section", 10)
                  << "Type\n";
        std::cout << std::string(56, '-') << "\n";
        for (const auto& [name, sym] : result.symbols) {
            std::ostringstream addrStr;
            addrStr << "0x" << std::uppercase << std::hex
                    << std::setfill('0') << std::setw(8) << sym.address;
            std::cout << padRight(name, 24)
                      << padRight(addrStr.str(), 14)
                      << padRight(sym.section, 10)
                      << (sym.isEqu ? "equ" : "label") << "\n";
        }
    }


    return 0;
}
