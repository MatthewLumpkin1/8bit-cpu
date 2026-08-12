# Plain-make alternative to CMake, for anyone who would rather not install it.
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -Wpedantic -O2 -Iinclude
CORE     := src/alu.cpp src/memory.cpp src/registers.cpp src/instruction.cpp src/cpu.cpp src/assembler.cpp
SUITES   := alu cpu assembler

.PHONY: all test clean demo
all: cpu8

cpu8: $(CORE) src/main.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@

test: $(SUITES:%=build/test_%)
	@for t in $^; do ./$$t || exit 1; done

build/test_%: tests/test_%.cpp $(CORE)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -Itests $^ -o $@

demo: cpu8
	@for p in programs/*.asm; do echo "--- $$p"; ./cpu8 $$p | tail -3; done

clean:
	rm -rf cpu8 build
