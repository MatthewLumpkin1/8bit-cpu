#ifndef INSTRUCTION_HPP
#define INSTRUCTION_HPP

#include <string>

// Instruction encoding, one byte:
//
//    7  6  5  4 | 3  2 | 1  0
//    [ opcode  ][ dst ][ src ]
//
// Instructions with an immediate or address add a second byte.
//
// Opcode 0xF is an escape into a second opcode map. A 4-bit opcode plus two
// 2-bit register fields uses all 8 bits, which caps the ISA at 16 instructions,
// and this one has 23. Single-operand instructions live in the extended map
// because they only need one register field, which leaves 2 spare bits in the
// extension byte - that is what makes LOADR/STORER fit.
//
// Extension byte: [ sub-opcode ][ dst ][ src ]

const int OP_NOP   = 0x0;
const int OP_MOV   = 0x1;
const int OP_MOVI  = 0x2;
const int OP_LOAD  = 0x3;
const int OP_STORE = 0x4;
const int OP_ADD   = 0x5;
const int OP_SUB   = 0x6;
const int OP_AND   = 0x7;
const int OP_OR    = 0x8;
const int OP_XOR   = 0x9;
const int OP_CMP   = 0xA;
const int OP_JMP   = 0xB;
const int OP_JZ    = 0xC;
const int OP_JNZ   = 0xD;
const int OP_RESERVED = 0xE;   // left free for a future instruction, e.g. CALL
const int OP_EXT   = 0xF;

const int SUB_NOT    = 0x0;
const int SUB_INC    = 0x1;
const int SUB_DEC    = 0x2;
const int SUB_SHL    = 0x3;
const int SUB_SHR    = 0x4;
const int SUB_HALT   = 0x5;
const int SUB_JC     = 0x6;
const int SUB_LOADR  = 0x7;
const int SUB_STORER = 0x8;

// Operand shape. Tells the assembler how to parse the line and how many bytes
// the instruction takes.
const int FORM_NONE         = 0;   // NOP, HALT
const int FORM_REG_REG      = 1;   // ADD R0, R1
const int FORM_REG_IMM      = 2;   // MOVI R0, 42
const int FORM_REG_ADDR     = 3;   // LOAD R0, [200]
const int FORM_ADDR         = 4;   // JMP loop
const int FORM_REG          = 5;   // INC R0
const int FORM_REG_INDIRECT = 6;   // LOADR R0, [R1]

struct Instruction {
    std::string mnemonic;
    int opcode;
    int subOpcode;      // only used when opcode is OP_EXT
    int form;
    int width;          // bytes
    std::string summary;
};

// Both the CPU decoder and the assembler read this one table. With separate
// copies an ISA change could desync them and the assembler would emit code the
// CPU decodes as something else.
int getInstructionCount();
Instruction getInstruction(int tableIndex);
int findInstructionByName(const std::string& mnemonic);   // -1 if not found
int findInstructionByOpcode(int opcode, int subOpcode);   // -1 if not found

#endif
