# 8-Bit CPU Simulator and Assembler

A custom 8-bit processor architecture, a cycle-stepped simulator for it in C++17,
and a two-pass assembler that turns its assembly language into machine code the
simulator executes.

```
   Assembly source  ──►  Assembler  ──►  Machine code  ──►  Memory
                         (2 passes)                            │
                                                               ▼
                                                    Fetch ─► Decode ─► Execute
                                                               │
                                          Registers / ALU / Memory / Flags
```

Nothing here executes strings. `ADD R2, R1` is assembled to the single byte
`0x59`, and the CPU decodes that byte into an opcode and two register selectors
the way hardware would.

**23 instructions · 4 registers · 256 bytes · 3 flags · 4 addressing modes ·
134 tests**

---

## Quick start

```bash
make && make test          # or: cmake -S . -B build && cmake --build build && ctest --test-dir build

./cpu8 --isa                                   # print the instruction set
./cpu8 programs/sum.asm                        # run
./cpu8 programs/comparison.asm --listing       # show address / bytes / source
./cpu8 programs/counter.asm --trace            # full CPU state after each instruction
./cpu8 programs/counter.asm --step             # pause between instructions
./cpu8 programs/memory.asm --dump 256          # hex-dump memory after HALT
```

## What it looks like

**Assembling** — `./cpu8 programs/comparison.asm --listing`

```
assembled 16 bytes, labels: DONE=12 R1_BIGGER=11

LISTING
0000:  20 25           MOVI R0, 37
0002:  24 5C           MOVI R1, 92
0004:  A1              CMP  R0, R1         ; C = 1 if R0 < R1
0005:  F0 60 0B        JC   R1_BIGGER
0008:  18              MOV  R2, R0         ; fall through: R0 >= R1
0009:  B0 0C           JMP  DONE
000B:  19              MOV  R2, R1
000C:  48 F2           DONE:   STORE R2, 0xF2
000E:  F0 50           HALT
```

Three instruction widths are visible in that listing: `A1` is one byte, `20 25`
is two, `F0 60 0B` is three. The forward reference to `R1_BIGGER` on line 4
resolves to `0B` — an address the assembler did not know when it reached that
line, which is why there are two passes.

**Executing** — `./cpu8 program.asm --trace`

```
--------------------------------
Cycle: 3
PC:    0x04  ->  0x05
IR:    0x51  (01010001)

Instruction:
ADD R0, R1

REGISTERS
R0: 00101100   44  0x2C
R1: 01100100  100  0x64
R2: 00000000    0  0x00
R3: 00000000    0  0x00

FLAGS
Z: 0   C: 1   N: 0

ALU:           ADD
Memory Access: -
--------------------------------
```

That trace is `200 + 100`. The register holds 44, and `C = 1` is the only record
that the real answer was 300 — the ninth bit does not exist in an 8-bit register,
so the carry flag is where it goes.

---

## Architecture

```
       ┌──────────────┐
       │      PC      │──── address ────┐
       └──────┬───────┘                 │
              │                         ▼
              │              ┌────────────────────┐
              │              │  Memory 256 × 8    │◄──── STORE
              │              └──────────┬─────────┘────► LOAD
              │                         │ instruction
              │                         ▼
              │              ┌────────────────────┐
              │              │        IR          │
              │              └──────────┬─────────┘
              │                         ▼
              │              ┌────────────────────┐
              │              │   Control Unit     │
              │              └───┬────────────┬───┘
              │                  ▼            ▼
              │        ┌─────────────┐   ┌──────────┐
              │        │  Registers  │──►│   ALU    │
              │        │ R0 R1 R2 R3 │◄──│  11 ops  │
              │        └─────────────┘   └────┬─────┘
              │                               ▼
              │                        ┌────────────┐
              └──── branch decision ───│ FLAGS Z C N│
                                       └────────────┘
```

Every arrow is explained in **[`docs/architecture.md`](docs/architecture.md)**,
along with the hardware each block corresponds to.

| | |
|---|---|
| Data width | 8 bits |
| Address width | 8 bits → 256 bytes, unified code and data |
| Registers | `R0`–`R3`, plus PC, IR, FLAGS |
| Flags | Z (zero), C (carry/borrow), N (negative) |
| Instructions | 23, in 1–3 bytes |
| Addressing modes | register, immediate, absolute, register-indirect |

The counts are not arbitrary. The address bus is 8 bits, so 256 bytes is
everything it can reach — not a capacity choice. The encoding has two 2-bit
register fields, and 2 bits selects one of four registers — which is why there
are four, and why `programs/memory.asm` has to reload a constant inside its loop.

---

