; Count R0 from 1 to 10.
; INC, CMP and a backward JNZ are all it takes to build a loop - the program
; counter does the rest.

        MOVI R0, 0
        MOVI R1, 10             ; limit

LOOP:   INC  R0
        CMP  R0, R1
        JNZ  LOOP

        STORE R0, 0xF0          ; expect 10
        HALT
