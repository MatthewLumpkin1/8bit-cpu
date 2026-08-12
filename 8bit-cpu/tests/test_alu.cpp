// ALU tests: the arithmetic, and every boundary where an 8-bit result stops
// being the mathematically correct one.
#include "testing.hpp"
#include "alu.hpp"

static unsigned char runALU(int operation, unsigned char a, unsigned char b, Flags& flags) {
    flags = makeClearFlags();
    return aluExecute(operation, a, b, flags);
}

static void testAddition() {
    std::cout << "Addition\n";
    Flags flags;

    checkEqual(runALU(ALU_ADD, 5, 3, flags), 8, "5 + 3 = 8");
    check(!flags.carry && !flags.zero && !flags.negative, "5 + 3 sets no flags");

    // 255 + 1 needs 9 bits. The register keeps 8, and the carry flag is the only
    // record that the 9th ever existed.
    checkEqual(runALU(ALU_ADD, 255, 1, flags), 0, "255 + 1 wraps to 0");
    check(flags.carry, "255 + 1 sets carry");
    check(flags.zero, "255 + 1 sets zero");

    // No carry out of bit 7, but bit 7 is now set, so this reads as -128 in
    // two's complement. That is signed overflow, and this CPU has no V flag to
    // report it - a documented limitation, pinned down here so it stays deliberate.
    checkEqual(runALU(ALU_ADD, 127, 1, flags), 128, "127 + 1 = 128");
    check(!flags.carry, "127 + 1 does not set carry");
    check(flags.negative, "127 + 1 sets negative");
}

static void testSubtraction() {
    std::cout << "Subtraction and compare\n";
    Flags flags;

    checkEqual(runALU(ALU_SUB, 10, 4, flags), 6, "10 - 4 = 6");
    check(!flags.carry, "10 - 4 does not borrow");

    checkEqual(runALU(ALU_SUB, 0, 1, flags), 255, "0 - 1 wraps to 255");
    check(flags.carry, "0 - 1 borrows");
    check(flags.negative, "0 - 1 sets negative");

    runALU(ALU_SUB, 42, 42, flags);
    check(flags.zero && !flags.carry, "42 - 42 sets zero, no borrow");

    Flags subFlags;
    Flags cmpFlags;
    runALU(ALU_SUB, 7, 9, subFlags);
    runALU(ALU_CMP, 7, 9, cmpFlags);
    check(subFlags.zero == cmpFlags.zero && subFlags.carry == cmpFlags.carry
          && subFlags.negative == cmpFlags.negative, "CMP produces the same flags as SUB");

    // Carry after CMP is an unsigned less-than test, which is what JC relies on.
    runALU(ALU_CMP, 3, 200, flags);
    check(flags.carry, "CMP 3, 200 sets carry (3 < 200)");
    runALU(ALU_CMP, 200, 3, flags);
    check(!flags.carry, "CMP 200, 3 clears carry (200 > 3)");
    runALU(ALU_CMP, 50, 50, flags);
    check(!flags.carry, "CMP 50, 50 clears carry (equal is not less than)");
}

static void testLogic() {
    std::cout << "Logic\n";
    Flags flags;

    checkEqual(runALU(ALU_AND, 0xCC, 0xAA, flags), 0x88, "0xCC AND 0xAA = 0x88");
    checkEqual(runALU(ALU_OR, 0xCC, 0xAA, flags), 0xEE, "0xCC OR 0xAA = 0xEE");
    checkEqual(runALU(ALU_XOR, 0xCC, 0xAA, flags), 0x66, "0xCC XOR 0xAA = 0x66");
    checkEqual(runALU(ALU_NOT, 0xCC, 0, flags), 0x33, "NOT 0xCC = 0x33");

    runALU(ALU_XOR, 0x5A, 0x5A, flags);
    check(flags.zero, "a value XOR itself sets zero");

    // A logic operation must not leave a stale carry from an earlier add.
    flags = makeClearFlags();
    aluExecute(ALU_ADD, 255, 1, flags);
    check(flags.carry, "carry is set by the add");
    aluExecute(ALU_AND, 0xFF, 0xFF, flags);
    check(!flags.carry, "the following AND clears carry");
}

static void testIncrementDecrementShift() {
    std::cout << "Increment, decrement, shift\n";
    Flags flags;

    checkEqual(runALU(ALU_INC, 255, 0, flags), 0, "INC 255 wraps to 0");
    check(flags.carry && flags.zero, "INC 255 sets carry and zero");

    checkEqual(runALU(ALU_DEC, 0, 0, flags), 255, "DEC 0 wraps to 255");
    check(flags.carry, "DEC 0 borrows");

    checkEqual(runALU(ALU_DEC, 1, 0, flags), 0, "DEC 1 = 0");
    check(flags.zero && !flags.carry, "DEC 1 sets zero without borrowing");

    checkEqual(runALU(ALU_SHL, 0x40, 0, flags), 0x80, "SHL 0x40 = 0x80");
    check(!flags.carry && flags.negative, "SHL 0x40 sets negative, no carry");

    checkEqual(runALU(ALU_SHL, 0x81, 0, flags), 0x02, "SHL 0x81 = 0x02");
    check(flags.carry, "SHL 0x81 puts the lost top bit in carry");

    // Logical, not arithmetic: 0x80 >> 1 is 0x40, not 0xC0, so shifting a
    // negative value does not keep its sign.
    checkEqual(runALU(ALU_SHR, 0x80, 0, flags), 0x40, "SHR 0x80 = 0x40, zero filled");
    check(!flags.negative, "SHR 0x80 clears negative");

    checkEqual(runALU(ALU_SHR, 0x03, 0, flags), 0x01, "SHR 0x03 = 0x01");
    check(flags.carry, "SHR 0x03 puts the lost bottom bit in carry");
}

static void testFlagRules() {
    std::cout << "Flag rules\n";
    Flags flags;

    runALU(ALU_ADD, 0, 0, flags);
    check(flags.zero, "zero flag set on a zero result");
    runALU(ALU_ADD, 0, 1, flags);
    check(!flags.zero, "zero flag clear on a nonzero result");

    runALU(ALU_ADD, 0x7F, 0, flags);
    check(!flags.negative, "0x7F is not negative");
    runALU(ALU_ADD, 0x80, 0, flags);
    check(flags.negative, "0x80 is negative");
    runALU(ALU_ADD, 0xFF, 0, flags);
    check(flags.negative, "0xFF is negative");
}

int main() {
    testAddition();
    testSubtraction();
    testLogic();
    testIncrementDecrementShift();
    testFlagRules();
    return reportResults("ALU");
}
