#include "instruction.hpp"

static const Instruction INSTRUCTION_TABLE[] = {
    {"NOP",    OP_NOP,   0,           FORM_NONE,         1, "do nothing for one instruction"},
    {"MOV",    OP_MOV,   0,           FORM_REG_REG,      1, "copy Rs into Rd"},
    {"MOVI",   OP_MOVI,  0,           FORM_REG_IMM,      2, "load an 8-bit constant into Rd"},
    {"LOAD",   OP_LOAD,  0,           FORM_REG_ADDR,     2, "read memory at a fixed address into Rd"},
    {"STORE",  OP_STORE, 0,           FORM_REG_ADDR,     2, "write Rd to a fixed memory address"},
    {"ADD",    OP_ADD,   0,           FORM_REG_REG,      1, "Rd = Rd + Rs"},
    {"SUB",    OP_SUB,   0,           FORM_REG_REG,      1, "Rd = Rd - Rs"},
    {"AND",    OP_AND,   0,           FORM_REG_REG,      1, "Rd = Rd & Rs"},
    {"OR",     OP_OR,    0,           FORM_REG_REG,      1, "Rd = Rd | Rs"},
    {"XOR",    OP_XOR,   0,           FORM_REG_REG,      1, "Rd = Rd ^ Rs"},
    {"CMP",    OP_CMP,   0,           FORM_REG_REG,      1, "flags from Rd - Rs, no writeback"},
    {"JMP",    OP_JMP,   0,           FORM_ADDR,         2, "unconditional jump"},
    {"JZ",     OP_JZ,    0,           FORM_ADDR,         2, "jump if Z = 1"},
    {"JNZ",    OP_JNZ,   0,           FORM_ADDR,         2, "jump if Z = 0"},
    {"NOT",    OP_EXT,   SUB_NOT,     FORM_REG,          2, "Rd = ~Rd"},
    {"INC",    OP_EXT,   SUB_INC,     FORM_REG,          2, "Rd = Rd + 1"},
    {"DEC",    OP_EXT,   SUB_DEC,     FORM_REG,          2, "Rd = Rd - 1"},
    {"SHL",    OP_EXT,   SUB_SHL,     FORM_REG,          2, "Rd = Rd << 1, C = bit shifted out"},
    {"SHR",    OP_EXT,   SUB_SHR,     FORM_REG,          2, "Rd = Rd >> 1, C = bit shifted out"},
    {"HALT",   OP_EXT,   SUB_HALT,    FORM_NONE,         2, "stop execution"},
    {"JC",     OP_EXT,   SUB_JC,      FORM_ADDR,         3, "jump if C = 1"},
    {"LOADR",  OP_EXT,   SUB_LOADR,   FORM_REG_INDIRECT, 2, "Rd = memory[Rs]"},
    {"STORER", OP_EXT,   SUB_STORER,  FORM_REG_INDIRECT, 2, "memory[Rd] = Rs"},
};

static const int INSTRUCTION_COUNT = 23;

int getInstructionCount() {
    return INSTRUCTION_COUNT;
}

Instruction getInstruction(int tableIndex) {
    return INSTRUCTION_TABLE[tableIndex];
}

int findInstructionByName(const std::string& mnemonic) {
    for (int i = 0; i < INSTRUCTION_COUNT; i++) {
        if (INSTRUCTION_TABLE[i].mnemonic == mnemonic) {
            return i;
        }
    }
    return -1;
}

int findInstructionByOpcode(int opcode, int subOpcode) {
    for (int i = 0; i < INSTRUCTION_COUNT; i++) {
        if (INSTRUCTION_TABLE[i].opcode != opcode) {
            continue;
        }
        if (opcode == OP_EXT && INSTRUCTION_TABLE[i].subOpcode != subOpcode) {
            continue;
        }
        return i;
    }
    return -1;
}
