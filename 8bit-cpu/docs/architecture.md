# Architecture

## Block diagram

```
                    ┌──────────────────────┐
                    │   Program Counter    │  8-bit
                    │        (PC)          │  holds the address of the
                    └──────────┬───────────┘  next instruction
                               │ address
              ┌────────────────┼─────────────────┐
              │                ▼                 │
              │     ┌────────────────────┐       │
              │     │       Memory       │       │
              │     │   256 × 8 bits     │◄──────┼──── data in  (STORE)
              │     │    0x00 - 0xFF     │───────┼───► data out (LOAD)
              │     └──────────┬─────────┘       │
              │                │ instruction     │
              │                ▼                 │
              │     ┌────────────────────┐       │
              │     │ Instruction Reg.   │  8-bit│
              │     │       (IR)         │       │
              │     └──────────┬─────────┘       │
              │                │                 │
              │                ▼                 │
              │     ┌────────────────────┐       │
              │     │   Control Unit     │       │
              │     │  opcode → control  │       │
              │     │      signals       │       │
              │     └───┬────────────┬───┘       │
              │         │            │           │
              │  reg select    ALU function      │
              │  write enable  select            │
              │         │            │           │
              │         ▼            ▼           │
              │  ┌─────────────┐  ┌──────────┐   │
              │  │  Register   │  │   ALU    │   │
              │  │    File     │─►│          │   │
              │  │ R0 R1 R2 R3 │─►│ 11 ops   │   │
              │  │  4 × 8 bits │◄─│          │   │
              │  └─────────────┘  └────┬─────┘   │
              │                        │ status  │
              │                        ▼         │
              │                 ┌────────────┐   │
              │                 │   FLAGS    │   │
              │                 │  Z  C  N   │   │
              │                 └──────┬─────┘   │
              │                        │         │
              └────────────────────────┴─────────┘
                     branch decision back to PC
```

Every arrow in that diagram, explained:

| Arrow | What travels on it | Why it exists |
|---|---|---|
| PC → Memory | an 8-bit address | fetch has to say *where* the next instruction is |
| Memory → IR | the instruction byte | the instruction must be held still while it is decoded, because the memory bus is needed again for operands |
| IR → Control Unit | opcode and register fields | the control unit turns the bit pattern into the signals that drive everything else |
| Control → Register File | register select, write enable | which register to read, which to write, and whether to write at all |
| Control → ALU | function select | which of the 11 operations to perform |
| Register File → ALU | two operands | a two-operand instruction needs both in the same cycle, so the file has two read ports |
| ALU → Register File | the result | writeback, for every ALU instruction except `CMP` |
| ALU → FLAGS | Z, C, N | status outputs latched at the end of the operation |
| FLAGS → PC | the branch decision | how `JZ`, `JNZ` and `JC` change control flow |
| Memory ↔ data | `LOAD`/`STORE` traffic | the same memory as instructions — see Von Neumann, below |
| PC → PC | increment, or a loaded branch target | sequential execution, and jumps |

## Specifications

| | |
|---|---|
| Data width | 8 bits |
| Address width | 8 bits |
| General-purpose registers | 4 (`R0`–`R3`) |
| Special registers | PC, IR, FLAGS |
| Flags | Z (zero), C (carry/borrow), N (negative) |
| Memory | 256 bytes, unified code and data |
| Instructions | 23 |
| Instruction length | 1, 2 or 3 bytes |
| ALU operations | 11 |
| Addressing modes | register, immediate, absolute, register-indirect |

### Why these numbers

**8-bit data.** One register holds `00000000`–`11111111`: 0–255 unsigned, or
−128–127 read as two's complement. Which of those two a value *is* depends
entirely on how the program interprets it — the hardware stores the same eight
bits either way, and only the flags differ in usefulness (`C` for unsigned
comparison, `N` for signed).

**256 bytes of memory.** Not a capacity decision. The address bus is 8 bits, so
2⁸ = 256 locations is everything it can reach. Adding memory would mean widening
the address bus, which would mean immediates and jump targets no longer fit in
one byte.

