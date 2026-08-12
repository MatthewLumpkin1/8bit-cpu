; Sum the integers 1 through 10.
; Shows why CMP exists separately from SUB: the loop has to compare the counter
; against the limit without destroying it.

        MOVI R0, 0              ; accumulator
        MOVI R1, 0              ; counter
        MOVI R2, 10             ; limit

LOOP:   INC  R1
        ADD  R0, R1
        CMP  R1, R2
        JNZ  LOOP

        STORE R0, 0xF1          ; expect 55
        HALT
