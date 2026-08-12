#include "cpu.hpp"
#include "instruction.hpp"

#include <cstdio>
#include <stdexcept>

CPU::CPU() {
    reset();
}

void CPU::reset() {
    registers.reset();
    memory.reset();
    flags = makeClearFlags();
    programCounter = 0;
    instructionRegister = 0;
    halted = false;
    cycles = 0;

    lastTrace.cycle = 0;
    lastTrace.pcBefore = 0;
    lastTrace.instructionByte = 0;
    lastTrace.disassembly = "";
    lastTrace.aluOperation = "-";
    lastTrace.memoryAccess = "-";
}

void CPU::loadProgram(const std::vector<unsigned char>& program, unsigned char startAddress) {
    memory.loadProgram(program, startAddress);
    programCounter = startAddress;
}

// The program counter is an unsigned char, so incrementing past 0xFF wraps to
// 0x00 the same way an 8-bit hardware counter does.
unsigned char CPU::fetch() {
    unsigned char value = memory.read(programCounter);
    programCounter++;
    return value;
}

static std::string toBinary(unsigned char value) {
    std::string bits;
    for (int bit = 7; bit >= 0; bit--) {
        if ((value >> bit) & 1) {
            bits += "1";
        } else {
            bits += "0";
        }
    }
    return bits;
}

TraceRecord CPU::step() {
    if (halted) {
        return lastTrace;
    }

    TraceRecord trace;
    trace.cycle = cycles + 1;
    trace.pcBefore = programCounter;
    trace.aluOperation = "-";
    trace.memoryAccess = "-";

    // FETCH
    instructionRegister = fetch();
    trace.instructionByte = instructionRegister;

    // DECODE - split the byte into its fields, exactly what the control unit's
    // combinational logic does with the instruction register's output bits.
    int opcode = instructionRegister >> 4;
    int destination = (instructionRegister >> 2) & 0x3;
    int source = instructionRegister & 0x3;
    int subOpcode = 0;

    if (opcode == OP_EXT) {
        unsigned char extensionByte = fetch();
        subOpcode = extensionByte >> 4;
        destination = (extensionByte >> 2) & 0x3;
        source = extensionByte & 0x3;
    }

    int tableIndex = findInstructionByOpcode(opcode, subOpcode);
    if (tableIndex < 0) {
        throw std::runtime_error("illegal opcode at address "
                                 + std::to_string((int)trace.pcBefore));
    }

    std::string destinationName = RegisterFile::name(destination);
    std::string sourceName = RegisterFile::name(source);
    std::string text = getInstruction(tableIndex).mnemonic;

    // EXECUTE
    if (opcode == OP_NOP) {
        // nothing to do

    } else if (opcode == OP_MOV) {
        // A datapath transfer, not an ALU operation, so no flags change. That is
        // what lets a program compute a condition, shuffle registers, and then
        // branch on the earlier result.
        registers.write(destination, registers.read(source));
        text += " " + destinationName + ", " + sourceName;

    } else if (opcode == OP_MOVI) {
        unsigned char immediate = fetch();
        registers.write(destination, immediate);
        text += " " + destinationName + ", " + std::to_string((int)immediate);

    } else if (opcode == OP_LOAD) {
        unsigned char address = fetch();
        registers.write(destination, memory.read(address));
        trace.memoryAccess = "READ  [" + std::to_string((int)address) + "]";
        text += " " + destinationName + ", [" + std::to_string((int)address) + "]";

    } else if (opcode == OP_STORE) {
        unsigned char address = fetch();
        memory.write(address, registers.read(destination));
        trace.memoryAccess = "WRITE [" + std::to_string((int)address) + "]";
        text += " " + destinationName + ", [" + std::to_string((int)address) + "]";

    } else if (opcode == OP_ADD || opcode == OP_SUB || opcode == OP_AND
               || opcode == OP_OR || opcode == OP_XOR || opcode == OP_CMP) {
        int operation = ALU_ADD;
        if (opcode == OP_SUB) operation = ALU_SUB;
        if (opcode == OP_AND) operation = ALU_AND;
        if (opcode == OP_OR)  operation = ALU_OR;
        if (opcode == OP_XOR) operation = ALU_XOR;
        if (opcode == OP_CMP) operation = ALU_CMP;

        unsigned char result = aluExecute(operation, registers.read(destination),
                                          registers.read(source), flags);
        if (opcode != OP_CMP) {
            registers.write(destination, result);
        }
        trace.aluOperation = aluOperationName(operation);
        text += " " + destinationName + ", " + sourceName;

    } else if (opcode == OP_JMP) {
        unsigned char address = fetch();
        programCounter = address;
        text += " " + std::to_string((int)address);

    } else if (opcode == OP_JZ) {
        unsigned char address = fetch();
        if (flags.zero) {
            programCounter = address;
        }
        text += " " + std::to_string((int)address);

    } else if (opcode == OP_JNZ) {
        unsigned char address = fetch();
        if (!flags.zero) {
            programCounter = address;
        }
        text += " " + std::to_string((int)address);

    } else if (opcode == OP_EXT) {
        if (subOpcode == SUB_HALT) {
            halted = true;

        } else if (subOpcode == SUB_JC) {
            unsigned char address = fetch();
            if (flags.carry) {
                programCounter = address;
            }
            text += " " + std::to_string((int)address);

        } else if (subOpcode == SUB_LOADR) {
            unsigned char address = registers.read(source);
            registers.write(destination, memory.read(address));
            trace.memoryAccess = "READ  [" + sourceName + "=" + std::to_string((int)address) + "]";
            text += " " + destinationName + ", [" + sourceName + "]";

        } else if (subOpcode == SUB_STORER) {
            unsigned char address = registers.read(destination);
            memory.write(address, registers.read(source));
            trace.memoryAccess = "WRITE [" + destinationName + "=" + std::to_string((int)address) + "]";
            text += " [" + destinationName + "], " + sourceName;

        } else {
            int operation = ALU_NOT;
            if (subOpcode == SUB_INC) operation = ALU_INC;
            if (subOpcode == SUB_DEC) operation = ALU_DEC;
            if (subOpcode == SUB_SHL) operation = ALU_SHL;
            if (subOpcode == SUB_SHR) operation = ALU_SHR;

            unsigned char result = aluExecute(operation, registers.read(destination), 0, flags);
            registers.write(destination, result);
            trace.aluOperation = aluOperationName(operation);
            text += " " + destinationName;
        }

    } else {
        throw std::runtime_error("reserved opcode 0xE at address "
                                 + std::to_string((int)trace.pcBefore));
    }

    trace.disassembly = text;
    cycles++;
    lastTrace = trace;
    return trace;
}

