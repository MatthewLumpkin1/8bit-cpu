// CPU tests: memory, the register file, every opcode, and control flow.
// Programs are written in assembly and assembled first, so these exercise the
// real encoding instead of hand-written bytes that could drift from it.
#include "testing.hpp"
#include "assembler.hpp"
#include "cpu.hpp"
#include "instruction.hpp"

#include <string>

static CPU runSource(const std::string& source, int maxCycles) {
    CPU cpu;
    AsmResult program = assemble(source);
    cpu.loadProgram(program.code, 0);
    cpu.run(maxCycles);
    return cpu;
}

static bool sourceThrows(const std::string& source, int maxCycles) {
    try {
        runSource(source, maxCycles);
        return false;
    } catch (...) {
        return true;
    }
}

static void testMemory() {
    std::cout << "Memory\n";
    Memory memory;

    memory.write(0x42, 0x35);
    checkEqual(memory.read(0x42), 0x35, "value read back from the address written");
    checkEqual(memory.read(0x43), 0, "an untouched cell reads as zero");

    for (int address = 0; address < MEMORY_SIZE; address++) {
        memory.write((unsigned char)address, (unsigned char)(255 - address));
    }
    bool allCorrect = true;
    for (int address = 0; address < MEMORY_SIZE; address++) {
        if (memory.read((unsigned char)address) != (unsigned char)(255 - address)) {
            allCorrect = false;
        }
    }
    check(allCorrect, "all 256 addresses store independently");

    bool rejected = false;
    try {
        std::vector<unsigned char> tooBig(300, 0);
        memory.loadProgram(tooBig, 0);
    } catch (...) {
        rejected = true;
    }
    check(rejected, "an oversized program is rejected");

    rejected = false;
    try {
        std::vector<unsigned char> small(10, 0);
        memory.loadProgram(small, 250);
    } catch (...) {
        rejected = true;
    }
    check(rejected, "a program loaded too near the top is rejected");

    memory.reset();
    memory.write(1, 1);
    memory.read(1);
    memory.read(1);
    checkEqual(memory.writeCount, 1, "write count is tracked");
    checkEqual(memory.readCount, 2, "read count is tracked");
}

static void testRegisterFile() {
    std::cout << "Register file\n";
    RegisterFile registers;

    for (int i = 0; i < REGISTER_COUNT; i++) {
        registers.write(i, (unsigned char)(i * 17));
    }
    bool allCorrect = true;
    for (int i = 0; i < REGISTER_COUNT; i++) {
        if (registers.read(i) != (unsigned char)(i * 17)) {
            allCorrect = false;
        }
    }
    check(allCorrect, "each register holds its own value");

    bool rejected = false;
    try {
        registers.read(4);
    } catch (...) {
        rejected = true;
    }
    check(rejected, "reading R4 is rejected");

    rejected = false;
    try {
        registers.write(-1, 0);
    } catch (...) {
        rejected = true;
    }
    check(rejected, "writing a negative index is rejected");
}

static void testDataMovement() {
    std::cout << "Data movement\n";

    CPU cpu = runSource("MOVI R0, 200\nMOVI R3, 7\nHALT\n", 100);
    checkEqual(cpu.registers.read(0), 200, "MOVI loads R0");
    checkEqual(cpu.registers.read(3), 7, "MOVI loads R3");

    // ADD leaves Z set, and the following MOV must not clear it, because MOV
    // never goes through the ALU.
    cpu = runSource("MOVI R0, 0\nMOVI R1, 0\nADD R0, R1\nMOVI R2, 9\nMOV R3, R2\nHALT\n", 100);
    checkEqual(cpu.registers.read(3), 9, "MOV copies R2 into R3");
    check(cpu.flags.zero, "MOV leaves the zero flag alone");

    cpu = runSource("MOVI R0, 123\nSTORE R0, 0x90\nLOAD R1, 0x90\nHALT\n", 100);
    checkEqual(cpu.memory.read(0x90), 123, "STORE writes to an absolute address");
    checkEqual(cpu.registers.read(1), 123, "LOAD reads it back");

    cpu = runSource("MOVI R0, 0xA0\nMOVI R1, 77\nSTORER R0, R1\n"
                    "MOVI R2, 0\nLOADR R2, R0\nHALT\n", 100);
    checkEqual(cpu.memory.read(0xA0), 77, "STORER writes through a pointer register");
    checkEqual(cpu.registers.read(2), 77, "LOADR reads through a pointer register");
}

