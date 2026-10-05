// ============================================================
// macros.hpp — RISC201 macro table and macro expander.
//
// HOW TO ADD A MACRO:
//   Add ONE new row to kMacroTable in macros.cpp.
//   Columns: name (upper-case), operand register class, expansion lines.
//   Use %0 in expansion lines to reference the macro's single operand.
// ============================================================
#pragma once

#include "isa.hpp"
#include <string>
#include <vector>

// ============================================================
// MacroDef — one row in the macro table
// ============================================================
struct MacroDef {
    const char* name;         // upper-case macro name
    RegKind     operandClass; // GPR or FPR (determines which reg class is expected)
    int         lineCount;    // number of expansion lines
    const char* lines[4];     // expansion template lines (max 4); %0 = macro operand
};

// ============================================================
// MacroExpander — expands macro invocations into assembly lines.
//
// Usage (called by the Assembler before pass-1 sizing):
//   MacroExpander exp;
//   if (exp.isMacro("push")) {
//       auto lines = exp.expand("push", "r3");
//       // lines = { "sub sp, sp, 4", "st r3, 0[sp]" }
//   }
// ============================================================
class MacroExpander {
public:
    MacroExpander();

    // Returns true if 'name' (case-insensitive) is a known macro.
    bool isMacro(const std::string& name) const;

    // Returns the MacroDef for 'name' (case-insensitive).
    // Throws std::runtime_error if not found.
    const MacroDef& getDef(const std::string& name) const;

    // Expand a macro invocation.
    //   name:    macro name (case-insensitive)
    //   operand: the single register operand (e.g., "r3", "f1")
    // Returns the list of expanded assembly lines.
    // Validates that the operand has the correct register class.
    // Throws std::runtime_error on error.
    std::vector<std::string> expand(const std::string& name,
                                    const std::string& operand) const;

    // Return the number of instructions the macro expands to (for sizing).
    int expansionSize(const std::string& name) const;

private:
    // Index into kMacroTable keyed by upper-case macro name
    std::map<std::string, int> index_;
};