## Instruction encoding

```
   7   6   5   4 | 3   2 | 1   0
 +---------------+-------+-------+
 |    opcode     |  dst  |  src  |
 +---------------+-------+-------+
```

`ADD R2, R1` → opcode `0101`, dst `10`, src `01` → `0x59`.

### The constraint that shaped the ISA

A 4-bit opcode plus two 2-bit register fields is exactly eight bits, which caps
the instruction set at **16**. This one needs 23.

| Option | Cost |
|---|---|
| Cut to 16 instructions | Lose indirect addressing and three ALU operations |
| Fixed 2-byte instruction word | Every instruction pays a byte, including `NOP` |
| 3-bit opcode | Only 8 primary instructions — worse |
| **Escape opcode into a second map** | One extra byte, only where it is used |

Opcode `0xF` became an escape into an extended map, the same mechanism as the
Z80's `0xCB` prefix. Single-operand instructions went into that map deliberately:
they need one register field, not two, so the extension byte has **two spare
bits** — and those are exactly what make register-indirect `LOADR`/`STORER` fit
with no further encoding work. Demoting two-operand instructions instead would
have filled the extension byte and left no room for indirect addressing.

Opcode `0xE` is reserved. Executing it raises an illegal-opcode error rather than
doing something arbitrary.

Full table, flag semantics and syntax:
**[`docs/instruction_set.md`](docs/instruction_set.md)**.

### Flags worth being specific about

`SUB` and `CMP` set carry when `a < b` — carry as a **borrow**, which is the
6502/x86 convention inverted. That makes `JC` read naturally after `CMP` as an
unsigned less-than. Either convention works; what matters is that a program
written against one and run on the other branches backwards everywhere, so it is
documented and tested.

There is **no overflow flag**. `127 + 1` yields `128` and sets `N` — correct as a
bit pattern, and correct as −128 in two's complement, but nothing tells the
program that a *signed* addition produced the wrong sign. A `V` flag is the first
thing this ALU should gain.

---

## The assembler

Two passes:

**Pass 1** walks the source, records the address of every label, and sizes every
instruction.
**Pass 2** emits machine code, resolving label references against the table pass 1
built.

The second pass is not optional, and the reason is forward references: at
`JMP done`, the address of `done` is not yet known. Sizing in pass 1 is what
makes the addresses right — instructions here are 1, 2 or 3 bytes, so the
assembler cannot assume a fixed stride. `test_assembler.cpp` pins that down with
a program containing all three widths before a label.

The assembler and the CPU decoder read the **same ISA table**
(`include/instruction.hpp`). With separate copies, an ISA change would
desynchronise them silently and the assembler would emit code the CPU decodes as
something else. Pass 2 also re-checks each emitted instruction against the width
pass 1 assumed, because if they ever disagreed every label after that point would
be wrong.

### Error handling

Bad input gets a line number and a specific message, not a crash:

```
$ ./cpu8 bad.asm
bad.asm:1: error: invalid register 'R9' (valid: R0-R3)
bad.asm:2: error: unknown instruction 'BANANA'
bad.asm:3: error: value 999 does not fit in 8 bits (0-255)
bad.asm:4: error: undefined label 'DOES_NOT_EXIST'
bad.asm:5: error: ADD expects 2 operand(s), got 1
bad.asm:6: error: duplicate label 'LOOP' (first defined at address 4)
bad.asm:7: error: 'ADD' is an instruction mnemonic and cannot be a label
bad.asm:8: error: program exceeds the 256-byte address space
```

---

## Demonstration programs

| Program | Demonstrates | Result |
|---|---|---|
| [`arithmetic.asm`](programs/arithmetic.asm) | every ALU operation once | 8 results at `0xE0`–`0xE7` |
| [`counter.asm`](programs/counter.asm) | the loop primitive: `INC`, `CMP`, backward `JNZ` | `10` at `0xF0` |
| [`sum.asm`](programs/sum.asm) | accumulator pattern; why `CMP` is not `SUB` | `55` at `0xF1` |
| [`comparison.asm`](programs/comparison.asm) | branching on carry, not zero | `92` at `0xF2` |
| [`memory.asm`](programs/memory.asm) | address vs data; register-indirect array walk | `75` at `0xF3` |
| [`overflow.asm`](programs/overflow.asm) | what an 8-bit register does at its edges | `0`, `1`, `255` at `0xF4`–`0xF6` |

`make demo` runs all six.

The one worth reading is `memory.asm`. It writes an array and walks it back with
a pointer, which is only possible because `STORER`/`LOADR` take their address
from a register at run time — `STORE` fixes its address at assembly time, so
without indirect addressing the same loop would need self-modifying code or one
unrolled instruction per element.

