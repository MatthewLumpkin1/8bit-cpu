# Instruction Set

23 instructions in two maps. The table below is generated from the same source
the CPU and assembler read (`include/instruction.hpp`); run `./cpu8 --isa` to
print the live version.

## Encoding

**Primary format — one byte:**

```
   7   6   5   4 | 3   2 | 1   0
 +---------------+-------+-------+
 |    opcode     |  dst  |  src  |
 +---------------+-------+-------+
```

Instructions taking an 8-bit immediate or address append a second byte.

**Opcode 0xF is an escape** into an extended map, whose extension byte is:

```
   7   6   5   4 | 3   2 | 1   0
 +---------------+-------+-------+
 |  sub-opcode   |  dst  |  src  |
 +---------------+-------+-------+
```

### Why an escape opcode

A 4-bit opcode with two 2-bit register fields uses all eight bits, which caps the
ISA at **16 instructions**. This ISA needs 23.

The options were:

| Option | Cost |
|---|---|
| Cut to 16 instructions | Loses register-indirect addressing and at least three ALU ops |
| Fixed 2-byte instruction word | Every instruction pays a byte, including `NOP` and `ADD` |
| 3-bit opcode, 5-bit operand field | Only 8 instructions in the primary map — worse |
| **Escape opcode into a second map** | One extra byte, only on instructions in the extended map |

The escape was chosen. It is the same mechanism as the Z80's `0xCB` prefix and
the x86 two-byte opcode map, and it costs a byte only where it is used.

**Single-operand instructions were put in the extended map deliberately.** They
need one register field, not two, so the extension byte has two spare bits — and
those spare bits are what make register-indirect `LOADR`/`STORER` fit without any
further encoding work. Had two-operand instructions been demoted instead, the
extension byte would have been full and indirect addressing would not exist.

Opcode `0xE` is reserved and unassigned. Executing it raises an illegal-opcode
error rather than doing something arbitrary.

## Primary map

| Mnemonic | Opcode | Bytes | Operands | Effect | Flags |
|---|---|---:|---|---|---|
| `NOP` | `0x0` | 1 | — | nothing | — |
| `MOV` | `0x1` | 1 | `Rd, Rs` | `Rd = Rs` | — |
| `MOVI` | `0x2` | 2 | `Rd, imm8` | `Rd = imm8` | — |
| `LOAD` | `0x3` | 2 | `Rd, [addr8]` | `Rd = mem[addr8]` | — |
| `STORE` | `0x4` | 2 | `Rd, [addr8]` | `mem[addr8] = Rd` | — |
| `ADD` | `0x5` | 1 | `Rd, Rs` | `Rd = Rd + Rs` | Z C N |
| `SUB` | `0x6` | 1 | `Rd, Rs` | `Rd = Rd − Rs` | Z C N |
| `AND` | `0x7` | 1 | `Rd, Rs` | `Rd = Rd & Rs` | Z N, C←0 |
| `OR` | `0x8` | 1 | `Rd, Rs` | `Rd = Rd \| Rs` | Z N, C←0 |
| `XOR` | `0x9` | 1 | `Rd, Rs` | `Rd = Rd ^ Rs` | Z N, C←0 |
| `CMP` | `0xA` | 1 | `Rd, Rs` | flags from `Rd − Rs`, no writeback | Z C N |
| `JMP` | `0xB` | 2 | `addr8` | `PC = addr8` | — |
| `JZ` | `0xC` | 2 | `addr8` | `PC = addr8` if `Z` | — |
| `JNZ` | `0xD` | 2 | `addr8` | `PC = addr8` if `!Z` | — |
| — | `0xE` | — | — | *reserved* | — |
| — | `0xF` | — | — | *escape* | — |

## Extended map (prefix `0xF`)

| Mnemonic | Sub | Bytes | Operands | Effect | Flags |
|---|---|---:|---|---|---|
| `NOT` | `0x0` | 2 | `Rd` | `Rd = ~Rd` | Z N, C←0 |
| `INC` | `0x1` | 2 | `Rd` | `Rd = Rd + 1` | Z C N |
| `DEC` | `0x2` | 2 | `Rd` | `Rd = Rd − 1` | Z C N |
| `SHL` | `0x3` | 2 | `Rd` | `Rd = Rd << 1` | Z C N |
| `SHR` | `0x4` | 2 | `Rd` | `Rd = Rd >> 1` (logical) | Z C N |
| `HALT` | `0x5` | 2 | — | stop | — |
| `JC` | `0x6` | 3 | `addr8` | `PC = addr8` if `C` | — |
| `LOADR` | `0x7` | 2 | `Rd, Rs` | `Rd = mem[Rs]` | — |
| `STORER` | `0x8` | 2 | `Rd, Rs` | `mem[Rd] = Rs` | — |

## Flags

| Flag | Set when |
|---|---|
| **Z** — zero | the 8-bit result is `0x00` |
| **C** — carry / borrow | `ADD`/`INC`: result exceeded 8 bits. `SUB`/`CMP`/`DEC`: the subtraction borrowed, i.e. `a < b`. `SHL`/`SHR`: the bit shifted out of the register. Logic operations clear it. |
| **N** — negative | bit 7 of the result is set (the sign bit under two's complement) |

### Carry is a borrow on subtraction

`SUB` and `CMP` set `C` when `a < b`. This is the 6502/x86 convention inverted:
those set carry to the *complement* of borrow, so on a 6502 `C = 1` after a
subtraction means *no* borrow. The convention here was chosen because it makes
`JC` read naturally after a `CMP`:

```asm
    CMP  R0, R1
    JC   less_than      ; taken when R0 < R1
```

Either convention works; the one that matters is that it is **documented and
consistent**, because a program written against one and run on the other branches
backwards everywhere.

### There is no overflow (V) flag

`N` reports bit 7 of the result, but nothing reports *signed* overflow. `127 + 1`
gives `128`, which sets `N` — correct as a bit pattern, and correct as `−128` in
two's complement, but the program has no way to learn that a signed addition
produced the wrong sign. A `V` flag (set when the operands share a sign that
differs from the result's) is the standard fix and is the first thing this ALU
should gain.

## Assembly syntax

```asm
; comments run from a semicolon to end of line

label:  INSTRUCTION operand, operand    ; a label may share a line or stand alone

        MOVI R0, 42          ; decimal
        MOVI R0, 0x2A        ; hex
        MOVI R0, 0b00101010  ; binary
        LOAD R0, [200]       ; brackets are optional, and purely for the reader
        LOAD R0, 200         ; identical
```

Mnemonics, register names and labels are case-insensitive. Labels may be used
before they are defined.
