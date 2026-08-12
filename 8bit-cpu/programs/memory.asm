; Write an array, then walk it back with a pointer.
;
; STORE takes an address fixed at assembly time, so it cannot walk anything.
; STORER/LOADR take the address from a register, which can change as the loop
; runs. Without them this would need self-modifying code or one unrolled
; instruction per element.
;
; Writes 5, 10, 15, 20, 25 to 0x80..0x84, then sums them back.

        MOVI R0, 0x80           ; pointer
        MOVI R1, 0              ; value
        MOVI R3, 5              ; step

FILL:   ADD  R1, R3
        STORER R0, R1
        INC  R0
        MOVI R2, 0x85           ; reloaded each pass - only four registers
        CMP  R0, R2
        JNZ  FILL

        MOVI R0, 0x80
        MOVI R1, 0              ; accumulator

SUMLOOP:
        LOADR R2, R0
        ADD  R1, R2
        INC  R0
        MOVI R2, 0x85
        CMP  R0, R2
        JNZ  SUMLOOP

        STORE R1, 0xF3          ; expect 75
        HALT