---

## Testing

`make test` — **134 tests across three suites, all passing.** No external test
dependency: `tests/testing.hpp` is a `check()` function that prints PASS/FAIL and
counts failures, and each test is an ordinary function.

**ALU (44).** Every operation, and every boundary where an 8-bit result stops
being the mathematically correct one: `255 + 1` (carry, zero), `127 + 1`
(negative *without* carry — the signed-overflow case the ISA cannot report),
`0 − 1` (borrow, wrap), `DEC` from zero, the bit shifted out by `SHL`/`SHR`, and
that a logic operation clears a carry left behind by an earlier add.

**CPU (51).** Memory across the whole address space and its rejection of an
oversized image; the register file including invalid indices; every opcode
executed and checked; each conditional branch tested in both the taken and
not-taken direction; loops; an infinite loop caught by the cycle guard; three
whole programs verified against their expected results; reserved and unassigned
opcodes raising; the PC wrapping at `0xFF`; and the trace reporting what actually
happened.

**Assembler (39).** Exact encodings byte by byte; every entry in the ISA table
assembling to its declared width (which catches a table entry with no working
parser path); backward labels, forward labels, labels on their own line,
case-insensitivity; and a test for each error message above, including that the
line number survives comments and blank lines.

One test is worth calling out: the one asserting that a program storing into its
own instruction bytes runs away instead of halting. That is not a bug being papered over — it is Von Neumann architecture,
found by a test that was originally written to check something else, and the
alternative (Harvard, with separate program and data memories) is a design choice
this CPU deliberately did not make.

---

## Layout

```
8bit-cpu/
├── README.md
├── CMakeLists.txt          CMake build with ctest
├── Makefile                plain-make alternative
├── include/
│   ├── instruction.hpp     ISA declarations — read by BOTH the CPU and assembler
│   ├── alu.hpp             combinational ALU, 11 operations
│   ├── memory.hpp          256 bytes of RAM
│   ├── registers.hpp       4 × 8-bit register file
│   ├── cpu.hpp             PC, IR, flags, fetch-decode-execute
│   └── assembler.hpp
├── src/                    instruction, alu, memory, registers, cpu, assembler, main
├── tests/                  testing.hpp + test_alu (44), test_cpu (51), test_assembler (39)
├── programs/               six demonstration programs
└── docs/
    ├── architecture.md     block diagram, every arrow explained, design decisions
    └── instruction_set.md  full ISA, encoding rationale, flag semantics
```

## Limitations

- **No overflow flag** — signed overflow is undetectable.
- **No stack, no `CALL`/`RET`, no subroutines** — every program is one flat
  routine. This is the reason opcode `0xE` was left unassigned.
- **No interrupts and no I/O** — the only way to observe a result is to dump
  memory after `HALT`.
- **One instruction per iteration** — no pipeline, no cycle-accurate timing. Real
  8-bit CPUs take several clock cycles per instruction; `cycles` here counts
  instructions retired.
- **Logical shift only** — `SHR` zero-fills, so it cannot halve a signed value.
  No rotates, no shift-through-carry.
- **No immediate arithmetic** — `ADD R0, 5` does not exist; constants must go
  through a register first. The primary opcode map is full.
- **Four registers is genuinely tight** — visible in `memory.asm`, which reloads
  a loop constant every iteration for lack of anywhere to keep it.
- **A datapath model, not a circuit** — no gates, no propagation delay, no
  fan-out. It shows what the architecture computes, not what the hardware costs.

## Future work

In the order that would actually make it better:

1. **Overflow flag** — one line in the ALU, and it closes the signed-arithmetic
   hole.
2. **Stack pointer, `PUSH`/`POP`, `CALL`/`RET`** — the difference between a
   machine that runs programs and one that runs *composable* programs. Opcode
   `0xE` is reserved for it.
3. **Immediate forms of the arithmetic instructions**, via a second escape map.
4. **A wider address space** — 16-bit addressing, which pulls in 2-byte
   immediates and a segmented or paged memory model.
5. **Memory-mapped I/O** — a couple of reserved addresses acting as ports, so a
   program can do something observable before it halts.
6. **Interrupts** — a vector table, and saving PC and flags on entry.
7. **Cycle-accurate timing** — a per-instruction cycle cost, moving `cycles` from
   instructions retired to clock ticks.
8. **Verilog implementation** — synthesise the same ISA onto an FPGA and check
   the two agree instruction for instruction. The clean split between `alu.cpp`,
   `registers.hpp` and the control logic in `cpu.cpp` was structured with this in
   mind.
