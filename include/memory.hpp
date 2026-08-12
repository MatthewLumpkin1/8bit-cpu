#ifndef MEMORY_HPP
#define MEMORY_HPP

#include <string>
#include <vector>

// 256 bytes of RAM. That is not a capacity choice - the address bus is 8 bits,
// so 2^8 = 256 locations is everything it can reach.
const int MEMORY_SIZE = 256;

class Memory {
public:
    Memory();

    unsigned char read(unsigned char address);
    void write(unsigned char address, unsigned char value);

    // The only path where a program could run off the end of memory, so this is
    // the only one that needs a range check.
    void loadProgram(const std::vector<unsigned char>& program, unsigned char startAddress);

    void reset();
    std::string hexDump(int firstAddress, int count);

    int readCount;
    int writeCount;

private:
    unsigned char cells[MEMORY_SIZE];
};

#endif
