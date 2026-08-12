#ifndef ALU_HPP
#define ALU_HPP

#include <string>

// The ALU is combinational logic: outputs depend only on the current inputs, so
// these are plain functions with no stored state. The flags it produces are
// latched by the CPU's flag register, the same way a real flag register catches
// the ALU's status outputs.

struct Flags {
    bool zero;
    bool carry;      // carry out on ADD/INC, borrow on SUB/CMP/DEC, shifted-out bit on SHL/SHR
    bool negative;   // bit 7, the sign bit in two's complement
};

const int ALU_ADD = 0;
const int ALU_SUB = 1;
const int ALU_AND = 2;
const int ALU_OR  = 3;
const int ALU_XOR = 4;
const int ALU_NOT = 5;
const int ALU_INC = 6;
const int ALU_DEC = 7;
const int ALU_SHL = 8;
const int ALU_SHR = 9;
const int ALU_CMP = 10;

Flags makeClearFlags();

// Computes the operation and writes the resulting status into flags.
// CMP returns the difference but the caller throws it away and keeps the flags.
unsigned char aluExecute(int operation, unsigned char a, unsigned char b, Flags& flags);

std::string aluOperationName(int operation);

#endif
