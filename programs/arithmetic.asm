; Every ALU operation once, results left in memory at 0xE0 onward.
; Inspect with:  ./cpu8 programs/arithmetic.asm --dump 256

        MOVI R0, 12
        MOVI R1, 5

        ADD  R0, R1             ; 17
        STORE R0, 0xE0

        MOVI R0, 12
        SUB  R0, R1             ; 7
        STORE R0, 0xE1

        MOVI R0, 0b11001100
        MOVI R1, 0b10101010

        MOV  R2, R0
        AND  R2, R1             ; 0b10001000
        STORE R2, 0xE2

        MOV  R2, R0
        OR   R2, R1             ; 0b11101110
        STORE R2, 0xE3

        MOV  R2, R0
        XOR  R2, R1             ; 0b01100110
        STORE R2, 0xE4

        MOV  R2, R0
        NOT  R2                 ; 0b00110011
        STORE R2, 0xE5

        MOVI R3, 6
        SHL  R3                 ; 12
        STORE R3, 0xE6
        SHR  R3
        SHR  R3                 ; 3
        STORE R3, 0xE7

        HALT
