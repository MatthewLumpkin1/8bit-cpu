#include "alu.hpp"

Flags makeClearFlags() {
    Flags flags;
    flags.zero = false;
    flags.carry = false;
    flags.negative = false;
    return flags;
}

unsigned char aluExecute(int operation, unsigned char a, unsigned char b, Flags& flags) {
    // Work in 16 bits so the 9th bit still exists to be tested, then truncate.
    // That 9th bit is the carry - real hardware has a physical carry-out line
    // from the top adder stage, and this is the software equivalent.
    int wide = 0;

    if (operation == ALU_ADD) {
        wide = (int)a + (int)b;
        flags.carry = (wide > 0xFF);
    } else if (operation == ALU_SUB || operation == ALU_CMP) {
        wide = (int)a - (int)b;
        flags.carry = (a < b);          // borrow; see docs/instruction_set.md
    } else if (operation == ALU_INC) {
        wide = (int)a + 1;
        flags.carry = (wide > 0xFF);
    } else if (operation == ALU_DEC) {
        wide = (int)a - 1;
        flags.carry = (a == 0);
    } else if (operation == ALU_AND) {
        wide = a & b;
        flags.carry = false;
    } else if (operation == ALU_OR) {
        wide = a | b;
        flags.carry = false;
    } else if (operation == ALU_XOR) {
        wide = a ^ b;
        flags.carry = false;
    } else if (operation == ALU_NOT) {
        wide = ~a;
        flags.carry = false;
    } else if (operation == ALU_SHL) {
        flags.carry = ((a & 0x80) != 0);
        wide = a << 1;
    } else if (operation == ALU_SHR) {
        flags.carry = ((a & 0x01) != 0);
        wide = a >> 1;                  // logical shift: zero fills, no sign extension
    }

    unsigned char result = (unsigned char)(wide & 0xFF);
    flags.zero = (result == 0);
    flags.negative = ((result & 0x80) != 0);
    return result;
}

std::string aluOperationName(int operation) {
    if (operation == ALU_ADD) return "ADD";
    if (operation == ALU_SUB) return "SUB";
    if (operation == ALU_AND) return "AND";
    if (operation == ALU_OR)  return "OR";
    if (operation == ALU_XOR) return "XOR";
    if (operation == ALU_NOT) return "NOT";
    if (operation == ALU_INC) return "INC";
    if (operation == ALU_DEC) return "DEC";
    if (operation == ALU_SHL) return "SHL";
    if (operation == ALU_SHR) return "SHR";
    if (operation == ALU_CMP) return "CMP";
    return "?";
}
