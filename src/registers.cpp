#include "registers.hpp"

#include <stdexcept>

RegisterFile::RegisterFile() {
    reset();
}

// A 2-bit field cannot hold an invalid register number, so decoded machine code
// can never reach this check. It guards the assembler and test paths, where an
// index comes from parsed text instead.
static void checkIndex(int index) {
    if (index < 0 || index >= REGISTER_COUNT) {
        throw std::out_of_range("register index " + std::to_string(index)
                                + " out of range (R0-R3)");
    }
}

unsigned char RegisterFile::read(int index) {
    checkIndex(index);
    return registers[index];
}

void RegisterFile::write(int index, unsigned char value) {
    checkIndex(index);
    registers[index] = value;
}

void RegisterFile::reset() {
    for (int i = 0; i < REGISTER_COUNT; i++) {
        registers[i] = 0;
    }
}

std::string RegisterFile::name(int index) {
    return "R" + std::to_string(index);
}