void CPU::run(int maxCycles) {
    while (!halted && cycles < maxCycles) {
        step();
    }
    if (!halted) {
        throw std::runtime_error("cycle limit of " + std::to_string(maxCycles)
                                 + " reached without HALT - the program probably loops forever");
    }
}

TraceRecord CPU::getLastTrace() {
    return lastTrace;
}

std::string CPU::stateText(const TraceRecord& trace) {
    char buffer[64];
    std::string output;

    output += "--------------------------------\n";
    output += "Cycle: " + std::to_string(trace.cycle) + "\n";

    snprintf(buffer, sizeof(buffer), "PC:    0x%02X  ->  0x%02X\n",
             trace.pcBefore, programCounter);
    output += buffer;

    snprintf(buffer, sizeof(buffer), "IR:    0x%02X  (%s)\n",
             trace.instructionByte, toBinary(trace.instructionByte).c_str());
    output += buffer;

    output += "\nInstruction:\n" + trace.disassembly + "\n\nREGISTERS\n";
    for (int i = 0; i < REGISTER_COUNT; i++) {
        unsigned char value = registers.read(i);
        snprintf(buffer, sizeof(buffer), "%s: %s  %3d  0x%02X\n",
                 RegisterFile::name(i).c_str(), toBinary(value).c_str(), value, value);
        output += buffer;
    }

    output += "\nFLAGS\n";
    snprintf(buffer, sizeof(buffer), "Z: %d   C: %d   N: %d\n",
             (int)flags.zero, (int)flags.carry, (int)flags.negative);
    output += buffer;

    output += "\nALU:           " + trace.aluOperation + "\n";
    output += "Memory Access: " + trace.memoryAccess + "\n";
    output += "--------------------------------\n";
    return output;
}