static void testEveryOpcode() {
    std::cout << "Every opcode\n";

    CPU cpu = runSource(
        "MOVI R0, 20\nMOVI R1, 6\n"
        "ADD R0, R1\nSTORE R0, 0\n"
        "SUB R0, R1\nSTORE R0, 1\n"
        "AND R0, R1\nSTORE R0, 2\n"
        "MOVI R0, 20\nOR R0, R1\nSTORE R0, 3\n"
        "MOVI R0, 20\nXOR R0, R1\nSTORE R0, 4\n"
        "MOVI R0, 20\nNOT R0\nSTORE R0, 5\n"
        "MOVI R0, 20\nINC R0\nSTORE R0, 6\n"
        "DEC R0\nSTORE R0, 7\n"
        "SHL R0\nSTORE R0, 8\n"
        "SHR R0\nSTORE R0, 9\n"
        "NOP\nHALT\n", 1000);

    int expected[10] = {26, 20, 4, 22, 18, 235, 21, 20, 40, 20};
    bool allCorrect = true;
    for (int i = 0; i < 10; i++) {
        if (cpu.memory.read((unsigned char)i) != expected[i]) {
            allCorrect = false;
        }
    }
    check(allCorrect, "all ten arithmetic and logic opcodes give the expected results");

    cpu = runSource("MOVI R0, 5\nNOP\nNOP\nHALT\n", 100);
    checkEqual(cpu.registers.read(0), 5, "NOP changes no register");
    checkEqual(cpu.cycles, 4, "NOP still costs one instruction");

    cpu = runSource("MOVI R0, 1\nHALT\nMOVI R0, 99\n", 100);
    check(cpu.halted, "HALT stops the CPU");
    checkEqual(cpu.registers.read(0), 1, "the instruction after HALT never runs");

    CPU halted;
    AsmResult program = assemble("HALT\n");
    halted.loadProgram(program.code, 0);
    halted.run(100);
    int cyclesBefore = halted.cycles;
    halted.step();
    checkEqual(halted.cycles, cyclesBefore, "stepping a halted CPU does nothing");
}

static void testControlFlow() {
    std::cout << "Control flow\n";

    CPU cpu = runSource("JMP SKIP\nMOVI R0, 99\nSKIP: MOVI R0, 7\nHALT\n", 100);
    checkEqual(cpu.registers.read(0), 7, "JMP skips over the instruction between");

    cpu = runSource("MOVI R0, 4\nMOVI R1, 4\nCMP R0, R1\nJZ HIT\n"
                    "MOVI R2, 0\nJMP END\nHIT: MOVI R2, 1\nEND: HALT\n", 100);
    checkEqual(cpu.registers.read(2), 1, "JZ taken when the values are equal");

    cpu = runSource("MOVI R0, 4\nMOVI R1, 5\nCMP R0, R1\nJZ HIT\n"
                    "MOVI R2, 0\nJMP END\nHIT: MOVI R2, 1\nEND: HALT\n", 100);
    checkEqual(cpu.registers.read(2), 0, "JZ not taken when they differ");

    cpu = runSource("MOVI R0, 4\nMOVI R1, 5\nCMP R0, R1\nJNZ HIT\n"
                    "MOVI R2, 0\nJMP END\nHIT: MOVI R2, 1\nEND: HALT\n", 100);
    checkEqual(cpu.registers.read(2), 1, "JNZ taken when the values differ");

    cpu = runSource("MOVI R0, 4\nMOVI R1, 4\nCMP R0, R1\nJNZ HIT\n"
                    "MOVI R2, 0\nJMP END\nHIT: MOVI R2, 1\nEND: HALT\n", 100);
    checkEqual(cpu.registers.read(2), 0, "JNZ not taken when they are equal");

    cpu = runSource("MOVI R0, 3\nMOVI R1, 200\nCMP R0, R1\nJC HIT\n"
                    "MOVI R2, 0\nJMP END\nHIT: MOVI R2, 1\nEND: HALT\n", 100);
    checkEqual(cpu.registers.read(2), 1, "JC taken when the compare borrowed");

    cpu = runSource("MOVI R0, 200\nMOVI R1, 3\nCMP R0, R1\nJC HIT\n"
                    "MOVI R2, 0\nJMP END\nHIT: MOVI R2, 1\nEND: HALT\n", 100);
    checkEqual(cpu.registers.read(2), 0, "JC not taken when it did not borrow");

    cpu = runSource("MOVI R0, 0\nMOVI R1, 10\nLOOP: INC R0\nCMP R0, R1\nJNZ LOOP\nHALT\n", 1000);
    checkEqual(cpu.registers.read(0), 10, "a backward jump forms a working loop");

    check(sourceThrows("LOOP: JMP LOOP\n", 500), "an endless loop hits the cycle guard");
}

