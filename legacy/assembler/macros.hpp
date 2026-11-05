#pragma once

#include "isa.hpp"
#include <string>
#include <vector>

struct MacroDef {
    const char* name;
    RegKind     operandClass;
    int         lineCount;
    const char* lines[4];
};

class MacroExpander {
public:
    MacroExpander();

    bool isMacro(const std::string& name) const;

    const MacroDef& getDef(const std::string& name) const;

    std::vector<std::string> expand(const std::string& name,
                                    const std::string& operand) const;

    int expansionSize(const std::string& name) const;

private:
    std::map<std::string, int> index_;
};
