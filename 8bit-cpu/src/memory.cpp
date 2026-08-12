#include "memory.hpp"

#include <cstdio>
#include <stdexcept>

Memory::Memory() {
    reset();
}

unsigned char Memory::read(unsigned char address) {
    readCount++;
    return cells[address];
}

void Memory::write(unsigned char address, unsigned char value) {
    writeCount++;
    cells[address] = value;
}

void Memory::loadProgram(const std::vector<unsigned char>& program, unsigned char startAddress) {
    if ((int)program.size() + (int)startAddress > MEMORY_SIZE) {
        throw std::runtime_error("program of " + std::to_string(program.size())
                                 + " bytes does not fit in memory at address "
                                 + std::to_string((int)startAddress));
    }
    for (int i = 0; i < (int)program.size(); i++) {
        cells[startAddress + i] = program[i];
    }
}

void Memory::reset() {
    for (int i = 0; i < MEMORY_SIZE; i++) {
        cells[i] = 0;
    }
    readCount = 0;
    writeCount = 0;
}

std::string Memory::hexDump(int firstAddress, int count) {
    std::string output;
    char buffer[16];

    for (int address = firstAddress; address < firstAddress + count && address < MEMORY_SIZE;
         address += 16) {
        snprintf(buffer, sizeof(buffer), "%02X: ", address);
        output += buffer;
        for (int i = address; i < address + 16 && i < MEMORY_SIZE; i++) {
            snprintf(buffer, sizeof(buffer), "%02X ", cells[i]);
            output += buffer;
        }
        output += "\n";
    }
    return output;
}
