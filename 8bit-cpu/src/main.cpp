#include "assembler.hpp"
#include "cpu.hpp"
#include "instruction.hpp"

#include <cstdio>
#include <map>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static bool hasFlag(const std::vector<std::string>& arguments, const std::string& flag) {
    for (int i = 0; i < (int)arguments.size(); i++) {
        if (arguments[i] == flag) {
            return true;
        }
    }
    return false;
}

static int getFlagValue(const std::vector<std::string>& arguments, const std::string& flag,
                        int defaultValue) {
    for (int i = 0; i + 1 < (int)arguments.size(); i++) {
        if (arguments[i] == flag) {
            return std::stoi(arguments[i + 1]);
        }
    }
    return defaultValue;
}

static void printUsage() {
    std::cout << "usage: cpu8 <program.asm> [options]\n\n"
              << "  --step        pause after each instruction (Enter = next, q = quit)\n"
              << "  --trace       print the full CPU state after every instruction\n"
              << "  --listing     show the assembler listing (address, bytes, source)\n"
              << "  --isa         print the instruction set table and exit\n"
              << "  --dump N      hex-dump the first N bytes of memory after HALT\n";
}

static void printInstructionSet() {
    char buffer[128];
    std::cout << "INSTRUCTION SET (" << getInstructionCount() << " instructions)\n\n";
    std::cout << "MNEMONIC  ENCODING  BYTES  MEANING\n";
    std::cout << "------------------------------------------------------------------------\n";

    for (int i = 0; i < getInstructionCount(); i++) {
        Instruction instruction = getInstruction(i);
        std::string encoding;
        if (instruction.opcode == OP_EXT) {
            snprintf(buffer, sizeof(buffer), "F%X..", instruction.subOpcode);
            encoding = buffer;
        } else {
            snprintf(buffer, sizeof(buffer), "%Xdds", instruction.opcode);
            encoding = buffer;
        }
        snprintf(buffer, sizeof(buffer), "%-9s %-9s %-6d %s\n",
                 instruction.mnemonic.c_str(), encoding.c_str(),
                 instruction.width, instruction.summary.c_str());
        std::cout << buffer;
    }
}

static std::string readFile(const std::string& path, bool& ok) {
    std::ifstream file(path);
    if (!file) {
        ok = false;
        return "";
    }
    std::stringstream contents;
    contents << file.rdbuf();
    ok = true;
    return contents.str();
}

int main(int argc, char** argv) {
    std::vector<std::string> arguments;
    for (int i = 1; i < argc; i++) {
        arguments.push_back(argv[i]);
    }

    if (hasFlag(arguments, "--isa")) {
        printInstructionSet();
        return 0;
    }
    if (arguments.empty()) {
        printUsage();
        return 1;
    }

    std::string path = arguments[0];
    bool stepMode = hasFlag(arguments, "--step");
    bool traceMode = hasFlag(arguments, "--trace") || stepMode;
    int dumpBytes = getFlagValue(arguments, "--dump", 0);

    bool fileOk = false;
    std::string source = readFile(path, fileOk);
    if (!fileOk) {
        std::cerr << "cannot open " << path << "\n";
        return 1;
    }

    AsmResult program;
    try {
        program = assemble(source);
    } catch (const AsmError& error) {
        std::cerr << path << ":" << error.line << ": error: " << error.message << "\n";
        return 2;
    }

    std::cout << "assembled " << program.code.size() << " bytes";
    if (!program.labels.empty()) {
        std::cout << ", labels:";
        std::map<std::string, int>::iterator it;
        for (it = program.labels.begin(); it != program.labels.end(); ++it) {
            std::cout << " " << it->first << "=" << it->second;
        }
    }
    std::cout << "\n";

    if (hasFlag(arguments, "--listing")) {
        std::cout << "\nLISTING\n";
        for (int i = 0; i < (int)program.listing.size(); i++) {
            std::cout << program.listing[i] << "\n";
        }
    }
    std::cout << "\n";

    CPU cpu;
    try {
        cpu.loadProgram(program.code, 0);
        while (!cpu.halted) {
            TraceRecord trace = cpu.step();
            if (traceMode) {
                std::cout << cpu.stateText(trace);
            }
            if (stepMode) {
                std::cout << "[Enter to step, q to quit] ";
                int key = std::cin.get();
                if (key == 'q' || key == EOF) {
                    break;
                }
            }
            if (cpu.cycles > 1000000) {
                throw std::runtime_error("no HALT after 1000000 instructions");
            }
        }
    } catch (const std::exception& error) {
        std::cerr << "runtime error: " << error.what() << "\n";
        return 3;
    }

    if (!traceMode) {
        std::cout << cpu.stateText(cpu.getLastTrace());
    }
    std::cout << "halted after " << cpu.cycles << " instructions ("
              << cpu.memory.readCount << " memory reads, "
              << cpu.memory.writeCount << " writes)\n";

    if (dumpBytes > 0) {
        std::cout << "\nMEMORY\n" << cpu.memory.hexDump(0, dumpBytes);
    }
    return 0;
}
