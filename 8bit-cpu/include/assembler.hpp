#ifndef ASSEMBLER_HPP
#define ASSEMBLER_HPP

#include <map>
#include <string>
#include <vector>

struct AsmError {
    int line;
    std::string message;
};

struct AsmResult {
    std::vector<unsigned char> code;
    std::map<std::string, int> labels;    // label name -> address
    std::vector<std::string> listing;     // address, bytes, source
};

// Two-pass assembler. Throws AsmError on the first problem it finds.
AsmResult assemble(const std::string& source);

#endif
