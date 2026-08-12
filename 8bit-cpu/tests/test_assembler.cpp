// Assembler tests: correct encoding, correct label resolution, and a readable
// error for every way a program can be wrong.
#include "testing.hpp"
#include "assembler.hpp"
#include "instruction.hpp"

#include <string>

static std::vector<unsigned char> assembleCode(const std::string& source) {
    return assemble(source).code;
}

// Returns the error message, or "" if the source assembled without complaint.
static std::string errorFor(const std::string& source) {
    try {
        assemble(source);
    } catch (const AsmError& error) {
        return error.message;
    }
    return "";
}

static bool mentions(const std::string& text, const std::string& phrase) {
    return text.find(phrase) != std::string::npos;
}

static bool sameBytes(const std::vector<unsigned char>& a,
                      const std::vector<unsigned char>& b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (int i = 0; i < (int)a.size(); i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

static void testEncoding() {
    std::cout << "Encoding\n";

    // ADD is opcode 0x5, R2 is dst (bits 3-2), R1 is src (bits 1-0):
    // 0101 10 01 = 0x59
    checkEqual(assembleCode("ADD R2, R1\n")[0], 0x59, "ADD R2, R1 packs into 0x59");

    std::vector<unsigned char> code = assembleCode("MOVI R3, 200\n");
    checkEqual((int)code.size(), 2, "the immediate form is two bytes");
    checkEqual(code[0], 0x2C, "the opcode byte is 0x2C");
    checkEqual(code[1], 200, "the second byte is the immediate");

    code = assembleCode("INC R1\n");
    checkEqual((int)code.size(), 2, "an extended instruction is two bytes");
    checkEqual(code[0], 0xF0, "it starts with the escape opcode");
    checkEqual(code[1], 0x14, "the extension byte holds sub-opcode 1 and R1");

    code = assembleCode("LOADR R2, R3\n");
    checkEqual(code[1], 0x7B, "LOADR uses both register fields of the extension byte");

    code = assembleCode("JC 12\n");
    checkEqual((int)code.size(), 3, "JC is three bytes");
    checkEqual(code[2], 12, "the third byte is the address");

    checkEqual(assembleCode("MOVI R0, 255\n")[1], 255, "decimal immediates work");
    checkEqual(assembleCode("MOVI R0, 0xFF\n")[1], 255, "hex immediates work");
    checkEqual(assembleCode("MOVI R0, 0b10101010\n")[1], 0xAA, "binary immediates work");

    check(sameBytes(assembleCode("LOAD R0, [200]\n"), assembleCode("load r0, 200\n")),
          "brackets and letter case make no difference");
    check(sameBytes(assembleCode("; comment\n\n  MOVI R0, 1   ; trailing\n\nHALT\n"),
                    assembleCode("MOVI R0, 1\nHALT\n")),
          "comments and blank lines are ignored");
}

static void testEveryInstructionAssembles() {
    std::cout << "ISA table coverage\n";
    // Catches a table entry that exists but has no working path through the
    // assembler.
    bool allCorrect = true;
    for (int i = 0; i < getInstructionCount(); i++) {
        Instruction instruction = getInstruction(i);
        std::string line = instruction.mnemonic;

        if (instruction.form == FORM_REG_REG)           line += " R0, R1";
        else if (instruction.form == FORM_REG_IMM)      line += " R0, 1";
        else if (instruction.form == FORM_REG_ADDR)     line += " R0, 8";
        else if (instruction.form == FORM_ADDR)         line += " 0";
        else if (instruction.form == FORM_REG)          line += " R0";
        else if (instruction.form == FORM_REG_INDIRECT) line += " R0, R1";

        if ((int)assembleCode(line + "\n").size() != instruction.width) {
            allCorrect = false;
        }
    }
    check(allCorrect, "all 23 instructions assemble to their declared width");
}

static void testLabels() {
    std::cout << "Labels\n";

    // MOVI is 2 bytes and INC is 2, so LOOP sits at address 2.
    std::vector<unsigned char> code = assembleCode("MOVI R0, 0\nLOOP: INC R0\nJMP LOOP\n");
    checkEqual(code[code.size() - 1], 2, "a backward label resolves");

    // The reason there are two passes: at the JMP, DONE is not known yet.
    code = assembleCode("JMP DONE\nMOVI R0, 1\nDONE: HALT\n");
    checkEqual(code[1], 4, "a forward label resolves");

    // 1-, 2- and 3-byte instructions before the label. If pass 1 assumed a fixed
    // instruction size, this address would come out wrong.
    AsmResult result = assemble("NOP\nMOVI R0, 1\nJC 0\nHERE: HALT\n");
    checkEqual(result.labels["HERE"], 6, "label addresses account for varying widths");

    check(sameBytes(assembleCode("START:\n MOVI R0, 1\n JMP START\n"),
                    assembleCode("START: MOVI R0, 1\n JMP START\n")),
          "a label can sit on its own line");
    check(sameBytes(assembleCode("loop: JMP LOOP\n"), assembleCode("LOOP: JMP loop\n")),
          "labels are case insensitive");
    checkEqual(assembleCode("JMP 42\n")[1], 42, "a plain number still works as a target");
}

static void testErrors() {
    std::cout << "Error messages\n";

    check(mentions(errorFor("ADD R9, R1\n"), "R9")
          && mentions(errorFor("ADD R9, R1\n"), "R0-R3"), "invalid register is reported");
    check(mentions(errorFor("BANANA R0\n"), "BANANA"), "unknown mnemonic is reported");
    check(mentions(errorFor("MOVI R0, 999\n"), "8 bits"), "an immediate over 255 is rejected");
    check(mentions(errorFor("MOVI R0, -1\n"), "8 bits"), "a negative immediate is rejected");
    check(mentions(errorFor("JMP DOES_NOT_EXIST\n"), "undefined label"),
          "an undefined label is reported");
    check(mentions(errorFor("ADD R0\n"), "expects 2"), "too few operands is reported");
    check(mentions(errorFor("HALT R0\n"), "expects 0"), "operands on HALT are reported");
    check(mentions(errorFor("MOVI R0, 1, 2\n"), "expects 2"), "too many operands is reported");
    check(mentions(errorFor("A: NOP\nA: NOP\n"), "duplicate"),
          "a duplicate label is reported");
    check(mentions(errorFor("ADD: NOP\n"), "mnemonic"),
          "a mnemonic used as a label is reported");
    check(mentions(errorFor("MOVI R0, banana\n"), "not a number"),
          "a non-numeric immediate is reported");

    std::string tooLong;
    for (int i = 0; i < 200; i++) {
        tooLong += "MOVI R0, 1\n";
    }
    check(mentions(errorFor(tooLong), "address space"),
          "a program larger than memory is rejected");

    int reportedLine = 0;
    try {
        assemble("NOP\n; comment\n\nADD R7, R0\n");
    } catch (const AsmError& error) {
        reportedLine = error.line;
    }
    checkEqual(reportedLine, 4, "the line number survives comments and blank lines");

    check(errorFor("MOVI R0, 1\nLOOP: INC R0\nJNZ LOOP\nHALT\n").empty(),
          "a valid program produces no error");
}

static void testListing() {
    std::cout << "Listing\n";
    AsmResult result = assemble("MOVI R0, 1\nINC R0\nHALT\n");
    checkEqual((int)result.listing.size(), 3, "one listing line per instruction");
    check(mentions(result.listing[0], "0000"), "the first instruction is at 0000");
    check(mentions(result.listing[1], "0002"), "the second instruction is at 0002");
}

int main() {
    testEncoding();
    testEveryInstructionAssembles();
    testLabels();
    testErrors();
    testListing();
    return reportResults("Assembler");
}