**Four registers.** The instruction encoding has room for two 2-bit register
fields, and 2 bits selects one of four. Eight registers would need 3-bit fields,
which the primary map cannot afford. This is a real cost: with only four
registers, `programs/memory.asm` has to reload a constant inside its loop because
there is nowhere to keep it.

## Fetch–decode–execute

```
FETCH     IR ← memory[PC];  PC ← PC + 1
          (if the opcode is the 0xF escape, fetch the extension byte too)

DECODE    opcode ← IR[7:4]      dst ← IR[3:2]      src ← IR[1:0]
          the control unit looks the opcode up and selects:
            - which ALU function, if any
            - whether the register file writes back
            - whether a further byte (immediate or address) is needed
            - whether the PC is loaded rather than incremented

EXECUTE   route operands and act:
            ADD/SUB/...  → ALU, writeback, latch flags
            MOV          → register-to-register, no ALU, no flag change
            LOAD/STORE   → memory read or write
            JMP/JZ/...   → load the PC, conditionally on a flag
            HALT         → stop the loop

repeat until HALT
```

The simulator retires one instruction per iteration. **Real hardware would not.**
A single-cycle-per-instruction machine has to stretch its clock period to cover
the slowest instruction; real 8-bit CPUs take several clock cycles per
instruction (the 6502 takes 2–7), and pipelined CPUs overlap the stages of
consecutive instructions. `cycles` in this simulator counts *instructions
retired*, not clock ticks, and the trace calls it a cycle only because that is
what the debugger display conventionally says.

## The register file in hardware

Each of the four registers is eight edge-triggered D flip-flops sharing a clock
and a write-enable line — one flip-flop per bit, each holding its value until a
clock edge and an asserted enable tell it to take a new one.

Reading is combinational: the flip-flop outputs are always driving, and a
multiplexer selected by the instruction's register field picks which register
reaches the bus. That is why a register read costs nothing in time while a write
must wait for a clock edge. Two read multiplexers and one write decoder is
exactly what `ADD Rd, Rs` needs: read two, write one, in a single cycle.

## Design decisions

### Von Neumann, not Harvard

Code and data share one 256-byte memory and one address space. The consequence
is real and is asserted in the test suite
(`code_and_data_share_one_address_space`): a `STORE` into the region holding the
program overwrites an instruction, and the program then runs away. A Harvard
architecture with separate instruction and data memories would prevent that, at
the cost of two address spaces, two buses, and no ability to load a program into
memory at run time.

### `CMP` is a separate instruction from `SUB`

`CMP` runs the subtraction through the ALU for its flags and discards the result.
Without it, every comparison would destroy one of its operands, so the loop in
`programs/sum.asm` would need to save and restore the counter around each test.
The cost is one opcode; the alternative costs two instructions per loop iteration.

### `MOV` does not set flags

It is a datapath transfer with no ALU involvement, so there is no status output
to latch. This is not a detail — it is what lets a program compute a condition,
shuffle registers, and *then* branch on the earlier result.

### No stack, no CALL/RET

There is no stack pointer and no subroutine mechanism. Every program here is a
single flat routine. Adding them means a stack pointer register, a region of
memory reserved for the stack, `PUSH`/`POP`, and `CALL`/`RET` pushing and popping
the return address — the natural next extension, and the reason `0xE` was left
unassigned.

## Limitations

- **No overflow flag.** Signed overflow is undetectable; see the ISA document.
- **No interrupts, no I/O.** The only way in or out is memory inspection after
  `HALT`.
- **One instruction per iteration.** No pipeline, no cycle-accurate timing, no
  memory latency.
- **No stack, no subroutines, no recursion.**
- **Logical shift only.** `SHR` zero-fills, so it cannot divide a signed value
  by two. No rotates, and no shift-through-carry.
- **8-bit immediates and jump targets only.** A consequence of the 256-byte
  address space, not an independent restriction.
- **No `MOVI`-style immediate for arithmetic.** `ADD R0, 5` does not exist; the
  constant must be loaded into a register first. The primary map is full.
- **The simulator models a datapath, not a circuit.** There is no gate-level
  logic, no timing, no propagation delay. It shows what the architecture
  *computes*, not what the hardware would *cost*.
