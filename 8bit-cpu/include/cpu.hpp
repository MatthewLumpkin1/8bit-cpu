#ifndef CPU_HPP
#define CPU_HPP

#include <string>
#include <vector>

#include "alu.hpp"
#include "memory.hpp"
#include "registers.hpp"

// What one instruction did, saved so the debugger can print it.
struct TraceRecord {
    int cycle;
    unsigned char pcBefore;
    unsigned char instructionByte;
    std::string disassembly;
    std::string aluOperation;
    std::string memoryAccess;
};

class CPU {
public:
    CPU();

    RegisterFile registers;
    Memory memory;
    Flags flags;

    unsigned char programCounter;      // address of the next instruction
    unsigned char instructionRegister; // the byte currently being executed
    bool halted;
    int cycles;                        // instructions retired, not clock ticks

    void reset();
    void loadProgram(const std::vector<unsigned char>& program, unsigned char startAddress);

    TraceRecord step();
    void run(int maxCycles);

    std::string stateText(const TraceRecord& trace);
    TraceRecord getLastTrace();

private:
    unsigned char fetch();
    TraceRecord lastTrace;
};

#endif
