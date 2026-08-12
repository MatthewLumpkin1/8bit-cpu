#ifndef REGISTERS_HPP
#define REGISTERS_HPP

#include <string>

// Four registers, because the encoding has 2-bit register fields and 2 bits
// select one of four.
//
// In hardware each register is eight D flip-flops sharing a clock and a write
// enable. Reading is combinational - the flip-flop outputs are always driving,
// and a multiplexer picks which register reaches the bus. Writing is
// sequential, so it waits for a clock edge. Two read paths and one write path
// is exactly what ADD Rd, Rs needs in a single cycle.
const int REGISTER_COUNT = 4;

class RegisterFile {
public:
    RegisterFile();

    unsigned char read(int index);
    void write(int index, unsigned char value);
    void reset();

    static std::string name(int index);

private:
    unsigned char registers[REGISTER_COUNT];
};

#endif