static void testWholePrograms() {
    std::cout << "Whole programs\n";

    CPU cpu = runSource("MOVI R0, 0\nMOVI R1, 0\nMOVI R2, 10\n"
                        "LOOP: INC R1\nADD R0, R1\nCMP R1, R2\nJNZ LOOP\nHALT\n", 1000);
    checkEqual(cpu.registers.read(0), 55, "sum of 1 through 10 is 55");

    cpu = runSource("MOVI R0, 37\nMOVI R1, 92\nCMP R0, R1\nJC BIG\n"
                    "MOV R2, R0\nJMP END\nBIG: MOV R2, R1\nEND: HALT\n", 100);
    checkEqual(cpu.registers.read(2), 92, "the larger of 37 and 92 is selected");

    cpu = runSource("MOVI R0, 0x80\nMOVI R1, 0\nMOVI R3, 5\n"
                    "FILL: ADD R1, R3\nSTORER R0, R1\nINC R0\nMOVI R2, 0x85\n"
                    "CMP R0, R2\nJNZ FILL\n"
                    "MOVI R0, 0x80\nMOVI R1, 0\n"
                    "SUM: LOADR R2, R0\nADD R1, R2\nINC R0\nMOVI R2, 0x85\n"
                    "CMP R0, R2\nJNZ SUM\nHALT\n", 1000);
    checkEqual(cpu.registers.read(1), 75, "an array written then summed gives 75");
    checkEqual(cpu.memory.read(0x82), 15, "the third array element is correct");
}

static void testDecoding() {
    std::cout << "Decoding and trace\n";

    CPU cpu;
    std::vector<unsigned char> reserved;
    reserved.push_back(0xE0);
    cpu.loadProgram(reserved, 0);
    bool threw = false;
    try {
        cpu.step();
    } catch (...) {
        threw = true;
    }
    check(threw, "the reserved opcode 0xE raises");

    CPU cpu2;
    std::vector<unsigned char> badExtended;
    badExtended.push_back(0xF0);
    badExtended.push_back(0xF0);
    cpu2.loadProgram(badExtended, 0);
    threw = false;
    try {
        cpu2.step();
    } catch (...) {
        threw = true;
    }
    check(threw, "an unassigned extended opcode raises");

    // The PC is 8 bits, so it cannot address past 0xFF - it wraps, like a
    // hardware counter.
    CPU cpu3;
    std::vector<unsigned char> nop;
    nop.push_back(0x00);
    cpu3.loadProgram(nop, 255);
    cpu3.step();
    checkEqual(cpu3.programCounter, 0, "the program counter wraps at the top of memory");

    CPU cpu4;
    AsmResult program = assemble("ADD R2, R1\nHALT\n");
    cpu4.loadProgram(program.code, 0);
    TraceRecord trace = cpu4.step();
    check(trace.disassembly == "ADD R2, R1", "the trace disassembles what it executed");
    check(trace.aluOperation == "ADD", "the trace reports the ALU operation");
    checkEqual(trace.pcBefore, 0, "the trace records the address it started at");

    CPU cpu5;
    program = assemble("STORE R0, 0x55\nHALT\n");
    cpu5.loadProgram(program.code, 0);
    trace = cpu5.step();
    check(trace.memoryAccess.find("WRITE") != std::string::npos,
          "the trace reports a memory write");
    check(trace.memoryAccess.find("85") != std::string::npos,
          "the trace reports the address written");
}

static void testVonNeumannAndReset() {
    std::cout << "Shared memory and reset\n";

    // There is one memory, so a STORE into the program region overwrites an
    // instruction. Here it lands on the second byte of HALT and the program runs
    // away instead of stopping. That is Von Neumann behaving normally, not a bug -
    // a Harvard machine with separate program and data memories would prevent it.
    check(sourceThrows("MOVI R0, 9\nSTORE R0, 5\nHALT\n", 500),
          "storing over the program's own HALT makes it run away");

    CPU cpu = runSource("MOVI R0, 9\nSTORE R0, 0x55\nHALT\n", 100);
    cpu.reset();
    checkEqual(cpu.registers.read(0), 0, "reset clears the registers");
    checkEqual(cpu.memory.read(0x55), 0, "reset clears memory");
    checkEqual(cpu.programCounter, 0, "reset returns the program counter to zero");
    checkEqual(cpu.cycles, 0, "reset clears the cycle count");
    check(!cpu.halted, "reset clears the halted flag");
}

int main() {
    testMemory();
    testRegisterFile();
    testDataMovement();
    testEveryOpcode();
    testControlFlow();
    testWholePrograms();
    testDecoding();
    testVonNeumannAndReset();
    return reportResults("CPU");
}
