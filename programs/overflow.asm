; What an 8-bit register does at its edges.
; 255 + 1 leaves 0 with C = 1 - the 9th bit is not stored anywhere, it is
; reported. 0 - 1 wraps to 255 with C = 1 as a borrow.

        MOVI R0, 255
        INC  R0
        STORE R0, 0xF4          ; expect 0
        JC   CARRIED
        MOVI R1, 0              ; not reached
        JMP  NEXT
CARRIED:
        MOVI R1, 1
NEXT:   STORE R1, 0xF5          ; expect 1

        MOVI R2, 0
        DEC  R2
        STORE R2, 0xF6          ; expect 255

        HALT
